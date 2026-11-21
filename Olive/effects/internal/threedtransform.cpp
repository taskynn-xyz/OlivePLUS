#include "threedtransform.h"

#include <QtMath>

#include "timeline/clip.h"
#include "timeline/sequence.h"
#include "global/math.h"
#include "rendering/renderfunctions.h"
#include "cameraeffect.h"

namespace {

// Must match MAX_SAMPLES in motionblur.frag
const int kMaxBlurSamples = 64;

}

ThreeDTransformEffect::ThreeDTransformEffect(Clip* c, const EffectMeta* em) : Effect(c, em) {
  SetFlags(Effect::CoordsFlag);

  // Shader used for the motion blur pass (shared with the default Transform and Shake effects).
  // The ShaderFlag itself is only set on frames that need the pass, see UpdateMotionBlur().
  vertPath = "motionblur/motionblur.vert";
  fragPath = "motionblur/motionblur.frag";

  EffectRow* position_row = new EffectRow(this, tr("Position"));

  position_x = new DoubleField(position_row, "posx");
  position_y = new DoubleField(position_row, "posy");
  position_z = new DoubleField(position_row, "posz");

  EffectRow* rotation_row = new EffectRow(this, tr("Rotation"));

  rotation_x = new DoubleField(rotation_row, "rotx");
  rotation_y = new DoubleField(rotation_row, "roty");
  rotation_z = new DoubleField(rotation_row, "rotz");

  EffectRow* scale_row = new EffectRow(this, tr("Scale"));
  scale_field = new DoubleField(scale_row, "scale");
  scale_field->SetMinimum(0);

  EffectRow* distance_row = new EffectRow(this, tr("Perspective"));
  distance_field = new DoubleField(distance_row, "distance");
  distance_field->SetMinimum(1);
  // camera distance in pixels: smaller values give a stronger, more dramatic perspective; larger
  // values look almost orthographic

  EffectRow* anchor_row = new EffectRow(this, tr("Anchor Point"));
  anchor_x = new DoubleField(anchor_row, "anchorx");
  anchor_y = new DoubleField(anchor_row, "anchory");

  EffectRow* opacity_row = new EffectRow(this, tr("Opacity"));
  opacity_field = new DoubleField(opacity_row, "opacity");
  opacity_field->SetMinimum(0);
  opacity_field->SetMaximum(100);

  // Motion blur rows. These MUST stay after the existing rows: saved projects match rows by position.
  EffectRow* shutter_angle_row = new EffectRow(this, tr("Shutter Angle"));
  shutter_angle = new DoubleField(shutter_angle_row, "shutterangle");
  shutter_angle->SetMinimum(0);
  shutter_angle->SetMaximum(720);

  EffectRow* shutter_phase_row = new EffectRow(this, tr("Shutter Phase"));
  shutter_phase = new DoubleField(shutter_phase_row, "shutterphase");
  shutter_phase->SetMinimum(-360);
  shutter_phase->SetMaximum(360);

  EffectRow* shutter_samples_row = new EffectRow(this, tr("Blur Samples"));
  shutter_samples = new DoubleField(shutter_samples_row, "shuttersamples");
  shutter_samples->SetMinimum(1);
  shutter_samples->SetMaximum(kMaxBlurSamples);

  // defaults: centred on the sequence, facing the camera
  position_z->SetDefault(0);
  rotation_x->SetDefault(0);
  rotation_y->SetDefault(0);
  rotation_z->SetDefault(0);
  scale_field->SetDefault(100);
  distance_field->SetDefault(2000);
  anchor_x->SetDefault(0);
  anchor_y->SetDefault(0);
  opacity_field->SetDefault(100);
  shutter_angle->SetDefault(180); // 0 disables motion blur
  shutter_phase->SetDefault(0);   // 0 = exposure centred on the frame
  shutter_samples->SetDefault(16);

  refresh();
}

void ThreeDTransformEffect::refresh() {
  if (parent_clip != nullptr && parent_clip->sequence != nullptr) {
    position_x->SetDefault(parent_clip->sequence->width / 2);
    position_y->SetDefault(parent_clip->sequence->height / 2);
  }
}

void ThreeDTransformEffect::Project(double timecode, double media_w, double media_h,
                                    double vertex_x[4], double vertex_y[4],
                                    double perspective[4]) {
  Clip* c = parent_clip;

  const double anchor_x_value = anchor_x->GetDoubleAt(timecode);
  const double anchor_y_value = anchor_y->GetDoubleAt(timecode);
  const double scale_value = scale_field->GetDoubleAt(timecode) * 0.01;

  // Position is in sequence pixels with the sequence centre as the origin, exactly like the default
  // Transform effect, so the two effects can be swapped without the clip jumping.
  double translate_x = 0;
  double translate_y = 0;
  if (c->sequence != nullptr) {
    translate_x = position_x->GetDoubleAt(timecode) - (c->sequence->width / 2.0);
    translate_y = position_y->GetDoubleAt(timecode) - (c->sequence->height / 2.0);
  }
  const double translate_z = position_z->GetDoubleAt(timecode);

  const double half_w = media_w / 2.0;
  const double half_h = media_h / 2.0;

  // Parenting happens here, in scene space. A 3D null contributes its real 3D transform, so its
  // child is actually rotated, moved and scaled in depth instead of being given a flat matrix.
  const double playhead = c->sequence != nullptr
      ? double(c->timeline_in(true)) - double(c->clip_in(true))
        + (timecode * c->sequence->frame_rate)
      : 0.0;

  double parent_matrix[16];
  bool used_3d_parent = false;
  const bool has_parent = build_parent_matrix(c, playhead, parent_matrix, true, &used_3d_parent);

  // This clip's own transform, as a matrix so the parent chain can be composed onto it. With no 3D
  // parent the order is translate, Y/X/Z rotation, scale, then the anchor offset, so rotation and
  // scale happen around the layer's own centre (anchor-relative, as before).
  //
  // Under a 3D null the rotation and scale are applied *before* the layer's position instead: the
  // null's centre is the origin of this parent space, so the layer now turns around the null rather
  // than around itself. That is what makes a group of parented layers orbit and scale as one object
  // instead of every layer spinning in place.
  double local[16];
  mat4_identity(local);
  if (used_3d_parent) {
    mat4_rotate_y(local, rotation_y->GetDoubleAt(timecode));
    mat4_rotate_x(local, rotation_x->GetDoubleAt(timecode));
    mat4_rotate_z(local, rotation_z->GetDoubleAt(timecode));
    mat4_scale(local, scale_value, scale_value);
    mat4_translate(local, translate_x, translate_y, translate_z);
    mat4_translate(local, -anchor_x_value, -anchor_y_value, 0);
  } else {
    mat4_translate(local, translate_x, translate_y, translate_z);
    mat4_rotate_y(local, rotation_y->GetDoubleAt(timecode));
    mat4_rotate_x(local, rotation_x->GetDoubleAt(timecode));
    mat4_rotate_z(local, rotation_z->GetDoubleAt(timecode));
    mat4_scale(local, scale_value, scale_value);
    mat4_translate(local, -anchor_x_value, -anchor_y_value, 0);
  }

  // Where the layer actually sits normally comes from its flat Transform effect, which the compositor
  // applies through the modelview *after* the 3D projection. That offset would then never take part in
  // the parent's rotation, so rotating a null would spin the layer around its own centre and only
  // afterwards move it in screen space. Folding the flat matrix into the scene matrix places the layer
  // before the parent chain is applied, so the null's own centre is the pivot it turns around.
  double placement[16];
  if (build_clip_flat_matrix(c, timecode, placement)) {
    double composed[16];
    mat4_multiply(placement, local, composed);
    for (int i = 0; i < 16; i++) {
      local[i] = composed[i];
    }
  }

  double scene[16];
  if (has_parent) {
    mat4_multiply(parent_matrix, local, scene);
  } else {
    for (int i = 0; i < 16; i++) {
      scene[i] = local[i];
    }
  }

  // A camera layer in the same sequence replaces the effect's own Perspective distance. Clips
  // without a 3D Transform never get here, so a camera only acts on 3D layers.
  Clip* camera_clip = c->sequence != nullptr ? camera::ActiveCamera(c->sequence, qRound(playhead))
                                             : nullptr;
  CameraEffect* camera = camera::EffectOf(camera_clip);
  double camera_timecode = 0.0;
  if (camera != nullptr && camera_clip->sequence != nullptr) {
    camera_timecode = (qMax(0.0, playhead - double(camera_clip->timeline_in(true)))
                       + double(camera_clip->clip_in(true))) / camera_clip->sequence->frame_rate;
  }

  const double own_camera_distance = qMax(1.0, distance_field->GetDoubleAt(timecode));

  // Corner order must match the order compose_sequence() draws them in (clockwise from top left).
  const double corner_x[4] = { -half_w, half_w, half_w, -half_w };
  const double corner_y[4] = { -half_h, -half_h, half_h, half_h };

  for (int i = 0; i < 4; i++) {
    double x, y, z;
    mat4_transform_point_3d(scene, corner_x[i], corner_y[i], 0.0, &x, &y, &z);

    if (camera != nullptr) {
      camera->ProjectPoint(camera_timecode, x, y, z,
                           &vertex_x[i], &vertex_y[i], &perspective[i]);
    } else {
      // No camera layer: the effect's own perspective, a camera on the Z axis looking at the plane.
      double denominator = own_camera_distance - z;
      if (denominator < 1.0) {
        denominator = 1.0;
      }
      const double projection = own_camera_distance / denominator;
      vertex_x[i] = x * projection;
      vertex_y[i] = y * projection;
      perspective[i] = projection;
    }
  }
}

void ThreeDTransformEffect::process_coords(double timecode, GLTextureCoords& coords, int) {
  Clip* c = parent_clip;

  const double media_w = c->media_width();
  const double media_h = c->media_height();

  if (media_w <= 0 || media_h <= 0) {
    return;
  }

  // decide whether this frame needs the motion blur pass (runs before the shader flag is checked)
  UpdateMotionBlur(timecode);

  double vertex_x[4];
  double vertex_y[4];
  double perspective[4];
  Project(timecode, media_w, media_h, vertex_x, vertex_y, perspective);

  const double corner_u[4] = { 0.0, 1.0, 1.0, 0.0 };
  const double corner_v[4] = { 0.0, 0.0, 1.0, 1.0 };

  int* out_x[4] = { &coords.vertexTopLeftX, &coords.vertexTopRightX,
                    &coords.vertexBottomRightX, &coords.vertexBottomLeftX };
  int* out_y[4] = { &coords.vertexTopLeftY, &coords.vertexTopRightY,
                    &coords.vertexBottomRightY, &coords.vertexBottomLeftY };
  float* tex_u[4] = { &coords.textureTopLeftX, &coords.textureTopRightX,
                      &coords.textureBottomRightX, &coords.textureBottomLeftX };
  float* tex_v[4] = { &coords.textureTopLeftY, &coords.textureTopRightY,
                      &coords.textureBottomRightY, &coords.textureBottomLeftY };
  float* tex_q[4] = { &coords.textureTopLeftQ, &coords.textureTopRightQ,
                      &coords.textureBottomRightQ, &coords.textureBottomLeftQ };

  for (int i = 0; i < 4; i++) {
    *out_x[i] = qRound(vertex_x[i]);
    *out_y[i] = qRound(vertex_y[i]);

    // projective texture coordinates: the hardware divides by Q, which reproduces the correct
    // perspective interpolation across the quad
    *tex_u[i] = float(corner_u[i] * perspective[i]);
    *tex_v[i] = float(corner_v[i] * perspective[i]);
    *tex_q[i] = float(perspective[i]);
  }

  coords.opacity *= float(opacity_field->GetDoubleAt(timecode) * 0.01);
}

bool ThreeDTransformEffect::GetTransformMatrix(double timecode, double* matrix) {
  Clip* c = parent_clip;
  if (c == nullptr || c->sequence == nullptr) {
    return false;
  }

  const double scale_value = scale_field->GetDoubleAt(timecode) * 0.01;

  // Position is in sequence pixels with the sequence centre as the origin.
  const double translate_x = position_x->GetDoubleAt(timecode) - (c->sequence->width / 2.0);
  const double translate_y = position_y->GetDoubleAt(timecode) - (c->sequence->height / 2.0);
  const double translate_z = position_z->GetDoubleAt(timecode);

  // Same rotation order as Project(): Z, then X, then Y.
  mat4_identity(matrix);
  mat4_translate(matrix, translate_x, translate_y, translate_z);
  mat4_rotate_y(matrix, rotation_y->GetDoubleAt(timecode));
  mat4_rotate_x(matrix, rotation_x->GetDoubleAt(timecode));
  mat4_rotate_z(matrix, rotation_z->GetDoubleAt(timecode));
  mat4_scale(matrix, scale_value, scale_value);

  // This is the clip's pure scene-space transform: no perspective is folded in. A parented child
  // composes it in 3D and projects the result with the camera, so nothing has to be faked here.
  mat4_translate(matrix, -anchor_x->GetDoubleAt(timecode), -anchor_y->GetDoubleAt(timecode), 0);

  // Same as Project(): the layer's flat Transform placement belongs inside the scene matrix, so a
  // child of this layer follows where it really sits (build_parent_matrix() skips that flat effect
  // for a parent that has a 3D Transform, so it is only counted here).
  double placement[16];
  if (build_clip_flat_matrix(c, timecode, placement)) {
    double composed[16];
    mat4_multiply(placement, matrix, composed);
    for (int i = 0; i < 16; i++) {
      matrix[i] = composed[i];
    }
  }

  return true;
}

MotionBlurPose ThreeDTransformEffect::pose_at(double timecode, double media_w, double media_h) {
  double vertex_x[4];
  double vertex_y[4];
  double perspective[4];
  Project(timecode, media_w, media_h, vertex_x, vertex_y, perspective);

  const double half_w = media_w / 2.0;
  const double half_h = media_h / 2.0;
  const double px[4] = { -half_w, half_w, half_w, -half_w };
  const double py[4] = { -half_h, -half_h, half_h, half_h };

  // A perspective projection is not affine, so each sample is approximated by the least squares
  // affine fit of its projected quad (exact for translation, scale and Z rotation).
  return motionblur::FitAffinePose(px, py, vertex_x, vertex_y);
}

void ThreeDTransformEffect::UpdateMotionBlur(double timecode) {
  blur_matrices_.clear();
  blur_offsets_.clear();

  bool active = false;

  Sequence* seq = parent_clip->sequence;

  if (seq != nullptr && seq->frame_rate > 0) {
    const double angle = shutter_angle->GetDoubleAt(timecode);

    if (angle > 0.0) {
      // The blur pass runs on the clip's source texture *before* the transform is applied to the
      // quad, so the motion samples are expressed in media pixels.
      const double media_w = parent_clip->media_width();
      const double media_h = parent_clip->media_height();

      if (media_w > 0 && media_h > 0) {
        const int sample_count = qBound(1, qRound(shutter_samples->GetDoubleAt(timecode)), kMaxBlurSamples);

        QVector<double> sample_times;
        motionblur::ShutterSampleOffsets(angle, shutter_phase->GetDoubleAt(timecode),
                                         sample_count, sample_times);

        MotionBlurPose p0 = pose_at(timecode, media_w, media_h);

        QVector<MotionBlurPose> poses;
        poses.reserve(sample_times.size());
        for (int i=0;i<sample_times.size();i++) {
          poses.append(pose_at(timecode + sample_times.at(i) / seq->frame_rate, media_w, media_h));
        }

        // Parenting is already part of Project() (the 3D chain is composed in scene space), so the
        // samples above already follow an animated parent. Folding a parent pose in again would
        // apply the parent twice.
        active = motionblur::BuildSamples(p0, poses, media_w, media_h, blur_matrices_, blur_offsets_);

        // Evaluating fields stores the result in each field's persistent data, so finish on the
        // real timecode to leave the UI showing the right values.
        (void) pose_at(timecode, media_w, media_h);
      }
    }
  }

  if (!active) {
    blur_matrices_.clear();
    blur_offsets_.clear();
  }

  // The pass has to run after the rest of the effect chain: on generator clips (titles, solids) the
  // source texture doesn't exist until a later effect has produced it.
  SetFlags(active ? (Effect::CoordsFlag | Effect::PostShaderFlag) : Effect::CoordsFlag);
}

void ThreeDTransformEffect::process_shader(double timecode, GLTextureCoords& coords, int iteration) {
  // resolution, time and every field's uniform
  Effect::process_shader(timecode, coords, iteration);

  const int count = blur_matrices_.size();
  glslProgram->setUniformValue("mbCount", count);
  if (count > 0) {
    glslProgram->setUniformValueArray("mbMat", blur_matrices_.constData(), count);
    glslProgram->setUniformValueArray("mbOff", blur_offsets_.constData(), count);
  }
}

void ThreeDTransformEffect::startEffect() {
  // Effect::open() only builds the shader program if the shader flag was set at that moment, which
  // it usually wasn't (the flag is only set on frames that are blurred), so build it on first use.
  if (is_open() && glslProgram == nullptr) {
    close();
    open();
  }

  Effect::startEffect();
}
