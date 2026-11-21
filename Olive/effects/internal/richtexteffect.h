#ifndef RICHTEXTEFFECT_H
#define RICHTEXTEFFECT_H

#include "effects/effect.h"

class QTextDocument;

class RichTextEffect : public Effect {
  Q_OBJECT
public:
  RichTextEffect(Clip* c, const EffectMeta *em);
  void redraw(double timecode);
protected:
  virtual bool AlwaysUpdate() override;

  /**
   * @brief Hook for subclasses to force formatting onto the document before it is rendered
   *
   * Called with the document built from the Text field, before it is laid out. A subclass that
   * exposes its own font/size/colour controls (see TitleEffect) merges them here, so the rendered
   * title takes its formatting from the effect panel rather than only from the stored HTML.
   */
  virtual void PrepareDocument(QTextDocument& doc, double timecode);
private:
  StringField* text_val;
  DoubleField* padding_field;
  DoubleField* position_x;
  DoubleField* position_y;
  ComboField* vertical_align;
  ComboField* autoscroll;

  BoolField* shadow_bool;
  DoubleField* shadow_angle;
  DoubleField* shadow_distance;
  ColorField* shadow_color;
  DoubleField* shadow_softness;
  DoubleField* shadow_opacity;

  // Added after the shadow rows so existing projects still match their rows by position.
  BoolField* gradient_bool;
  ColorField* gradient_color_a;
  ColorField* gradient_color_b;
  DoubleField* gradient_angle;

  BoolField* outline_bool;
  ColorField* outline_color;
  DoubleField* outline_width;
};

#endif // RICHTEXTEFFECT_H
