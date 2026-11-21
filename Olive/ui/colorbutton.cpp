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

#include "colorbutton.h"

#include <QColorDialog>
#include <QString>

ColorButton::ColorButton(QWidget *parent)
  : QPushButton(parent), color(Qt::white) {
  set_button_color();
  connect(this, SIGNAL(clicked(bool)), this, SLOT(open_dialog()));
}

QColor ColorButton::get_color() {
  return color;
}

void ColorButton::set_color(QColor c) {
  color = c;
  set_button_color();
}

void ColorButton::set_button_color() {
  // The application-wide Fluent stylesheet paints every QPushButton background itself, so the
  // palette colour this button used to set was never drawn and the swatch stayed blank in the effect
  // controls. A widget-level stylesheet overrides the application one, so the button always shows
  // the current colour.
  const QColor border = (color.lightness() > 127) ? color.darker(140) : color.lighter(160);

  setStyleSheet(QString("QPushButton {"
                        " background-color: rgba(%1, %2, %3, %4);"
                        " border: 1px solid %5;"
                        " border-radius: 3px;"
                        " min-height: 18px;"
                        "}"
                        "QPushButton:hover {"
                        " border: 1px solid #0078d4;"
                        "}"
                        "QPushButton:disabled {"
                        " background-color: rgba(%1, %2, %3, 60);"
                        "}")
                .arg(color.red())
                .arg(color.green())
                .arg(color.blue())
                .arg(color.alpha() / 255.0, 0, 'f', 3)
                .arg(border.name()));
}

void ColorButton::open_dialog() {
  QColor new_color = QColorDialog::getColor(color, nullptr, tr("Set Color"));
  if (new_color.isValid() && color != new_color) {
    set_color(new_color);
    set_button_color();
    emit color_changed(color);
  }
}
