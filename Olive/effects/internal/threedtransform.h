#ifndef THREEDTRANSFORMEFFECT_H
#define THREEDTRANSFORMEFFECT_H

#include <QVector3D>
#include <QVector4D>

#include "effects/effect.h"
#include "motionblur.h"

/**
 * @brief A perspective (3D) transform for clips
 *
 * Unlike the default Transform effect, this one rotates the clip in three dimensions and projects
 * it with a camera, so keyframing Y rotation gives the classic "card flip" motion-graphics move.
 * The compositor already draws clips as a single quad, so the projection is done on the CPU: each
 * corner is transformed and perspective-divided, and its texture coordinate carries the perspective
 * weight (GLTextureCoords::texture*Q) so the image is interpolated correctly across the quad.
 *
 * Motion blur uses the same pass as the default Transform effect (effects/motionblur/). The pass
 * blurs the clip's source texture before the transform is applied, and works from affine motion
 * samples; a perspective projection is not affine, so each sample is approximated by the least
 * squares affine fit of that sample's projected quad. That is exact for translation, scale and Z
 * rotation, and a good approximation for X/Y rotation (which is what produces the nice smear
 * during a card flip).
 */
class ThreeDTransformEffect : public Effect {
  Q_OBJECT
public:
  ThreeDTransformEffect(Clip* c, const EffectMeta* em);

  void process_coords(double timecode, GLTextureCoords& coords, int data) override;

  // parenting: this clip's own scene-space transform, without any projection baked in
  bool GetTransformMatrix(double timecode, double* matrix) override;
  void process_shader(double timecode, GLTextureCoords& coords, int iteration) override;
  void startEffect() override;

protected:
  void refresh() override;

private:
  /**
   * @brief Project the media's four corners at a given time
   *
   * Fills the screen position (in sequence pixels, origin at the sequence centre) and the
   * perspective weight of each corner, in the order the compositor draws them: top left, top right,
   * bottom right, bottom left.
   */
  void Project(double timecode, double media_w, double media_h,
               double vertex_x[4], double vertex_y[4], double perspective[4]);

  /**
   * @brief Affine approximation of the projection at a given time
   *
   * Least squares fit of the projected quad, expressed as the MotionBlurPose the motion blur pass
   * expects (screen = T + L * media pixel).
   */
  MotionBlurPose pose_at(double timecode, double media_w, double media_h);

  void UpdateMotionBlur(double timecode);

  DoubleField* position_x;
  DoubleField* position_y;
  DoubleField* position_z;

  DoubleField* rotation_x;
  DoubleField* rotation_y;
  DoubleField* rotation_z;

  DoubleField* scale_field;
  DoubleField* distance_field;

  DoubleField* anchor_x;
  DoubleField* anchor_y;

  DoubleField* opacity_field;

  DoubleField* shutter_angle;
  DoubleField* shutter_phase;
  DoubleField* shutter_samples;

  QVector<QVector4D> blur_matrices_;
  QVector<QVector3D> blur_offsets_;
};

#endif // THREEDTRANSFORMEFFECT_H
