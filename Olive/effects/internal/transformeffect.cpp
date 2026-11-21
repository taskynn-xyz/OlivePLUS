/***

    Olive - Non-Linear Video Editor
    Copyright (C) 2019  Olive Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

***/

#include "transformeffect.h"

#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QOpenGLFunctions>
#include <QComboBox>
#include <QMouseEvent>
#include <QtMath>

#include "ui/collapsiblewidget.h"
#include "timeline/clip.h"
#include "timeline/sequence.h"
#include "project/footage.h"
#include "global/math.h"
#include "ui/labelslider.h"
#include "ui/comboboxex.h"
#include "panels/project.h"
#include "global/debug.h"

#include "panels/panels.h"
#include "panels/viewer.h"
#include "ui/viewerwidget.h"
#include "rendering/renderfunctions.h"

namespace {

// Must match MAX_SAMPLES in transformmb.frag
const int kMaxBlurSamples = 64;

}

TransformEffect::TransformEffect(Clip* c, const EffectMeta* em) : Effect(c, em) {
  SetFlags(Effect::CoordsFlag);

  // Shader used for the motion blur pass (shared with the Shake effect). The ShaderFlag itself is only
  // set on frames that need the pass (see UpdateMotionBlur()). Paths are relative to an effects folder,
  // so motionblur/ ships in effects/motionblur/ (next to the executable's effects directory).
  vertPath = "motionblur/motionblur.vert";
  fragPath = "motionblur/motionblur.frag";

  EffectRow* position_row = new EffectRow(this, tr("Position"));

  position_x = new DoubleField(position_row, "posx"); // position X
  position_y = new DoubleField(position_row, "posy"); // position Y

  EffectRow* scale_row = new EffectRow(this, tr("Scale"));

  // scale X (and Y is uniform scale is selected)
  scale_x = new DoubleField(scale_row, "scalex");
  scale_x->SetMinimum(0);

  // scale Y (disabled if uniform scale is selected)
  scale_y = new DoubleField(scale_row, "scaley");
  scale_y->SetMinimum(0);

  EffectRow* uniform_scale_row = new EffectRow(this, tr("Uniform Scale"));

  uniform_scale_field = new BoolField(uniform_scale_row, "uniformscale"); // uniform scale option

  EffectRow* rotation_row = new EffectRow(this, tr("Rotation"));

  rotation = new DoubleField(rotation_row, "rotation");

  EffectRow* anchor_point_row = new EffectRow(this, tr("Anchor Point"));

  anchor_x_box = new DoubleField(anchor_point_row, "anchorx"); // anchor point X
  anchor_y_box = new DoubleField(anchor_point_row, "anchory"); // anchor point Y

  EffectRow* opacity_row = new EffectRow(this, tr("Opacity"));

  // opacity
  opacity = new DoubleField(opacity_row, "opacity");
  opacity->SetMinimum(0);
  opacity->SetMaximum(100);

  EffectRow* blend_mode_row = new EffectRow(this, tr("Blend Mode"));

  // blend mode
  blend_mode_box = new ComboField(blend_mode_row, "blendmode");
  blend_mode_box->SetColumnSpan(2);
  for (int i=0;i<BLEND_MODE_COUNT;i++) {
    blend_mode_box->AddItem(blend_mode_name(i), i);
  }

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

  // set up gizmos
  top_left_gizmo = add_gizmo(GIZMO_TYPE_DOT);
  top_left_gizmo->set_cursor(Qt::SizeFDiagCursor);
  top_left_gizmo->x_field1 = scale_x;

  top_center_gizmo = add_gizmo(GIZMO_TYPE_DOT);
  top_center_gizmo->set_cursor(Qt::SizeVerCursor);
  top_center_gizmo->y_field1 = scale_x;

  top_right_gizmo = add_gizmo(GIZMO_TYPE_DOT);
  top_right_gizmo->set_cursor(Qt::SizeBDiagCursor);
  top_right_gizmo->x_field1 = scale_x;

  bottom_left_gizmo = add_gizmo(GIZMO_TYPE_DOT);
  bottom_left_gizmo->set_cursor(Qt::SizeBDiagCursor);
  bottom_left_gizmo->x_field1 = scale_x;

  bottom_center_gizmo = add_gizmo(GIZMO_TYPE_DOT);
  bottom_center_gizmo->set_cursor(Qt::SizeVerCursor);
  bottom_center_gizmo->y_field1 = scale_x;

  bottom_right_gizmo = add_gizmo(GIZMO_TYPE_DOT);
  bottom_right_gizmo->set_cursor(Qt::SizeFDiagCursor);
  bottom_right_gizmo->x_field1 = scale_x;

  left_center_gizmo = add_gizmo(GIZMO_TYPE_DOT);
  left_center_gizmo->set_cursor(Qt::SizeHorCursor);
  left_center_gizmo->x_field1 = scale_x;

  right_center_gizmo = add_gizmo(GIZMO_TYPE_DOT);
  right_center_gizmo->set_cursor(Qt::SizeHorCursor);
  right_center_gizmo->x_field1 = scale_x;

  anchor_gizmo = add_gizmo(GIZMO_TYPE_TARGET);
  anchor_gizmo->set_cursor(Qt::SizeAllCursor);
  anchor_gizmo->x_field1 = anchor_x_box;
  anchor_gizmo->y_field1 = anchor_y_box;
  anchor_gizmo->x_field2 = position_x;
  anchor_gizmo->y_field2 = position_y;

  rotate_gizmo = add_gizmo(GIZMO_TYPE_DOT);
  rotate_gizmo->color = Qt::green;
  rotate_gizmo->set_cursor(Qt::SizeAllCursor);
  rotate_gizmo->x_field1 = rotation;

  rect_gizmo = add_gizmo(GIZMO_TYPE_POLY);
  rect_gizmo->x_field1 = position_x;
  rect_gizmo->y_field1 = position_y;

  connect(uniform_scale_field, SIGNAL(Toggled(bool)), this, SLOT(toggle_uniform_scale(bool)));

  // set defaults
  uniform_scale_field->SetValueAt(0, true);
  blend_mode_box->SetValueAt(0, int(BLEND_MODE_NORMAL));
  anchor_x_box->SetDefault(0);
  anchor_y_box->SetDefault(0);
  opacity->SetDefault(100);
  scale_x->SetDefault(100);
  scale_y->SetDefault(100);
  shutter_angle->SetDefault(180); // 0 disables motion blur
  shutter_phase->SetDefault(0);   // 0 = exposure centered on the frame
  shutter_samples->SetDefault(16);

  refresh();
}

void TransformEffect::refresh() {
  if (parent_clip != nullptr && parent_clip->sequence != nullptr) {

    position_x->SetDefault(parent_clip->sequence->width/2);
    position_y->SetDefault(parent_clip->sequence->height/2);

    double x_percent_multipler = 200.0 / parent_clip->sequence->width;
    double y_percent_multipler = 200.0 / parent_clip->sequence->height;

    top_left_gizmo->x_field_multi1 = -x_percent_multipler;
    top_left_gizmo->y_field_multi1 = -y_percent_multipler;
    top_center_gizmo->y_field_multi1 = -y_percent_multipler;
    top_right_gizmo->x_field_multi1 = x_percent_multipler;
    top_right_gizmo->y_field_multi1 = -y_percent_multipler;
    bottom_left_gizmo->x_field_multi1 = -x_percent_multipler;
    bottom_left_gizmo->y_field_multi1 = y_percent_multipler;
    bottom_center_gizmo->y_field_multi1 = y_percent_multipler;
    bottom_right_gizmo->x_field_multi1 = x_percent_multipler;
    bottom_right_gizmo->y_field_multi1 = y_percent_multipler;
    left_center_gizmo->x_field_multi1 = -x_percent_multipler;
    right_center_gizmo->x_field_multi1 = x_percent_multipler;
    rotate_gizmo->x_field_multi1 = x_percent_multipler;

  }
}

void TransformEffect::toggle_uniform_scale(bool enabled) {
  scale_y->SetEnabled(!enabled);

  top_center_gizmo->y_field1 = enabled ? scale_x : scale_y;
  bottom_center_gizmo->y_field1 = enabled ? scale_x : scale_y;
  top_left_gizmo->y_field1 = enabled ? nullptr : scale_y;
  top_right_gizmo->y_field1 = enabled ? nullptr : scale_y;
  bottom_left_gizmo->y_field1 = enabled ? nullptr : scale_y;
  bottom_right_gizmo->y_field1 = enabled ? nullptr : scale_y;
}

void TransformEffect::process_coords(double timecode, GLTextureCoords& coords, int) {
  // When the clip also has a 3D Transform, that effect folds this matrix into the 3D scene
  // (ThreeDTransformEffect::Project), where it is what the layer is parented by. Applying it here as
  // well would move the layer twice, and applying it after the 3D projection would keep its offset
  // out of a 3D parent's rotation.
  const bool geometry_handled_by_3d = parent_clip != nullptr && clip_has_3d_transform(parent_clip);

  if (geometry_handled_by_3d) {
    blur_matrices_.clear();
    blur_offsets_.clear();
    SetFlags(Effect::CoordsFlag);
  } else {
    // decide whether this frame needs the motion blur pass (runs before the shader flag is checked)
    UpdateMotionBlur(timecode);

    // position
    glTranslated(position_x->GetDoubleAt(timecode)-(parent_clip->sequence->width/2),
                 position_y->GetDoubleAt(timecode)-(parent_clip->sequence->height/2),
                 0);

    // anchor point
    int anchor_x_offset = qRound(anchor_x_box->GetDoubleAt(timecode));
    int anchor_y_offset = qRound(anchor_y_box->GetDoubleAt(timecode));
    coords.vertexTopLeftX -= anchor_x_offset;
    coords.vertexTopRightX -= anchor_x_offset;
    coords.vertexBottomLeftX -= anchor_x_offset;
    coords.vertexBottomRightX -= anchor_x_offset;
    coords.vertexTopLeftY -= anchor_y_offset;
    coords.vertexTopRightY -= anchor_y_offset;
    coords.vertexBottomLeftY -= anchor_y_offset;
    coords.vertexBottomRightY -= anchor_y_offset;

    // rotation
    glRotated(rotation->GetDoubleAt(timecode), 0, 0, 1);

    // scale
    double sx = scale_x->GetDoubleAt(timecode)*0.01;
    double sy = (uniform_scale_field->GetBoolAt(timecode)) ? sx : scale_y->GetDoubleAt(timecode)*0.01;
    glScaled(sx, sy, 1);
  }

  // blend mode
  coords.blendmode = blend_mode_box->GetValueAt(timecode).toInt();

  // opacity
  coords.opacity *= float(opacity->GetDoubleAt(timecode)*0.01);
}

bool TransformEffect::GetTransformMatrix(double timecode, double* matrix) {
  if (parent_clip == nullptr || parent_clip->sequence == nullptr) {
    return false;
  }

  const double seq_w = parent_clip->sequence->width;
  const double seq_h = parent_clip->sequence->height;

  double sx = scale_x->GetDoubleAt(timecode) * 0.01;
  const double sy = uniform_scale_field->GetBoolAt(timecode) ? sx : scale_y->GetDoubleAt(timecode) * 0.01;

  // Same order as process_coords(): translate to the position, rotate, scale, then move the
  // clip's geometry onto its anchor point.
  mat4_identity(matrix);
  mat4_translate(matrix,
                 position_x->GetDoubleAt(timecode) - (seq_w / 2.0),
                 position_y->GetDoubleAt(timecode) - (seq_h / 2.0),
                 0);
  mat4_rotate_z(matrix, rotation->GetDoubleAt(timecode));
  mat4_scale(matrix, sx, sy);
  mat4_translate(matrix,
                 -anchor_x_box->GetDoubleAt(timecode),
                 -anchor_y_box->GetDoubleAt(timecode),
                 0);

  return true;
}

MotionBlurPose TransformEffect::pose_at(double timecode, double seq_w, double seq_h) {
  MotionBlurPose p;
  p.tx = position_x->GetDoubleAt(timecode) - (seq_w / 2.0);
  p.ty = position_y->GetDoubleAt(timecode) - (seq_h / 2.0);
  p.ax = qRound(anchor_x_box->GetDoubleAt(timecode));
  p.ay = qRound(anchor_y_box->GetDoubleAt(timecode));

  const double rad = qDegreesToRadians(rotation->GetDoubleAt(timecode));
  const double sx = scale_x->GetDoubleAt(timecode) * 0.01;
  const double sy = uniform_scale_field->GetBoolAt(timecode) ? sx : scale_y->GetDoubleAt(timecode) * 0.01;
  const double c = qCos(rad);
  const double s = qSin(rad);

  p.m00 = c * sx;
  p.m01 = -s * sy;
  p.m10 = s * sx;
  p.m11 = c * sy;
  return p;
}

void TransformEffect::UpdateMotionBlur(double timecode) {
  blur_matrices_.clear();
  blur_offsets_.clear();

  bool active = false;

  Sequence* seq = parent_clip->sequence;

  if (seq != nullptr && seq->frame_rate > 0) {
    const double angle = shutter_angle->GetDoubleAt(timecode);

    if (angle > 0.0) {
      // The blur pass runs on the clip's source texture *before* the transform is applied to the quad
      // (see compose_sequence()), so the mapping has to be expressed in media pixels. Media and
      // sequence pixels differ whenever the clip resolution differs from the sequence.
      const double media_w = parent_clip->media_width();
      const double media_h = parent_clip->media_height();
      const double seq_w = seq->width;
      const double seq_h = seq->height;

      if (media_w > 0 && media_h > 0) {
        const int sample_count = qBound(1, qRound(shutter_samples->GetDoubleAt(timecode)), kMaxBlurSamples);

        QVector<double> sample_times;
        motionblur::ShutterSampleOffsets(angle, shutter_phase->GetDoubleAt(timecode),
                                         sample_count, sample_times);

        // same math as process_coords(): M(p) = T + L * (p - anchor), where p is a media pixel,
        // L = rotation * scale and T is the position in sequence pixels.
        MotionBlurPose p0 = pose_at(timecode, seq_w, seq_h);

        QVector<MotionBlurPose> poses;
        poses.reserve(sample_times.size());
        for (int i=0;i<sample_times.size();i++) {
          poses.append(pose_at(timecode + sample_times.at(i) / seq->frame_rate, seq_w, seq_h));
        }

        // A parented clip inherits its parent's motion too, but the parent matrix is applied by the
        // compositor at draw time, outside this effect, so fold it into every sample here. Without
        // this an animated null moves a parented clip with no motion blur at all.
        const double base_playhead = double(seq->playhead);
        MotionBlurPose parent_pose;
        if (motionblur::ParentPose(parent_clip, base_playhead, parent_pose)) {
          p0 = motionblur::ComposePose(parent_pose, p0);
          for (int i=0;i<poses.size();i++) {
            MotionBlurPose parent_sample;
            if (motionblur::ParentPose(parent_clip, base_playhead + sample_times.at(i), parent_sample)) {
              poses[i] = motionblur::ComposePose(parent_sample, poses.at(i));
            }
          }
        }

        active = motionblur::BuildSamples(p0, poses, media_w, media_h, blur_matrices_, blur_offsets_);

        // Evaluating fields stores the result in each field's persistent data, so finish on the
        // real timecode to leave the UI showing the right values.
        (void) pose_at(timecode, seq_w, seq_h);
      }
    }
  }

  if (!active) {
    blur_matrices_.clear();
    blur_offsets_.clear();
  }

  // The pass has to run after the rest of the effect chain: on generator clips the source texture
  // doesn't exist until a later effect (e.g. Rich Text) creates it.
  SetFlags(active ? (Effect::CoordsFlag | Effect::PostShaderFlag) : Effect::CoordsFlag);
}

void TransformEffect::process_shader(double timecode, GLTextureCoords& coords, int iteration) {
  // resolution, time and every field's uniform
  Effect::process_shader(timecode, coords, iteration);

  const int count = blur_matrices_.size();
  glslProgram->setUniformValue("mbCount", count);
  if (count > 0) {
    glslProgram->setUniformValueArray("mbMat", blur_matrices_.constData(), count);
    glslProgram->setUniformValueArray("mbOff", blur_offsets_.constData(), count);
  }
}

void TransformEffect::startEffect() {
  // Effect::open() only builds the shader program if the shader flag was set at that moment, which it
  // usually wasn't (the flag is only set on frames that are blurred), so build it on first use.
  if (is_open() && glslProgram == nullptr) {
    close();
    open();
  }

  Effect::startEffect();
}

void TransformEffect::gizmo_draw(double, GLTextureCoords& coords) {
  top_left_gizmo->world_pos[0] = QPoint(coords.vertexTopLeftX, coords.vertexTopLeftY);
  top_center_gizmo->world_pos[0] = QPoint(lerp(coords.vertexTopLeftX, coords.vertexTopRightX, 0.5), lerp(coords.vertexTopLeftY, coords.vertexTopRightY, 0.5));
  top_right_gizmo->world_pos[0] = QPoint(coords.vertexTopRightX, coords.vertexTopRightY);
  right_center_gizmo->world_pos[0] = QPoint(lerp(coords.vertexTopRightX, coords.vertexBottomRightX, 0.5), lerp(coords.vertexTopRightY, coords.vertexBottomRightY, 0.5));
  bottom_right_gizmo->world_pos[0] = QPoint(coords.vertexBottomRightX, coords.vertexBottomRightY);
  bottom_center_gizmo->world_pos[0] = QPoint(lerp(coords.vertexBottomRightX, coords.vertexBottomLeftX, 0.5), lerp(coords.vertexBottomRightY, coords.vertexBottomLeftY, 0.5));
  bottom_left_gizmo->world_pos[0] = QPoint(coords.vertexBottomLeftX, coords.vertexBottomLeftY);
  left_center_gizmo->world_pos[0] = QPoint(lerp(coords.vertexBottomLeftX, coords.vertexTopLeftX, 0.5), lerp(coords.vertexBottomLeftY, coords.vertexTopLeftY, 0.5));

  rotate_gizmo->world_pos[0] = QPoint(lerp(top_center_gizmo->world_pos[0].x(), bottom_center_gizmo->world_pos[0].x(), -0.1), lerp(top_center_gizmo->world_pos[0].y(), bottom_center_gizmo->world_pos[0].y(), -0.1));

  rect_gizmo->world_pos[0] = QPoint(coords.vertexTopLeftX, coords.vertexTopLeftY);
  rect_gizmo->world_pos[1] = QPoint(coords.vertexTopRightX, coords.vertexTopRightY);
  rect_gizmo->world_pos[2] = QPoint(coords.vertexBottomRightX, coords.vertexBottomRightY);
  rect_gizmo->world_pos[3] = QPoint(coords.vertexBottomLeftX, coords.vertexBottomLeftY);
}
