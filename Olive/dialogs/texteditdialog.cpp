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

#include "texteditdialog.h"

#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTimer>
#include <QSignalBlocker>
#include <QDebug>
#include <QTextCharFormat>
#include <QTextDocument>

#include "ui/icons.h"

TextEditDialog::TextEditDialog(QWidget *parent, const QString &s, bool rich_text) :
  QDialog(parent),
  rich_text_(rich_text)
{
  setWindowTitle(tr("Edit Text"));

  QVBoxLayout* layout = new QVBoxLayout(this);

  // Create central text editor object
  textEdit = new QTextEdit(this);
  textEdit->setUndoRedoEnabled(true);

  // Upper toolbar
  if (rich_text) {
    QHBoxLayout* toolbar = new QHBoxLayout();

    // Italic Button
    italic_button = new QPushButton();
    italic_button->setIcon(olive::icon::CreateIconFromSVG(":/icons/italic.svg", false));
    italic_button->setCheckable(true);
    connect(italic_button, SIGNAL(clicked(bool)), this, SLOT(SetItalic(bool)));
    toolbar->addWidget(italic_button);

    // Underline Button
    underline_button = new QPushButton();
    underline_button->setIcon(olive::icon::CreateIconFromSVG(":/icons/underline.svg", false));
    underline_button->setCheckable(true);
    connect(underline_button, SIGNAL(clicked(bool)), this, SLOT(SetUnderline(bool)));
    toolbar->addWidget(underline_button);

    // Font Name. Non-editable so the combo's current index is always the family the text uses: an
    // editable QFontComboBox only puts text into its line edit when the toolbar syncs it from the
    // cursor, which left the index at -1 and made the combo stop reporting the user's picks.
    font_list = new QFontComboBox();
    font_list->setEditable(false);
    connect(font_list, &QFontComboBox::currentFontChanged, this, &TextEditDialog::SetFontFamily);
    toolbar->addWidget(font_list);

    // Font Weight
    font_weight = new QComboBox();
    font_weight->addItem(tr("Thin"), QFont::Thin);
    font_weight->addItem(tr("Extra Light"), QFont::ExtraLight);
    font_weight->addItem(tr("Light"), QFont::Light);
    font_weight->addItem(tr("Normal"), QFont::Normal);
    font_weight->addItem(tr("Medium"), QFont::Medium);
    font_weight->addItem(tr("Demi Bold"), QFont::DemiBold);
    font_weight->addItem(tr("Bold"), QFont::Bold);
    font_weight->addItem(tr("Extra Bold"), QFont::ExtraBold);
    font_weight->addItem(tr("Black"), QFont::Black);
    connect(font_weight, SIGNAL(currentIndexChanged(int)), this, SLOT(SetFontWeight(int)));
    toolbar->addWidget(font_weight);

    // Font Size
    font_size = new LabelSlider();
    connect(font_size, SIGNAL(valueChanged(double)), this, SLOT(SetFontSize(double)));
    toolbar->addWidget(font_size);

    // Font Color
    font_color = new ColorButton();
    connect(font_color, SIGNAL(color_changed(const QColor&)), this, SLOT(SetFontColor(const QColor&)));
    toolbar->addWidget(font_color);

    toolbar->addStretch();

    // Left Align
    left_align_button = new QPushButton();
    left_align_button->setIcon(olive::icon::CreateIconFromSVG(":/icons/align-left.svg", false));
    left_align_button->setCheckable(true);
    left_align_button->setProperty("a", Qt::AlignLeft);
    connect(left_align_button, SIGNAL(clicked(bool)), this, SLOT(SetAlignmentFromProperty()));
    toolbar->addWidget(left_align_button);

    // Center Align
    center_align_button = new QPushButton();
    center_align_button->setIcon(olive::icon::CreateIconFromSVG(":/icons/align-center.svg", false));
    center_align_button->setCheckable(true);
    center_align_button->setProperty("a", Qt::AlignCenter);
    connect(center_align_button, SIGNAL(clicked(bool)), this, SLOT(SetAlignmentFromProperty()));
    toolbar->addWidget(center_align_button);

    // Right Align
    right_align_button = new QPushButton();
    right_align_button->setIcon(olive::icon::CreateIconFromSVG(":/icons/align-right.svg", false));
    right_align_button->setCheckable(true);
    right_align_button->setProperty("a", Qt::AlignRight);
    connect(right_align_button, SIGNAL(clicked(bool)), this, SLOT(SetAlignmentFromProperty()));
    toolbar->addWidget(right_align_button);

    // Justify Align
    justify_align_button = new QPushButton();
    justify_align_button->setIcon(olive::icon::CreateIconFromSVG(":/icons/justify-center.svg", false));
    justify_align_button->setCheckable(true);
    justify_align_button->setProperty("a", Qt::AlignJustify);
    connect(justify_align_button, SIGNAL(clicked(bool)), this, SLOT(SetAlignmentFromProperty()));
    toolbar->addWidget(justify_align_button);

    layout->addLayout(toolbar);
  }

  layout->addWidget(textEdit);

  // Lower toolbar
  /*
  if (rich_text) {
    QHBoxLayout* lower_toolbar = new QHBoxLayout();

    lower_toolbar->addWidget(new QLabel(tr("Letter Spacing:")));

    letter_spacing = new LabelSlider();
    connect(letter_spacing, SIGNAL(valueChanged(double)), this, SLOT(SetLetterSpacing(qreal)));
    lower_toolbar->addWidget(letter_spacing);

    lower_toolbar->addStretch();

    layout->addLayout(lower_toolbar);
  }
  */

  // Create dialog buttons at the bottom
  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  buttons->setCenterButtons(true);
  layout->addWidget(buttons);
  connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

  // Connect the cursor position changing to the rich text toolbar buttons updating (so for example, when italic text
  // is selected, the italic button will be pressed)
  connect(textEdit, SIGNAL(cursorPositionChanged()), this, SLOT(UpdateUIFromTextCursor()));

  // Set the widget's text based on the rich text mode
  if (rich_text_) {
    textEdit->setHtml(s);
  } else {
    textEdit->setPlainText(s);
  }

  // Helps ensure the UI elements update correctly at the beginning - when the cursor is at the start, the UI elements
  // show up blank. Setting it to the end is probably more expected behavior anyway.
  textEdit->moveCursor(QTextCursor::End);
}

const QString& TextEditDialog::get_string() {
  return result_str;
}

void TextEditDialog::accept() {
  // The document is the single source of truth: every toolbar action merges a character format into
  // it, and the stored title HTML is exactly what the document exports here.
  result_str = rich_text_ ? textEdit->toHtml() : textEdit->toPlainText();
  QDialog::accept();
}

void TextEditDialog::ApplyCharFormat(const QTextCharFormat& format)
{
  QTextCursor cursor = textEdit->textCursor();

  // Without a selection a merge only affects the next typed character, which made picking a font
  // look like it did nothing. Titles are normally styled as a whole, so style the whole document.
  if (!cursor.hasSelection()) {
    cursor.select(QTextCursor::Document);
  }

  cursor.mergeCharFormat(format);

  // keep the choice for anything typed next
  textEdit->mergeCurrentCharFormat(format);
  textEdit->setFocus();
}

void TextEditDialog::SetFontWeight(int i)
{
  QTextCharFormat format;
  format.setFontWeight(font_weight->itemData(i).toInt());
  ApplyCharFormat(format);
}

void TextEditDialog::SetFontFamily(const QFont& f)
{
  if (f.family().isEmpty()) {
    return;
  }

  QTextCharFormat format;
  format.setFontFamily(f.family());
  ApplyCharFormat(format);
}

void TextEditDialog::SetItalic(bool italic)
{
  QTextCharFormat format;
  format.setFontItalic(italic);
  ApplyCharFormat(format);
}

void TextEditDialog::SetUnderline(bool underline)
{
  QTextCharFormat format;
  format.setFontUnderline(underline);
  ApplyCharFormat(format);
}

void TextEditDialog::SetFontSize(double size)
{
  QTextCharFormat format;
  format.setFontPointSize(size);
  ApplyCharFormat(format);
}

void TextEditDialog::SetFontColor(const QColor& color)
{
  QTextCharFormat format;
  format.setForeground(color);
  ApplyCharFormat(format);
}

void TextEditDialog::SetAlignmentFromProperty()
{
  textEdit->setAlignment(static_cast<Qt::Alignment>(sender()->property("a").toInt()));
  UpdateUIFromTextCursor();
}

void TextEditDialog::UpdateUIFromTextCursor()
{
  if (rich_text_) {
    italic_button->setChecked(textEdit->fontItalic());
    underline_button->setChecked(textEdit->fontUnderline());

    // These widgets are only being synchronised to the cursor's formatting. Block their signals so
    // that updating them can't send the value back into the document and undo what the user just
    // chose (which made the font combo appear to do nothing while typing).
    QSignalBlocker font_list_blocker(font_list);
    QSignalBlocker font_weight_blocker(font_weight);
    QSignalBlocker font_size_blocker(font_size);
    QSignalBlocker font_color_blocker(font_color);

    // The cursor's character format can have no family of its own (the family then comes from the
    // document default), so show the default rather than blanking the combo.
    const QString current_family = textEdit->currentCharFormat().fontFamily();
    font_list->setCurrentText(current_family.isEmpty()
                              ? textEdit->document()->defaultFont().family()
                              : current_family);
    font_size->SetValue(textEdit->fontPointSize());
    font_color->set_color(textEdit->textColor());

    for (int i=0;i<font_weight->count();i++) {
      if (font_weight->itemData(i).toInt() == textEdit->fontWeight()) {
        font_weight->setCurrentIndex(i);
        break;
      }
    }

    Qt::Alignment align = textEdit->alignment();
    left_align_button->setChecked(align == Qt::AlignLeft);
    center_align_button->setChecked(align == Qt::AlignCenter);
    right_align_button->setChecked(align == Qt::AlignRight);
    justify_align_button->setChecked(align == Qt::AlignJustify);
  }
}
