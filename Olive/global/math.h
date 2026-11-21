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

#ifndef MATH_H
#define MATH_H

#include <QRect>

int lerp(int a, int b, double t);
float float_lerp(float a, float b, float t);
double double_lerp(double a, double b, double t);
double quad_from_t(double a, double b, double c, double t);
double quad_t_from_x(double x, double a, double b, double c);
double cubic_from_t(double a, double b, double c, double d, double t);
double cubic_t_from_x(double x_target, double a, double b, double c, double d);
double solveCubicBezier(double p0, double p1, double p2, double p3, double x);

QRect fit_size_into_rect(const QRect& r, int width, int height);

// decibel conversion functions
double amplitude_to_db(double amplitude);
double db_to_amplitude(double db);

// 4x4 matrices in column-major order, the layout glMultMatrixd() and OpenGL expect. The
// post-multiplying helpers below match the order of the equivalent glTranslate()/glRotate()/
// glScale() calls, so a transform can be reproduced exactly on the CPU.
void mat4_identity(double* m);
void mat4_multiply(const double* a, const double* b, double* out);
void mat4_translate(double* m, double x, double y, double z);
void mat4_rotate_x(double* m, double degrees);
void mat4_rotate_y(double* m, double degrees);
void mat4_rotate_z(double* m, double degrees);
void mat4_scale(double* m, double sx, double sy);

/**
 * @brief Transform a 2D point by a 4x4 matrix, including the perspective divide
 *
 * Used to bring a parented clip's geometry into the space its parent transformed it into.
 */
void mat4_transform_point(const double* m, double x, double y, double* out_x, double* out_y);

/**
 * @brief Transform a 3D point by a 4x4 matrix, without a perspective divide
 *
 * The matrices the 3D Transform builds for parenting are rigid (translation, rotation, scale), so
 * the homogenizing weight stays 1; the divide is done by the camera afterwards.
 */
void mat4_transform_point_3d(const double* m, double x, double y, double z,
                             double* out_x, double* out_y, double* out_z);

#endif // MATH_H
