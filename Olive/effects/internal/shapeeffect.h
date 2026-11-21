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

#ifndef SHAPEEFFECT_H
#define SHAPEEFFECT_H

#include <QPainterPath>
#include <QImage>

#include "effects/effect.h"

/**
 * @brief A vector shape generator, After Effects style
 *
 * Draws a rectangle, ellipse, polygon, star or line into the clip's frame with QPainter, so the
 * edges stay smooth at any size. Everything about the shape is an effect field and therefore
 * keyframable: size, position, rotation, corner radius, point count, star inner radius, solid or
 * gradient fill, stroke width and colour, an animated trim path, opacity and an edge feather.
 *
 * Like the Solid effect it is a generator: the clip has no media, and the image it paints becomes
 * the clip's texture, so the usual Transform/3D Transform/blur effects still apply on top of it.
 */
class ShapeEffect : public Effect {
  Q_OBJECT
public:
  enum ShapeType {
    SHAPE_RECTANGLE,
    SHAPE_ELLIPSE,
    SHAPE_POLYGON,
    SHAPE_STAR,
    SHAPE_LINE
  };

  ShapeEffect(Clip* c, const EffectMeta* em);

  void refresh() override;
  void redraw(double timecode) override;

private slots:
  /**
   * @brief Enable/disable the fields that don't apply to the current shape type or options
   */
  void update_ui();

private:
  /**
   * @brief Build the shape outline in local coordinates (the shape centred on 0,0)
   */
  QPainterPath build_path(int type, double shape_w, double shape_h, double corner_radius,
                          int point_count, double inner_percent) const;

  /**
   * @brief Cut a path down to the segment between two percentages
   *
   * Used for the trim path option: animating Start/End draws a stroke on, which is a staple of
   * motion-graphics line work. `offset` rotates the whole segment around the path.
   */
  QPainterPath trim_path(const QPainterPath& path, double start_pct, double end_pct,
                         double offset_pct) const;

  /**
   * @brief Soften the shape's edges by blurring the frame's alpha
   */
  void feather_image(int radius);

  ComboField* shape_type_;

  DoubleField* width_;
  DoubleField* height_;
  DoubleField* position_x_;
  DoubleField* position_y_;
  DoubleField* rotation_;
  DoubleField* corner_radius_;

  DoubleField* points_;
  DoubleField* inner_radius_;

  BoolField* fill_enabled_;
  ColorField* fill_color_;
  BoolField* gradient_enabled_;
  ColorField* gradient_color_a_;
  ColorField* gradient_color_b_;
  DoubleField* gradient_angle_;

  BoolField* stroke_enabled_;
  ColorField* stroke_color_;
  DoubleField* stroke_width_;

  DoubleField* trim_start_;
  DoubleField* trim_end_;
  DoubleField* trim_offset_;

  DoubleField* opacity_;
  DoubleField* feather_;
};

#endif // SHAPEEFFECT_H
