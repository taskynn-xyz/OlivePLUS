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

#include "motionblur.h"

#include <QtMath>

#include "timeline/clip.h"
#include "rendering/renderfunctions.h"

int motionblur::ShutterSampleOffsets(double angle_deg, double phase_deg, int samples, QVector<double>& offsets) {
  offsets.clear();

  if (samples < 1) samples = 1;

  // exposure window in frames, relative to this frame
  const double exposure = angle_deg / 360.0;
  const double start = phase_deg / 360.0 - exposure * 0.5;

  for (int i=0;i<samples;i++) {
    offsets.append(start + exposure * (double(i) + 0.5) / double(samples));
  }

  return samples;
}

bool motionblur::BuildSamples(const MotionBlurPose& current,
                              const QVector<MotionBlurPose>& samples,
                              double media_w, double media_h,
                              QVector<QVector4D>& matrices,
                              QVector<QVector3D>& offsets) {
  bool moving = false;

  if (media_w <= 0 || media_h <= 0) return false;

  // a pose collapsed to a line or a point has no image to blur
  const double det0 = current.m00 * current.m11 - current.m01 * current.m10;
  if (qAbs(det0) < 1e-12) return false;

  for (int i=0;i<samples.size();i++) {
    const MotionBlurPose& s = samples.at(i);

    const double det = s.m00 * s.m11 - s.m01 * s.m10;
    if (qAbs(det) < 1e-12) {
      // the clip is collapsed to a line/point at this sample; it contributes nothing
      continue;
    }

    // inverse of the sample's linear part
    const double i00 =  s.m11 / det;
    const double i01 = -s.m01 / det;
    const double i10 = -s.m10 / det;
    const double i11 =  s.m00 / det;

    // The output pixel at sequence position s shows media point M_0^-1(s) at this frame and
    // M_i^-1(s) at the sample time. The blur pass runs before the transform, so we need the
    // equivalent map from this frame's media pixels to the sample's media pixels:
    //   C_i(p) = M_i^-1(M_0(p)) = A*p + b,  A = L_i^-1 * L_0
    const double a00 = i00 * current.m00 + i01 * current.m10;
    const double a01 = i00 * current.m01 + i01 * current.m11;
    const double a10 = i10 * current.m00 + i11 * current.m10;
    const double a11 = i10 * current.m01 + i11 * current.m11;

    // b = anchor_i - A*anchor_0 + L_i^-1 * (T_0 - T_i)
    const double dtx = current.tx - s.tx;
    const double dty = current.ty - s.ty;
    const double bx = s.ax - (a00 * current.ax + a01 * current.ay) + (i00 * dtx + i01 * dty);
    const double by = s.ay - (a10 * current.ax + a11 * current.ay) + (i10 * dtx + i11 * dty);

    // Media pixels to normalized texture coordinates: p = (uv - 0.5) * size. A is expressed in
    // pixels, so the aspect ratio has to be folded into the off-diagonal terms; skipping this makes
    // rotation smear in the wrong direction on non-square frames.
    const double u00 = a00;
    const double u01 = a01 * media_h / media_w;
    const double u10 = a10 * media_w / media_h;
    const double u11 = a11;
    const double ox = 0.5 - 0.5 * (u00 + u01) + bx / media_w;
    const double oy = 0.5 - 0.5 * (u10 + u11) + by / media_h;

    // an unchanged pose maps every coordinate to itself: identity matrix, zero offset
    if (qAbs(u00 - 1) > 1e-6 || qAbs(u11 - 1) > 1e-6
        || qAbs(u01) > 1e-6 || qAbs(u10) > 1e-6
        || qAbs(ox) > 1e-6 || qAbs(oy) > 1e-6) {
      moving = true;
    }

    matrices.append(QVector4D(float(u00), float(u01), float(u10), float(u11)));
    offsets.append(QVector3D(float(ox), float(oy), 1.0f));
  }

  return moving;
}


MotionBlurPose motionblur::ComposePose(const MotionBlurPose& parent, const MotionBlurPose& child) {
  MotionBlurPose result;

  // The anchor stays the child's anchor: composing keeps "screen = L * (p - anchor) + T".
  result.ax = child.ax;
  result.ay = child.ay;

  result.m00 = (parent.m00 * child.m00) + (parent.m01 * child.m10);
  result.m01 = (parent.m00 * child.m01) + (parent.m01 * child.m11);
  result.m10 = (parent.m10 * child.m00) + (parent.m11 * child.m10);
  result.m11 = (parent.m10 * child.m01) + (parent.m11 * child.m11);

  result.tx = (parent.m00 * child.tx) + (parent.m01 * child.ty) + parent.tx;
  result.ty = (parent.m10 * child.tx) + (parent.m11 * child.ty) + parent.ty;

  return result;
}

MotionBlurPose motionblur::FitAffinePose(const double px[4], const double py[4],
                                        const double qx[4], const double qy[4]) {
  // Least squares fit of  screen = L * source + t  over the four corners. The sums below are the
  // mean-removed normal equations of that 2x2 fit.
  double mean_px = 0, mean_py = 0, mean_qx = 0, mean_qy = 0;
  for (int i = 0; i < 4; i++) {
    mean_px += px[i] * 0.25;
    mean_py += py[i] * 0.25;
    mean_qx += qx[i] * 0.25;
    mean_qy += qy[i] * 0.25;
  }

  double sxx = 0, sxy = 0, syy = 0;
  double sx_qx = 0, sx_qy = 0, sy_qx = 0, sy_qy = 0;
  for (int i = 0; i < 4; i++) {
    const double dx = px[i] - mean_px;
    const double dy = py[i] - mean_py;
    const double dqx = qx[i] - mean_qx;
    const double dqy = qy[i] - mean_qy;

    sxx += dx * dx;
    sxy += dx * dy;
    syy += dy * dy;
    sx_qx += dx * dqx;
    sx_qy += dx * dqy;
    sy_qx += dy * dqx;
    sy_qy += dy * dqy;
  }

  MotionBlurPose pose;
  pose.ax = 0;
  pose.ay = 0;

  const double det = (sxx * syy) - (sxy * sxy);
  if (qAbs(det) < 1e-9) {
    // degenerate quad (edge on to the camera) - no usable motion
    pose.m00 = pose.m11 = 0;
    pose.m01 = pose.m10 = 0;
    pose.tx = pose.ty = 0;
    return pose;
  }

  // L = inverse([sxx sxy; sxy syy]) * [sx_qx sx_qy; sy_qx sy_qy]
  pose.m00 = ((syy * sx_qx) - (sxy * sy_qx)) / det;
  pose.m01 = ((sxx * sy_qx) - (sxy * sx_qx)) / det;
  pose.m10 = ((syy * sx_qy) - (sxy * sy_qy)) / det;
  pose.m11 = ((sxx * sy_qy) - (sxy * sx_qy)) / det;

  pose.tx = mean_qx - ((pose.m00 * mean_px) + (pose.m01 * mean_py));
  pose.ty = mean_qy - ((pose.m10 * mean_px) + (pose.m11 * mean_py));

  return pose;
}

bool motionblur::ParentPose(Clip* clip, double playhead, MotionBlurPose& pose) {
  double matrix[16];

  if (clip == nullptr || !build_parent_matrix(clip, playhead, matrix)) {
    return false;
  }

  // build_parent_matrix() returns a column-major 4x4 in sequence pixels, origin at the sequence
  // centre, which is exactly the pose convention.
  pose.ax = 0;
  pose.ay = 0;
  pose.m00 = matrix[0];
  pose.m10 = matrix[1];
  pose.m01 = matrix[4];
  pose.m11 = matrix[5];
  pose.tx = matrix[12];
  pose.ty = matrix[13];

  return true;
}
