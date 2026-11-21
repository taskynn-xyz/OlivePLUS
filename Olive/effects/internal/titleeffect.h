#ifndef TITLETEFFECT_H
#define TITLETEFFECT_H

#include "richtexteffect.h"

/**
 * @brief A text generator for titles, with its formatting in the effect panel
 *
 * This is the title generator the Timeline's "Title" command uses. It renders exactly like the Rich
 * Text effect (same text field, gradient, outline and shadow) but exposes Font, Size, Bold, Italic
 * and Color as effect fields and forces them onto the text before rendering. That makes the font in
 * the panel authoritative: it cannot be lost to whatever the stored HTML happens to carry.
 */
class TitleEffect : public RichTextEffect {
  Q_OBJECT
public:
  TitleEffect(Clip* c, const EffectMeta* em);

protected:
  void PrepareDocument(QTextDocument& doc, double timecode) override;

private:
  FontField* font_field;
  DoubleField* size_field;
  BoolField* bold_field;
  BoolField* italic_field;
  ColorField* color_field;
};

#endif // TITLETEFFECT_H
