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

#ifndef SHAKEEFFECT_H
#define SHAKEEFFECT_H

#include <QVector>
#include <QVector3D>
#include <QVector4D>

#include "effects/effect.h"
#include "motionblur.h"

#define RANDOM_VAL_SIZE 30

class ShakeEffect : public Effect {
  Q_OBJECT
public:
    ShakeEffect(Clip* c, const EffectMeta* em);
    void process_coords(double timecode, GLTextureCoords& coords, int data);

    // motion blur pass (only runs on frames where the shake actually moves)
    void process_shader(double timecode, GLTextureCoords& coords, int iteration);
    void startEffect();

  DoubleField* intensity_val;
  DoubleField* rotation_val;
  DoubleField* frequency_val;
private:
  double random_vals[RANDOM_VAL_SIZE];

  // motion blur
  void UpdateMotionBlur(double timecode);
  MotionBlurPose pose_at(double timecode);
  // raw shake values at one instant: translation in media pixels, rotation in degrees
  void shake_at(double timecode, double& xoff, double& yoff, double& rotoff);

  DoubleField* shutter_angle;
  DoubleField* shutter_phase;
  DoubleField* shutter_samples;

  QVector<QVector4D> blur_matrices_;
  QVector<QVector3D> blur_offsets_;
};

#endif // SHAKEEFFECT_H
