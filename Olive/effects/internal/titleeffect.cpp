#include "titleeffect.h"

#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QtMath>

TitleEffect::TitleEffect(Clip* c, const EffectMeta* em) : RichTextEffect(c, em) {
  // Appended after the rows the Rich Text effect creates (text, layout, gradient, outline, shadow).
  EffectRow* font_row = new EffectRow(this, tr("Font"));
  font_field = new FontField(font_row, "font");
  font_field->SetColumnSpan(2);

  EffectRow* size_row = new EffectRow(this, tr("Size"));
  size_field = new DoubleField(size_row, "size");
  size_field->SetMinimum(1);

  EffectRow* bold_row = new EffectRow(this, tr("Bold"));
  bold_field = new BoolField(bold_row, "bold");
  bold_field->SetColumnSpan(2);

  EffectRow* italic_row = new EffectRow(this, tr("Italic"));
  italic_field = new BoolField(italic_row, "italic");
  italic_field->SetColumnSpan(2);

  EffectRow* color_row = new EffectRow(this, tr("Color"));
  color_field = new ColorField(color_row, "color");
  color_field->SetColumnSpan(2);

  // Default to the application's own font so a new title looks familiar before it is styled.
  const QFont default_font;
  if (!default_font.family().isEmpty()) {
    font_field->SetValueAt(0, default_font.family());
  }
  size_field->SetDefault(36);
  color_field->SetValueAt(0, QColor(Qt::white));
}

void TitleEffect::PrepareDocument(QTextDocument& doc, double timecode) {
  QTextCharFormat format;

  const QString family = font_field->GetFontAt(timecode);
  if (!family.isEmpty()) {
    format.setFontFamily(family);
  }
  format.setFontPointSize(qMax(1.0, size_field->GetDoubleAt(timecode)));
  format.setFontWeight(bold_field->GetBoolAt(timecode) ? QFont::Bold : QFont::Normal);
  format.setFontItalic(italic_field->GetBoolAt(timecode));
  format.setForeground(color_field->GetColorAt(timecode));

  // merge over the whole document: characters that arrived with their own family (pasted HTML, a
  // font set in the Edit Text dialog) are re-styled to the panel's settings
  QTextCursor cursor(&doc);
  cursor.select(QTextCursor::Document);
  cursor.mergeCharFormat(format);
}
