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

#include "shakeeffect.h"

#include <QGridLayout>
#include <QLabel>
#include <QtMath>
#include <QOpenGLFunctions>

#include "ui/labelslider.h"
#include "ui/collapsiblewidget.h"
#include "timeline/clip.h"
#include "timeline/sequence.h"
#include "panels/timeline.h"
#include "global/debug.h"

namespace {

// Must match MAX_SAMPLES in motionblur.frag
const int kMaxBlurSamples = 64;

}

ShakeEffect::ShakeEffect(Clip* c, const EffectMeta *em) : Effect(c, em) {
  SetFlags(Effect::CoordsFlag);

  // Shader used for the motion blur pass (shared with the Transform effect). The ShaderFlag itself is
  // only set on frames that need the pass (see UpdateMotionBlur()). Paths are relative to an effects
  // folder, so motionblur/ ships in effects/motionblur/ (next to the executable's effects directory).
  vertPath = "motionblur/motionblur.vert";
  fragPath = "motionblur/motionblur.frag";

  EffectRow* intensity_row = new EffectRow(this, tr("Intensity"));
  intensity_val = new DoubleField(intensity_row, "intensity");
  intensity_val->SetMinimum(0);
  intensity_val->SetDefault(25);

  EffectRow* rotation_row = new EffectRow(this, tr("Rotation"));
  rotation_val = new DoubleField(rotation_row, "rotation");
  rotation_val->SetMinimum(0);
  rotation_val->SetDefault(10);

  EffectRow* frequency_row = new EffectRow(this, tr("Frequency"));
  frequency_val = new DoubleField(frequency_row, "frequency");
  frequency_val->SetMinimum(0);
  frequency_val->SetDefault(5);

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

  shutter_angle->SetDefault(180); // 0 disables motion blur
  shutter_phase->SetDefault(0);   // 0 = exposure centred on the frame
  shutter_samples->SetDefault(16);

  const auto limit = std::numeric_limits<int32_t>::max();
  for (int i=0;i<RANDOM_VAL_SIZE;i++) {
    random_vals[i] = static_cast<double>(this->randomNumber<int32_t>()) / limit;
  }
}

void ShakeEffect::shake_at(double timecode, double& xoff, double& yoff, double& rotoff) {
  int lim = RANDOM_VAL_SIZE/6;

  double multiplier = intensity_val->GetDoubleAt(timecode)/lim;
  double rotmult = rotation_val->GetDoubleAt(timecode)/lim/10;
  double x = timecode * frequency_val->GetDoubleAt(timecode);

  xoff = 0;
  yoff = 0;
  rotoff = 0;

  for (int i=0;i<lim;i++) {
    int offset = 6*i;
    xoff += qSin((x+random_vals[offset])*random_vals[offset+1]);
    yoff += qSin((x+random_vals[offset+2])*random_vals[offset+3]);
    rotoff += qSin((x+random_vals[offset+4])*random_vals[offset+5]);
  }

  xoff *= multiplier;
  yoff *= multiplier;
  rotoff *= rotmult;
}

void ShakeEffect::process_coords(double timecode, GLTextureCoords& coords, int) {
  // decide whether this frame needs the motion blur pass (runs before the shader flag is checked)
  UpdateMotionBlur(timecode);

  double xoff, yoff, rotoff;
  shake_at(timecode, xoff, yoff, rotoff);

  coords.vertexTopLeftX += xoff;
  coords.vertexTopRightX += xoff;
  coords.vertexBottomLeftX += xoff;
  coords.vertexBottomRightX += xoff;

  coords.vertexTopLeftY += yoff;
  coords.vertexTopRightY += yoff;
  coords.vertexBottomLeftY += yoff;
  coords.vertexBottomRightY += yoff;

  glRotatef(rotoff, 0, 0, 1);
}

MotionBlurPose ShakeEffect::pose_at(double timecode) {
  double xoff, yoff, rotoff;
  shake_at(timecode, xoff, yoff, rotoff);

  MotionBlurPose p;
  p.tx = xoff;
  p.ty = yoff;

  // glRotatef pivots around the clip's centre, which is the pose space origin (anchor 0)
  p.ax = 0;
  p.ay = 0;

  const double rad = qDegreesToRadians(rotoff);
  const double c = qCos(rad);
  const double s = qSin(rad);

  p.m00 = c;
  p.m01 = -s;
  p.m10 = s;
  p.m11 = c;
  return p;
}

void ShakeEffect::UpdateMotionBlur(double timecode) {
  blur_matrices_.clear();
  blur_offsets_.clear();

  bool active = false;

  Sequence* seq = parent_clip->sequence;

  if (seq != nullptr && seq->frame_rate > 0) {
    const double angle = shutter_angle->GetDoubleAt(timecode);

    if (angle > 0.0) {
      // The blur pass runs on the clip's source texture before the shake is applied to the quad, so
      // the mapping is expressed in media pixels.
      const double media_w = parent_clip->media_width();
      const double media_h = parent_clip->media_height();

      if (media_w > 0 && media_h > 0) {
        const int sample_count = qBound(1, qRound(shutter_samples->GetDoubleAt(timecode)), kMaxBlurSamples);

        QVector<double> sample_times;
        motionblur::ShutterSampleOffsets(angle, shutter_phase->GetDoubleAt(timecode),
                                         sample_count, sample_times);

        const MotionBlurPose p0 = pose_at(timecode);

        QVector<MotionBlurPose> poses;
        poses.reserve(sample_times.size());
        for (int i=0;i<sample_times.size();i++) {
          poses.append(pose_at(timecode + sample_times.at(i) / seq->frame_rate));
        }

        active = motionblur::BuildSamples(p0, poses, media_w, media_h, blur_matrices_, blur_offsets_);

        // Evaluating fields stores the result in each field's persistent data, so finish on the
        // real timecode to leave the UI showing the right values.
        (void) pose_at(timecode);
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

void ShakeEffect::process_shader(double timecode, GLTextureCoords& coords, int iteration) {
  // resolution, time and every field's uniform
  Effect::process_shader(timecode, coords, iteration);

  const int count = blur_matrices_.size();
  glslProgram->setUniformValue("mbCount", count);
  if (count > 0) {
    glslProgram->setUniformValueArray("mbMat", blur_matrices_.constData(), count);
    glslProgram->setUniformValueArray("mbOff", blur_offsets_.constData(), count);
  }
}

void ShakeEffect::startEffect() {
  // Effect::open() only builds the shader program if the shader flag was set at that moment, which it
  // usually wasn't (the flag is only set on frames that are blurred), so build it on first use.
  if (is_open() && glslProgram == nullptr) {
    close();
    open();
  }

  Effect::startEffect();
}
