#include "richtexteffect.h"

#include <QTextDocument>
#include <QtMath>

#include "timeline/clip.h"
#include "textrender.h"

enum AutoscrollDirection {
  SCROLL_OFF,
  SCROLL_UP,
  SCROLL_DOWN,
  SCROLL_LEFT,
  SCROLL_RIGHT,
};

RichTextEffect::RichTextEffect(Clip *c, const EffectMeta *em) :
  Effect(c, em)
{
  SetFlags(Effect::SuperimposeFlag);

  EffectRow* text_row = new EffectRow(this, tr("Text"));
  text_val = new StringField(text_row, "text");
  text_val->SetColumnSpan(2);

  EffectRow* padding_row = new EffectRow(this, tr("Padding"));
  padding_field = new DoubleField(padding_row, "padding");
  padding_field->SetColumnSpan(2);

  EffectRow* position_row = new EffectRow(this, tr("Position"));
  position_x = new DoubleField(position_row, "posx");
  position_y = new DoubleField(position_row, "posy");

  EffectRow* vertical_align_row = new EffectRow(this, tr("Vertical Align:"));
  vertical_align = new ComboField(vertical_align_row, "valign");
  vertical_align->AddItem(tr("Top"), Qt::AlignTop);
  vertical_align->AddItem(tr("Center"), Qt::AlignCenter);
  vertical_align->AddItem(tr("Bottom"), Qt::AlignBottom);
  vertical_align->SetValueAt(0, Qt::AlignCenter);
  vertical_align->SetColumnSpan(2);

  EffectRow* autoscroll_row = new EffectRow(this, tr("Auto-Scroll"));
  autoscroll = new ComboField(autoscroll_row, "autoscroll");
  autoscroll->AddItem(tr("Off"), SCROLL_OFF);
  autoscroll->AddItem(tr("Up"), SCROLL_UP);
  autoscroll->AddItem(tr("Down"), SCROLL_DOWN);
  autoscroll->AddItem(tr("Left"), SCROLL_LEFT);
  autoscroll->AddItem(tr("Right"), SCROLL_RIGHT);
  autoscroll->SetColumnSpan(2);

  EffectRow* shadow_row = new EffectRow(this, tr("Shadow"));
  shadow_bool = new BoolField(shadow_row, "shadow");
  shadow_bool->SetColumnSpan(2);

  EffectRow* shadow_color_row = new EffectRow(this, tr("Shadow Color"));
  shadow_color = new ColorField(shadow_color_row, "shadowcolor");
  shadow_color->SetColumnSpan(2);

  EffectRow* shadow_angle_row = new EffectRow(this, tr("Shadow Angle"));
  shadow_angle = new DoubleField(shadow_angle_row, "shadowangle");
  shadow_angle->SetColumnSpan(2);

  EffectRow* shadow_distance_row = new EffectRow(this, tr("Shadow Distance"));
  shadow_distance = new DoubleField(shadow_distance_row, "shadowdistance");
  shadow_distance->SetColumnSpan(2);
  shadow_distance->SetMinimum(0);

  EffectRow* shadow_softness_row = new EffectRow(this, tr("Shadow Softness"));
  shadow_softness = new DoubleField(shadow_softness_row, "shadowsoftness");
  shadow_softness->SetColumnSpan(2);
  shadow_softness->SetMinimum(0);

  EffectRow* shadow_opacity_row = new EffectRow(this, tr("Shadow Opacity"));
  shadow_opacity = new DoubleField(shadow_opacity_row, "shadowopacity");
  shadow_opacity->SetColumnSpan(2);
  shadow_opacity->SetMinimum(0);
  shadow_opacity->SetMaximum(100);

  // Gradient fill and outline. These are appended after the shadow rows because saved projects
  // match an effect's rows by position.
  EffectRow* gradient_row = new EffectRow(this, tr("Gradient Fill"));
  gradient_bool = new BoolField(gradient_row, "gradient");
  gradient_bool->SetColumnSpan(2);

  EffectRow* gradient_colors_row = new EffectRow(this, tr("Gradient Colors"));
  gradient_color_a = new ColorField(gradient_colors_row, "gradientcolora");
  gradient_color_b = new ColorField(gradient_colors_row, "gradientcolorb");

  EffectRow* gradient_angle_row = new EffectRow(this, tr("Gradient Angle"));
  gradient_angle = new DoubleField(gradient_angle_row, "gradientangle");
  gradient_angle->SetColumnSpan(2);

  EffectRow* outline_row = new EffectRow(this, tr("Outline"));
  outline_bool = new BoolField(outline_row, "outline");
  outline_bool->SetColumnSpan(2);

  EffectRow* outline_color_row = new EffectRow(this, tr("Outline Color"));
  outline_color = new ColorField(outline_color_row, "outlinecolor");
  outline_color->SetColumnSpan(2);

  EffectRow* outline_width_row = new EffectRow(this, tr("Outline Width"));
  outline_width = new DoubleField(outline_width_row, "outlinewidth");
  outline_width->SetColumnSpan(2);
  outline_width->SetMinimum(0);

  // defaults
  shadow_angle->SetDefault(45);
  shadow_distance->SetDefault(6);
  shadow_softness->SetDefault(4);
  shadow_opacity->SetDefault(60);
  gradient_color_a->SetValueAt(0, QColor(255, 255, 255));
  gradient_color_b->SetValueAt(0, QColor(120, 190, 255));
  gradient_angle->SetDefault(90);
  outline_color->SetValueAt(0, QColor(0, 0, 0));
  outline_width->SetDefault(3);

  // Create default text
  text_val->SetValueAt(0, "<html>"
                            "<body style=\"color: #ffffff; font-size: 36pt;\">"
                              "<center>Sample Text</center>"
                            "</body>"
                          "</html>");
}

void RichTextEffect::PrepareDocument(QTextDocument&, double)
{
}

void RichTextEffect::redraw(double timecode)
{
  int width = img.width();
  int height = img.height();

  int padding = qRound(padding_field->GetDoubleAt(timecode));

  width -= 2 * padding;
  height -= 2 * padding;

  QTextDocument td;
  td.setHtml(text_val->GetStringAt(timecode));

  // subclasses may force formatting (e.g. a font chosen in the effect panel) before layout
  PrepareDocument(td, timecode);

  td.setTextWidth(qMax(1, width));

  int translate_x = qRound(position_x->GetDoubleAt(timecode) + padding);
  int translate_y = qRound(position_y->GetDoubleAt(timecode) + padding);

  int doc_height = qRound(td.size().height());

  AutoscrollDirection auto_scroll_dir = static_cast<AutoscrollDirection>(autoscroll->GetValueAt(timecode).toInt());

  double scroll_progress = 0;

  if (auto_scroll_dir != SCROLL_OFF) {
    double clip_length_secs = double(parent_clip->length()) / parent_clip->media_frame_rate();
    scroll_progress = (timecode - double(parent_clip->clip_in()) / parent_clip->media_frame_rate()) / clip_length_secs;
  }

  if (auto_scroll_dir == SCROLL_OFF || auto_scroll_dir == SCROLL_LEFT || auto_scroll_dir == SCROLL_RIGHT) {

    // If we're not auto-scrolling the vertical direction, respect the vertical alignment
    if (vertical_align->GetValueAt(timecode).toInt() == Qt::AlignCenter) {
      translate_y += height / 2 - doc_height / 2;
    } else if (vertical_align->GetValueAt(timecode).toInt() == Qt::AlignBottom) {
      translate_y += height - doc_height;
    }

    // Check if we are autoscrolling
    if (auto_scroll_dir != SCROLL_OFF) {

      if (auto_scroll_dir == SCROLL_LEFT) {
        scroll_progress = 1.0 - scroll_progress;
      }

      int doc_width = qRound(td.size().width());
      translate_x += qRound(-doc_width + (img.width() + doc_width) * scroll_progress);
    }

  } else if (auto_scroll_dir == SCROLL_UP || auto_scroll_dir == SCROLL_DOWN) {

    // Auto-scroll bottom to top or top to bottom

    if (auto_scroll_dir == SCROLL_UP) {
      scroll_progress = 1.0 - scroll_progress;
    }

    translate_y += qRound(-doc_height + (img.height() + doc_height)*scroll_progress);

  }

  img.fill(Qt::transparent);

  // Fill / outline / shadow styling. The text itself is converted to glyph outlines by the renderer,
  // so the fill can be a gradient, the outline is a real stroke around the letters and the shadow is
  // the same silhouette offset and blurred.
  TextViewStyle style;
  style.gradient = gradient_bool->GetBoolAt(timecode);
  style.gradient_start = gradient_color_a->GetColorAt(timecode);
  style.gradient_end = gradient_color_b->GetColorAt(timecode);
  style.gradient_angle = gradient_angle->GetDoubleAt(timecode);
  style.outline = outline_bool->GetBoolAt(timecode);
  style.outline_color = outline_color->GetColorAt(timecode);
  style.outline_width = outline_width->GetDoubleAt(timecode);
  style.shadow = shadow_bool->GetBoolAt(timecode);
  style.shadow_color = shadow_color->GetColorAt(timecode);
  style.shadow_angle = shadow_angle->GetDoubleAt(timecode);
  style.shadow_distance = shadow_distance->GetDoubleAt(timecode);
  style.shadow_softness = shadow_softness->GetDoubleAt(timecode);
  style.shadow_opacity = shadow_opacity->GetDoubleAt(timecode);

  olive::text::RenderStyled(img, td, QPoint(translate_x, translate_y), img.rect(), style);
}

bool RichTextEffect::AlwaysUpdate()
{
  return autoscroll->GetValueAt(autoscroll->Now()).toInt() != SCROLL_OFF;
}
