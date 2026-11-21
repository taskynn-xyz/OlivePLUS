#include "cameraeffect.h"

#include <QtMath>

#include "global/math.h"
#include "rendering/renderfunctions.h"
#include "timeline/clip.h"
#include "timeline/sequence.h"

CameraEffect::CameraEffect(Clip* c, const EffectMeta* em) : Effect(c, em) {
  EffectRow* position_row = new EffectRow(this, tr("Position"));

  position_x = new DoubleField(position_row, "posx");
  position_y = new DoubleField(position_row, "posy");
  position_z = new DoubleField(position_row, "posz");

  EffectRow* target_row = new EffectRow(this, tr("Point of Interest"));

  target_x = new DoubleField(target_row, "targetx");
  target_y = new DoubleField(target_row, "targety");
  target_z = new DoubleField(target_row, "targetz");

  EffectRow* zoom_row = new EffectRow(this, tr("Zoom"));

  zoom_field = new DoubleField(zoom_row, "zoom");
  zoom_field->SetMinimum(1);

  // Z is towards the viewer, so a positive camera Z sits in front of the content and looks back at
  // it. A distance equal to the zoom gives a 1:1 mapping for anything on the Z = 0 plane.
  position_z->SetDefault(2000);
  target_z->SetDefault(0);
  zoom_field->SetDefault(2000);

  refresh();
}

void CameraEffect::refresh() {
  if (parent_clip != nullptr && parent_clip->sequence != nullptr) {
    const double centre_x = parent_clip->sequence->width / 2.0;
    const double centre_y = parent_clip->sequence->height / 2.0;

    position_x->SetDefault(centre_x);
    position_y->SetDefault(centre_y);
    target_x->SetDefault(centre_x);
    target_y->SetDefault(centre_y);
  }
}

void CameraEffect::ProjectPoint(double timecode, double x, double y, double z,
                                double* out_x, double* out_y, double* out_w) {
  Clip* c = parent_clip;

  const double origin_x = c != nullptr && c->sequence != nullptr ? c->sequence->width / 2.0 : 0;
  const double origin_y = c != nullptr && c->sequence != nullptr ? c->sequence->height / 2.0 : 0;

  double position_x_value = position_x->GetDoubleAt(timecode) - origin_x;
  double position_y_value = position_y->GetDoubleAt(timecode) - origin_y;
  double position_z_value = position_z->GetDoubleAt(timecode);

  double target_x_value = target_x->GetDoubleAt(timecode) - origin_x;
  double target_y_value = target_y->GetDoubleAt(timecode) - origin_y;
  double target_z_value = target_z->GetDoubleAt(timecode);

  // A 3D null can drive the camera: the camera layer's own parent chain is applied to both the
  // camera and its point of interest, so parenting a camera to a null makes a rig - moving, rotating
  // or scaling the null moves the camera through the scene (and orbiting it around the null) without
  // touching any camera keyframes. This happens before the view basis is built, so the camera keeps
  // looking where its point of interest ended up.
  if (c != nullptr && c->sequence != nullptr) {
    const double playhead = double(c->timeline_in(true)) - double(c->clip_in(true))
        + (timecode * c->sequence->frame_rate);

    double parent_matrix[16];
    if (build_parent_matrix(c, playhead, parent_matrix, true)) {
      double moved_x, moved_y, moved_z;

      mat4_transform_point_3d(parent_matrix, position_x_value, position_y_value, position_z_value,
                              &moved_x, &moved_y, &moved_z);
      position_x_value = moved_x;
      position_y_value = moved_y;
      position_z_value = moved_z;

      mat4_transform_point_3d(parent_matrix, target_x_value, target_y_value, target_z_value,
                              &moved_x, &moved_y, &moved_z);
      target_x_value = moved_x;
      target_y_value = moved_y;
      target_z_value = moved_z;
    }
  }

  // look direction: from the camera towards its point of interest
  double forward_x = target_x_value - position_x_value;
  double forward_y = target_y_value - position_y_value;
  double forward_z = target_z_value - position_z_value;

  double forward_length = qSqrt((forward_x * forward_x) + (forward_y * forward_y)
                                + (forward_z * forward_z));
  if (forward_length < 1e-6) {
    // camera sitting on its own point of interest: look straight down -Z (into the scene)
    forward_x = 0;
    forward_y = 0;
    forward_z = -1;
    forward_length = 1;
  }
  forward_x /= forward_length;
  forward_y /= forward_length;
  forward_z /= forward_length;

  // screen up is -Y, since Y grows downwards in sequence space
  double right_x = -forward_z;
  double right_y = 0;
  double right_z = forward_x;
  double right_length = qSqrt((right_x * right_x) + (right_z * right_z));
  if (right_length < 1e-6) {
    // looking straight up or down: any horizontal axis will do
    right_x = 1;
    right_y = 0;
    right_z = 0;
  } else {
    right_x /= right_length;
    right_z /= right_length;
  }

  // screen down direction
  const double down_x = (right_y * forward_z) - (right_z * forward_y);
  const double down_y = (right_z * forward_x) - (right_x * forward_z);
  const double down_z = (right_x * forward_y) - (right_y * forward_x);

  const double relative_x = x - position_x_value;
  const double relative_y = y - position_y_value;
  const double relative_z = z - position_z_value;

  const double camera_x = (relative_x * right_x) + (relative_y * right_y) + (relative_z * right_z);
  const double camera_y = (relative_x * down_x) + (relative_y * down_y) + (relative_z * down_z);
  double depth = (relative_x * forward_x) + (relative_y * forward_y) + (relative_z * forward_z);

  // A point at or behind the camera would invert the image, so clamp it just in front
  if (depth < 1.0) {
    depth = 1.0;
  }

  const double zoom = qMax(1.0, zoom_field->GetDoubleAt(timecode));
  const double weight = zoom / depth;

  *out_x = camera_x * weight;
  *out_y = camera_y * weight;
  *out_w = weight;
}

namespace camera {

Clip* ActiveCamera(Sequence* sequence, long playhead) {
  if (sequence == nullptr) {
    return nullptr;
  }

  for (int i=0;i<sequence->clips.size();i++) {
    Clip* c = sequence->clips.at(i).get();
    if (c != nullptr && c->IsCameraLayer() && c->enabled() && c->IsActiveAt(playhead)) {
      return c;
    }
  }

  return nullptr;
}

CameraEffect* EffectOf(Clip* camera_layer) {
  if (camera_layer == nullptr) {
    return nullptr;
  }

  for (int i=0;i<camera_layer->effects.size();i++) {
    Effect* e = camera_layer->effects.at(i).get();
    if (e != nullptr && e->IsEnabled() && e->meta != nullptr
        && e->meta->internal == EFFECT_INTERNAL_CAMERA) {
      return static_cast<CameraEffect*>(e);
    }
  }

  return nullptr;
}

}
