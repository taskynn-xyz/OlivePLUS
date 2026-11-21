#include "textrender.h"

#include <QAbstractTextDocumentLayout>
#include <QGlyphRun>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QRawFont>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QtMath>

#include "ui/blur.h"

namespace {

struct TextGlyphRun {
  QPainterPath path;
  QColor color;
  bool underline;
  bool strikeout;
  QRectF bounds;
};

// Collects every glyph of the document as a painter path, grouped per text run so that each run can
// keep its own colour and decorations.
QVector<TextGlyphRun> CollectRuns(QTextDocument& doc) {
  QVector<TextGlyphRun> runs;

  QAbstractTextDocumentLayout* layout = doc.documentLayout();
  if (layout == nullptr) {
    return runs;
  }

  // Asking for the document size forces the layout, so every block/line has been laid out by the
  // time the glyph runs are read below.
  layout->documentSize();

  for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
    QTextLayout* block_layout = block.layout();
    if (block_layout == nullptr) {
      continue;
    }

    const QPointF block_offset = layout->blockBoundingRect(block).topLeft();

    for (int i = 0; i < block_layout->lineCount(); i++) {
      const QTextLine line = block_layout->lineAt(i);
      if (!line.isValid()) {
        continue;
      }

      const QPointF line_offset = block_offset + line.position();
      const QList<QGlyphRun> glyph_runs = line.glyphRuns();

      for (int j = 0; j < glyph_runs.size(); j++) {
        const QGlyphRun& glyph_run = glyph_runs.at(j);

        if (glyph_run.isEmpty()) {
          continue;
        }

        const QRawFont raw_font = glyph_run.rawFont();
        if (!raw_font.isValid()) {
          continue;
        }

        const QVector<quint32> indexes = glyph_run.glyphIndexes();
        const QVector<QPointF> positions = glyph_run.positions();

        QPainterPath path;
        for (int k = 0; k < indexes.size() && k < positions.size(); k++) {
          const QPainterPath glyph = raw_font.pathForGlyph(indexes.at(k));
          if (glyph.isEmpty()) {
            // whitespace and other blank glyphs have no outline
            continue;
          }
          path.addPath(glyph.translated(line_offset + positions.at(k)));
        }

        if (path.isEmpty()) {
          continue;
        }

        // Character format of the first character in this run, for colour/underline/strikeout. The
        // run's starting x position is mapped back to a character index in the line (QGlyphRun does
        // not expose the character indexes on every Qt version).
        QTextCharFormat format;
        const int line_character = line.xToCursor(positions.first().x());
        const int character = qBound(0, line.textStart() + line_character + 1, block.length());
        QTextCursor cursor(block);
        cursor.setPosition(block.position() + character);
        format = cursor.charFormat();

        TextGlyphRun run;
        run.path = path;
        run.bounds = path.boundingRect();
        run.color = format.foreground().style() == Qt::NoBrush
            ? QColor(Qt::white)
            : format.foreground().color();
        run.underline = format.fontUnderline();
        run.strikeout = format.fontStrikeOut();
        runs.append(run);
      }
    }
  }

  return runs;
}

QLinearGradient BuildGradient(const QRectF& bounds, double angle, const QColor& start,
                              const QColor& end) {
  const double radians = qDegreesToRadians(angle);
  const double dir_x = qCos(radians);
  const double dir_y = qSin(radians);

  // half of the extent of the text box along the gradient direction, so the gradient always spans
  // the whole text whatever the angle is
  const double half = (qAbs(dir_x) * bounds.width() + qAbs(dir_y) * bounds.height()) / 2.0;
  const QPointF centre = bounds.center();

  QLinearGradient gradient(centre.x() - dir_x * half, centre.y() - dir_y * half,
                           centre.x() + dir_x * half, centre.y() + dir_y * half);
  gradient.setColorAt(0.0, start);
  gradient.setColorAt(1.0, end);
  return gradient;
}

}

TextViewStyle::TextViewStyle() :
  gradient(false),
  gradient_start(255, 255, 255),
  gradient_end(120, 190, 255),
  gradient_angle(90.0),
  outline(false),
  outline_color(0, 0, 0),
  outline_width(3.0),
  shadow(false),
  shadow_color(0, 0, 0),
  shadow_angle(45.0),
  shadow_distance(6.0),
  shadow_softness(4.0),
  shadow_opacity(60.0)
{
}

void olive::text::RenderStyled(QImage& target, QTextDocument& doc, const QPoint& offset,
                               const QRect& clip, const TextViewStyle& style) {
  const QVector<TextGlyphRun> runs = CollectRuns(doc);
  if (runs.isEmpty()) {
    return;
  }

  QPainterPath combined;
  for (int i = 0; i < runs.size(); i++) {
    combined.addPath(runs.at(i).path);
  }
  const QRectF bounds = combined.boundingRect();

  QPainter p(&target);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::SmoothPixmapTransform);

  // Shadow first, then the outline, then the fill on top: the stroke is centred on the glyph edge,
  // so filling afterwards hides the inner half and leaves a clean outline of `outline_width`.
  if (style.shadow && style.shadow_opacity > 0.0) {
    const double angle = qDegreesToRadians(style.shadow_angle);
    const QPointF shadow_offset(qCos(angle) * style.shadow_distance,
                                qSin(angle) * style.shadow_distance);
    const QPainterPath shadow_path = combined.translated(shadow_offset);

    QColor shadow_color = style.shadow_color;
    shadow_color.setAlphaF(float(qBound(0.0, style.shadow_opacity / 100.0, 1.0)));

    const int softness = qRound(style.shadow_softness);
    if (softness > 0) {
      // The shadow needs its own layer so it can be blurred without touching the text.
      QImage layer(target.size(), QImage::Format_ARGB32_Premultiplied);
      layer.fill(Qt::transparent);

      QPainter layer_painter(&layer);
      layer_painter.setRenderHint(QPainter::Antialiasing);
      layer_painter.translate(offset);
      layer_painter.fillPath(shadow_path, shadow_color);
      layer_painter.end();

      olive::ui::blur(layer, layer.rect(), softness, true);

      p.save();
      p.resetTransform();
      p.drawImage(0, 0, layer);
      p.restore();
    } else {
      p.save();
      p.translate(offset);
      p.fillPath(shadow_path, shadow_color);
      p.restore();
    }
  }

  p.save();
  p.translate(offset);
  p.setClipRect(clip.translated(-offset));

  if (style.outline && style.outline_width > 0.0) {
    QPainterPathStroker stroker;
    stroker.setWidth(style.outline_width * 2.0);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    p.setPen(Qt::NoPen);
    p.setBrush(style.outline_color);
    p.drawPath(stroker.createStroke(combined));
  }

  if (style.gradient) {
    // one gradient across the whole title, so every glyph is filled with the same shape
    p.setPen(Qt::NoPen);
    p.setBrush(BuildGradient(bounds, style.gradient_angle, style.gradient_start, style.gradient_end));
    p.drawPath(combined);
  } else {
    for (int i = 0; i < runs.size(); i++) {
      p.fillPath(runs.at(i).path, runs.at(i).color);
    }
  }

  // Underline and strikethrough are not part of the glyph outlines, so they are drawn by hand.
  for (int i = 0; i < runs.size(); i++) {
    const TextGlyphRun& run = runs.at(i);
    if (!run.underline && !run.strikeout) {
      continue;
    }

    const QColor color = style.gradient ? style.gradient_end : run.color;
    const double thickness = qMax(1.0, run.bounds.height() / 18.0);
    const double width = run.bounds.width();

    if (run.underline) {
      p.fillRect(QRectF(run.bounds.left(), run.bounds.bottom(), width, thickness), color);
    }
    if (run.strikeout) {
      const double y = run.bounds.top() + run.bounds.height() * 0.55;
      p.fillRect(QRectF(run.bounds.left(), y, width, thickness), color);
    }
  }

  p.restore();
  p.end();
}
