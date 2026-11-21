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

#ifndef MOTIONBLUR_H
#define MOTIONBLUR_H

#include <QVector>
#include <QVector3D>
#include <QVector4D>

// How an effect places the clip at one instant, in the same units its process_coords() uses:
// a media pixel p is drawn at T + L * (p - anchor), where T is in sequence pixels, the anchor is
// in media pixels and L is the 2x2 linear part (rotation * scale).
struct MotionBlurPose {
  double tx, ty;
  double ax, ay;
  double m00, m01, m10, m11;
};

class Clip;

namespace motionblur {

// Fills offsets with the sub-frame times (in frames, relative to the current frame) covered by a
// shutter of angle_deg / phase_deg, and returns the number of samples (always at least 1).
int ShutterSampleOffsets(double angle_deg, double phase_deg, int samples, QVector<double>& offsets);

// For every sample pose, builds the affine map in normalized texture space that takes this frame's
// texture coordinates to the texture coordinates the same image content had at the sample time.
// The blur pass runs on the clip's source texture before the effect transforms it, so the map is
// derived in media pixels and then converted to uv (this conversion folds in the frame's aspect
// ratio, which matters for rotation on non-square media).
// Returns true if any sample differs from the current pose, i.e. the clip is actually moving.
bool BuildSamples(const MotionBlurPose& current,
                  const QVector<MotionBlurPose>& samples,
                  double media_w, double media_h,
                  QVector<QVector4D>& matrices,
                  QVector<QVector3D>& offsets);

// Least squares fit of  screen = L * source + T  over four matching point pairs (source in media
// pixels, screen in sequence pixels). Turns a procedural projection into the affine pose the blur
// pass works with; ax/ay are left at 0.
MotionBlurPose FitAffinePose(const double px[4], const double py[4],
                             const double qx[4], const double qy[4]);

// Composes a parent's transform on top of a child's pose. Both are in the same units (media
// pixels to sequence pixels), so the result is the pose of a clip that is parented to a null.
MotionBlurPose ComposePose(const MotionBlurPose& parent, const MotionBlurPose& child);

// Evaluates clip's parent chain (Transform + 3D Transform, with the same 3D-null rules the
// compositor uses) at a possibly fractional playhead, as a pose. False when there is no parent
// transform.
bool ParentPose(Clip* clip, double playhead, MotionBlurPose& pose);

}

#endif // MOTIONBLUR_H
