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

#include "shapeeffect.h"

#include <QPainter>
#include <QLinearGradient>
#include <QPen>
#include <QtMath>

#include "timeline/clip.h"
#include "timeline/sequence.h"

namespace {

/**
 * @brief Box blur one row of an RGBA image, using a sliding window sum
 *
 * Linear in the number of pixels (rather than pixels * radius), because the feather runs on the
 * full frame and has to stay interactive while the user drags a slider.
 */
void blur_rows(const QImage& src, QImage& dst, int radius) {
  const int w = src.width();
  const int h = src.height();
  const int window = (radius * 2) + 1;

  for (int y=0;y<h;y++) {
    const uchar* in = src.constScanLine(y);
    uchar* out = dst.scanLine(y);

    int sum[4] = { 0, 0, 0, 0 };
    for (int k=-radius;k<=radius;k++) {
      const uchar* pixel = in + (qBound(0, k, w-1) * 4);
      for (int c=0;c<4;c++) {
        sum[c] += pixel[c];
      }
    }

    for (int x=0;x<w;x++) {
      for (int c=0;c<4;c++) {
        out[(x*4)+c] = uchar(sum[c] / window);
      }

      const uchar* add = in + (qBound(0, x + radius + 1, w-1) * 4);
      const uchar* sub = in + (qBound(0, x - radius, w-1) * 4);

      for (int c=0;c<4;c++) {
        sum[c] += add[c] - sub[c];
      }
    }
  }
}

/// Same as blur_rows() but down the columns
void blur_columns(const QImage& src, QImage& dst, int radius) {
  const int w = src.width();
  const int h = src.height();
  const int window = (radius * 2) + 1;

  for (int x=0;x<w;x++) {
    int sum[4] = { 0, 0, 0, 0 };

    for (int k=-radius;k<=radius;k++) {
      const uchar* pixel = src.constScanLine(qBound(0, k, h-1)) + (x * 4);
      for (int c=0;c<4;c++) {
        sum[c] += pixel[c];
      }
    }

    for (int y=0;y<h;y++) {
      uchar* out = dst.scanLine(y) + (x * 4);
      for (int c=0;c<4;c++) {
        out[c] = uchar(sum[c] / window);
      }

      const uchar* add = src.constScanLine(qBound(0, y + radius + 1, h-1)) + (x * 4);
      const uchar* sub = src.constScanLine(qBound(0, y - radius, h-1)) + (x * 4);

      for (int c=0;c<4;c++) {
        sum[c] += add[c] - sub[c];
      }
    }
  }
}

}

ShapeEffect::ShapeEffect(Clip* c, const EffectMeta* em) : Effect(c, em) {
  SetFlags(Effect::SuperimposeFlag);

  EffectRow* type_row = new EffectRow(this, tr("Shape"));
  shape_type_ = new ComboField(type_row, "type");
  shape_type_->AddItem(tr("Rectangle"), int(SHAPE_RECTANGLE));
  shape_type_->AddItem(tr("Ellipse"), int(SHAPE_ELLIPSE));
  shape_type_->AddItem(tr("Polygon"), int(SHAPE_POLYGON));
  shape_type_->AddItem(tr("Star"), int(SHAPE_STAR));
  shape_type_->AddItem(tr("Line"), int(SHAPE_LINE));

  EffectRow* size_row = new EffectRow(this, tr("Size"));
  width_ = new DoubleField(size_row, "width");
  width_->SetMinimum(0);
  height_ = new DoubleField(size_row, "height");
  height_->SetMinimum(0);

  EffectRow* position_row = new EffectRow(this, tr("Position"));
  position_x_ = new DoubleField(position_row, "posx");
  position_y_ = new DoubleField(position_row, "posy");

  EffectRow* rotation_row = new EffectRow(this, tr("Rotation"));
  rotation_ = new DoubleField(rotation_row, "rotation");

  EffectRow* corner_row = new EffectRow(this, tr("Corner Radius"));
  corner_radius_ = new DoubleField(corner_row, "cornerradius");
  corner_radius_->SetMinimum(0);

  EffectRow* points_row = new EffectRow(this, tr("Points"));
  points_ = new DoubleField(points_row, "points");
  points_->SetMinimum(3);
  points_->SetMaximum(64);

  EffectRow* inner_row = new EffectRow(this, tr("Star Inner Radius"));
  inner_radius_ = new DoubleField(inner_row, "innerradius");
  inner_radius_->SetMinimum(0);
  inner_radius_->SetMaximum(100);

  EffectRow* fill_row = new EffectRow(this, tr("Fill"));
  fill_enabled_ = new BoolField(fill_row, "fill");
  fill_color_ = new ColorField(fill_row, "fillcolor");

  EffectRow* gradient_row = new EffectRow(this, tr("Gradient"));
  gradient_enabled_ = new BoolField(gradient_row, "gradient");

  EffectRow* gradient_colors_row = new EffectRow(this, tr("Gradient Colors"));
  gradient_color_a_ = new ColorField(gradient_colors_row, "gradienta");
  gradient_color_b_ = new ColorField(gradient_colors_row, "gradientb");

  EffectRow* gradient_angle_row = new EffectRow(this, tr("Gradient Angle"));
  gradient_angle_ = new DoubleField(gradient_angle_row, "gradientangle");
  gradient_angle_->SetMinimum(-360);
  gradient_angle_->SetMaximum(360);

  EffectRow* stroke_row = new EffectRow(this, tr("Stroke"));
  stroke_enabled_ = new BoolField(stroke_row, "stroke");

  EffectRow* stroke_color_row = new EffectRow(this, tr("Stroke Color"));
  stroke_color_ = new ColorField(stroke_color_row, "strokecolor");

  EffectRow* stroke_width_row = new EffectRow(this, tr("Stroke Width"));
  stroke_width_ = new DoubleField(stroke_width_row, "strokewidth");
  stroke_width_->SetMinimum(0);

  EffectRow* trim_start_row = new EffectRow(this, tr("Trim Start"));
  trim_start_ = new DoubleField(trim_start_row, "trimstart");
  trim_start_->SetMinimum(0);
  trim_start_->SetMaximum(100);

  EffectRow* trim_end_row = new EffectRow(this, tr("Trim End"));
  trim_end_ = new DoubleField(trim_end_row, "trimend");
  trim_end_->SetMinimum(0);
  trim_end_->SetMaximum(100);

  EffectRow* trim_offset_row = new EffectRow(this, tr("Trim Offset"));
  trim_offset_ = new DoubleField(trim_offset_row, "trimoffset");
  trim_offset_->SetMinimum(-360);
  trim_offset_->SetMaximum(360);

  EffectRow* opacity_row = new EffectRow(this, tr("Opacity"));
  opacity_ = new DoubleField(opacity_row, "opacity");
  opacity_->SetMinimum(0);
  opacity_->SetMaximum(100);

  EffectRow* feather_row = new EffectRow(this, tr("Feather"));
  feather_ = new DoubleField(feather_row, "feather");
  feather_->SetMinimum(0);
  feather_->SetMaximum(200);

  // defaults (the size and position defaults come from the sequence, see refresh())
  shape_type_->SetValueAt(0, int(SHAPE_RECTANGLE));
  rotation_->SetDefault(0);
  corner_radius_->SetDefault(0);
  points_->SetDefault(5);
  inner_radius_->SetDefault(50);
  fill_enabled_->SetValueAt(0, true);
  fill_color_->SetValueAt(0, QColor(255, 255, 255));
  gradient_enabled_->SetValueAt(0, false);
  gradient_color_a_->SetValueAt(0, QColor(64, 156, 255));
  gradient_color_b_->SetValueAt(0, QColor(255, 64, 159));
  gradient_angle_->SetDefault(0);
  stroke_enabled_->SetValueAt(0, false);
  stroke_color_->SetValueAt(0, QColor(0, 0, 0));
  stroke_width_->SetDefault(4);
  trim_start_->SetDefault(0);
  trim_end_->SetDefault(100);
  trim_offset_->SetDefault(0);
  opacity_->SetDefault(100);
  feather_->SetDefault(0);

  connect(shape_type_, SIGNAL(DataChanged(const QVariant&)), this, SLOT(update_ui()));
  connect(gradient_enabled_, SIGNAL(Toggled(bool)), this, SLOT(update_ui()));
  connect(stroke_enabled_, SIGNAL(Toggled(bool)), this, SLOT(update_ui()));

  refresh();
  update_ui();
}

void ShapeEffect::refresh() {
  if (parent_clip != nullptr && parent_clip->sequence != nullptr) {
    width_->SetDefault(parent_clip->sequence->width / 3.0);
    height_->SetDefault(parent_clip->sequence->height / 3.0);
    position_x_->SetDefault(parent_clip->sequence->width / 2.0);
    position_y_->SetDefault(parent_clip->sequence->height / 2.0);
  }
}

void ShapeEffect::update_ui() {
  const int type = shape_type_->GetValueAt(0).toInt();

  corner_radius_->SetEnabled(type == SHAPE_RECTANGLE);
  points_->SetEnabled(type == SHAPE_POLYGON || type == SHAPE_STAR);
  inner_radius_->SetEnabled(type == SHAPE_STAR);

  const bool gradient = gradient_enabled_->GetValueAt(0).toBool();
  fill_color_->SetEnabled(!gradient);
  gradient_color_a_->SetEnabled(gradient);
  gradient_color_b_->SetEnabled(gradient);
  gradient_angle_->SetEnabled(gradient);

  const bool stroke = stroke_enabled_->GetValueAt(0).toBool();
  stroke_color_->SetEnabled(stroke);
  stroke_width_->SetEnabled(stroke);
  trim_start_->SetEnabled(stroke);
  trim_end_->SetEnabled(stroke);
  trim_offset_->SetEnabled(stroke);
}

QPainterPath ShapeEffect::build_path(int type, double shape_w, double shape_h,
                                     double corner_radius, int point_count,
                                     double inner_percent) const {
  QPainterPath path;

  const double half_w = shape_w / 2.0;
  const double half_h = shape_h / 2.0;

  switch (type) {
  case SHAPE_RECTANGLE:
    if (corner_radius > 0.0) {
      const double radius = qMin(corner_radius, qMin(half_w, half_h));
      path.addRoundedRect(QRectF(-half_w, -half_h, shape_w, shape_h), radius, radius);
    } else {
      path.addRect(QRectF(-half_w, -half_h, shape_w, shape_h));
    }
    break;
  case SHAPE_ELLIPSE:
    path.addEllipse(QPointF(0, 0), half_w, half_h);
    break;
  case SHAPE_POLYGON:
  case SHAPE_STAR:
  {
    const int count = qBound(3, point_count, 64);
    const double inner = qBound(0.0, inner_percent, 100.0) / 100.0;
    const int total = (type == SHAPE_STAR) ? count * 2 : count;

    for (int i=0;i<total;i++) {
      // start at the top so a polygon points upwards by default
      const double angle = (-M_PI / 2.0) + ((2.0 * M_PI * i) / total);
      const double scale = (type == SHAPE_STAR && (i % 2) == 1) ? inner : 1.0;

      const double x = qCos(angle) * half_w * scale;
      const double y = qSin(angle) * half_h * scale;

      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
    }
    path.closeSubpath();
  }
    break;
  case SHAPE_LINE:
    path.moveTo(-half_w, 0);
    path.lineTo(half_w, 0);
    break;
  }

  return path;
}

QPainterPath ShapeEffect::trim_path(const QPainterPath& path, double start_pct, double end_pct,
                                    double offset_pct) const {
  if (path.isEmpty()) {
    return path;
  }

  const double length = (end_pct - start_pct) / 100.0;

  // nothing to trim: the whole outline is stroked
  if (length <= 0.0) {
    return QPainterPath();
  }
  if (length >= 0.999) {
    return path;
  }

  double start = (start_pct + offset_pct) / 100.0;
  start -= qFloor(start);

  QPainterPath result;

  // the outline is sampled rather than split, which keeps this working for every shape type
  const int steps = qMax(8, qRound(360.0 * length));

  for (int i=0;i<=steps;i++) {
    double t = start + (length * double(i) / double(steps));
    t -= qFloor(t);

    const QPointF point = path.pointAtPercent(t);

    if (i == 0) {
      result.moveTo(point);
    } else {
      result.lineTo(point);
    }
  }

  return result;
}

void ShapeEffect::feather_image(int radius) {
  if (radius < 1) {
    return;
  }

  // three box passes approximate a gaussian, which reads as a soft edge
  const int passes = 3;
  const int pass_radius = qMax(1, radius / passes);

  for (int i=0;i<passes;i++) {
    QImage source = img.copy();
    blur_rows(source, img, pass_radius);

    source = img.copy();
    blur_columns(source, img, pass_radius);
  }
}

void ShapeEffect::redraw(double timecode) {
  const int w = img.width();
  const int h = img.height();

  img.fill(Qt::transparent);

  if (w <= 0 || h <= 0 || parent_clip == nullptr) {
    return;
  }

  const int type = shape_type_->GetValueAt(timecode).toInt();

  const double shape_w = qMax(0.0, width_->GetDoubleAt(timecode));
  const double shape_h = qMax(0.0, height_->GetDoubleAt(timecode));

  QPainterPath path = build_path(type, shape_w, shape_h,
                                 corner_radius_->GetDoubleAt(timecode),
                                 qRound(points_->GetDoubleAt(timecode)),
                                 inner_radius_->GetDoubleAt(timecode));

  if (path.isEmpty()) {
    return;
  }

  const double seq_w = (parent_clip->sequence != nullptr) ? parent_clip->sequence->width : w;
  const double seq_h = (parent_clip->sequence != nullptr) ? parent_clip->sequence->height : h;

  // position is in sequence pixels with the sequence centre as the origin, like Transform
  const double position_x = position_x_->GetDoubleAt(timecode) - (seq_w / 2.0) + (w / 2.0);
  const double position_y = position_y_->GetDoubleAt(timecode) - (seq_h / 2.0) + (h / 2.0);

  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setOpacity(qBound(0.0, opacity_->GetDoubleAt(timecode) * 0.01, 1.0));

  p.translate(position_x, position_y);
  p.rotate(rotation_->GetDoubleAt(timecode));

  if (fill_enabled_->GetValueAt(timecode).toBool()) {
    if (gradient_enabled_->GetValueAt(timecode).toBool()) {
      const double angle = qDegreesToRadians(gradient_angle_->GetDoubleAt(timecode));
      const double length = qMax(shape_w, shape_h) / 2.0;
      const double dir_x = qCos(angle) * length;
      const double dir_y = qSin(angle) * length;

      QLinearGradient gradient(-dir_x, -dir_y, dir_x, dir_y);
      gradient.setColorAt(0.0, gradient_color_a_->GetColorAt(timecode));
      gradient.setColorAt(1.0, gradient_color_b_->GetColorAt(timecode));

      p.fillPath(path, QBrush(gradient));
    } else {
      p.fillPath(path, QBrush(fill_color_->GetColorAt(timecode)));
    }
  }

  if (stroke_enabled_->GetValueAt(timecode).toBool()) {
    const double stroke_width_value = stroke_width_->GetDoubleAt(timecode);

    if (stroke_width_value > 0.0) {
      const QPainterPath stroke_path = trim_path(path,
                                                 trim_start_->GetDoubleAt(timecode),
                                                 trim_end_->GetDoubleAt(timecode),
                                                 trim_offset_->GetDoubleAt(timecode));

      if (!stroke_path.isEmpty()) {
        QPen pen(stroke_color_->GetColorAt(timecode), stroke_width_value,
                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(stroke_path);
      }
    }
  }

  p.end();

  feather_image(qRound(feather_->GetDoubleAt(timecode)));
}
