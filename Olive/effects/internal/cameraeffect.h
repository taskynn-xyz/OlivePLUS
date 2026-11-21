#ifndef CAMERAEFFECT_H
#define CAMERAEFFECT_H

#include "effects/effect.h"

class Clip;
class Sequence;

/**
 * @brief A 3D camera, the equivalent of After Effects' camera layer
 *
 * The camera is a layer that draws nothing. Its effect holds a position, a point of interest and a
 * zoom (focal length in pixels), and it is only read by the 3D Transform effect: a clip without a
 * 3D Transform is composited in 2D and is not affected by the camera at all.
 *
 * A camera layer can be parented like any other layer. Its parent chain is applied to the camera and
 * to its point of interest before the view is built, so a 3D null works as a camera rig: rotating the
 * null orbits the camera around it, moving it trucks the camera, and the camera keeps aiming at the
 * transformed point of interest.
 *
 * Coordinates are the same ones the 3D Transform uses: sequence pixels with the origin at the
 * sequence centre, X to the right, Y down and Z towards the viewer.
 */
class CameraEffect : public Effect {
  Q_OBJECT
public:
  CameraEffect(Clip* c, const EffectMeta* em);

  /**
   * @brief Project a point in scene space to sequence pixels
   *
   * @param timecode
   *
   * Time within the camera layer (seconds), so its keyframes evaluate on the camera's own time.
   *
   * @param out_w
   *
   * The perspective weight of the point, which the 3D Transform puts into the corner's texture Q
   * so the quad is interpolated with a real perspective divide.
   */
  void ProjectPoint(double timecode, double x, double y, double z,
                    double* out_x, double* out_y, double* out_w);

protected:
  void refresh() override;

private:
  DoubleField* position_x;
  DoubleField* position_y;
  DoubleField* position_z;

  DoubleField* target_x;
  DoubleField* target_y;
  DoubleField* target_z;

  DoubleField* zoom_field;
};

namespace camera {

/**
 * @brief The camera layer driving a sequence at a given time, or nullptr if it has none
 *
 * Only one camera is used. If several camera layers overlap, the first one in the sequence wins.
 */
Clip* ActiveCamera(Sequence* sequence, long playhead);

/**
 * @brief The enabled Camera effect of a camera layer, or nullptr
 */
CameraEffect* EffectOf(Clip* camera_layer);

}

#endif // CAMERAEFFECT_H
