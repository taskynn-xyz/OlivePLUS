#ifndef TEXTRENDER_H
#define TEXTRENDER_H

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QString>

class QTextDocument;

/**
 * @brief Styling options for rendering rich text as a title
 *
 * These are the "look" options that apply to a whole block of text: a solid or gradient fill, an
 * outline and a drop shadow. Per-character formatting (font, size, weight, colour) still comes from
 * the rich text document itself.
 */
struct TextViewStyle {
  TextViewStyle();

  // Fill
  bool gradient;
  QColor gradient_start;
  QColor gradient_end;
  double gradient_angle;

  // Outline
  bool outline;
  QColor outline_color;
  double outline_width;

  // Shadow
  bool shadow;
  QColor shadow_color;
  double shadow_angle;
  double shadow_distance;
  double shadow_softness;
  double shadow_opacity;
};

namespace olive {
namespace text {

/**
 * @brief Render a laid out QTextDocument into an image using a TextViewStyle
 *
 * The text is converted into glyph outlines (via QRawFont) instead of being drawn straight to the
 * image, which is what makes the extra looks possible: the shadow is the blurred outline offset,
 * the outline is the same outline stroked, and the fill can be a gradient that runs across the
 * whole text instead of one colour per character. The document still handles shaping, kerning,
 * line breaking and alignment.
 *
 * @param target
 *
 * Image to draw into. Its existing contents are kept (the caller clears it when needed).
 *
 * @param doc
 *
 * Document to draw. It must already be laid out at the intended width.
 *
 * @param offset
 *
 * Where the top left of the document goes, in target pixels.
 *
 * @param clip
 *
 * Rectangle (in target pixels) the text is clipped to.
 *
 * @param style
 *
 * Fill/outline/shadow styling.
 */
void RenderStyled(QImage& target, QTextDocument& doc, const QPoint& offset, const QRect& clip,
                  const TextViewStyle& style);

}
}

#endif // TEXTRENDER_H
