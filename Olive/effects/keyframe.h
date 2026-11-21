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

#ifndef KEYFRAME_H
#define KEYFRAME_H

#include <QVariant>
#include <QVector>

class EffectField;

class EffectKeyframe {
public:
  EffectKeyframe();

  int type;
  long time;
  QVariant data;

  // only for bezier type
  double pre_handle_x;
  double pre_handle_y;
  double post_handle_x;
  double post_handle_y;
};

/**
 * @brief Easing presets that can be applied to keyframes
 *
 * LINEAR and HOLD only change the interpolation type. SMOOTH, IN_OUT, IN and OUT switch to bezier
 * interpolation and build the handles automatically, so a keyframe can be eased without dragging
 * handles in the Graph Editor.
 */
enum KeyframeEasing {
  /** Straight interpolation towards the next keyframe. */
  KEYFRAME_EASING_LINEAR = 0,

  /** Keep this keyframe's value until the next keyframe. */
  KEYFRAME_EASING_HOLD,

  /** Bezier interpolation with automatic handles. Keyframes that are already bezier keep their handles. */
  KEYFRAME_EASING_SMOOTH,

  /** Slow arrival and slow departure - the classic "easy ease". */
  KEYFRAME_EASING_IN_OUT,

  /** Slow arrival, constant-speed departure. */
  KEYFRAME_EASING_IN,

  /** Slow departure, constant-speed arrival. */
  KEYFRAME_EASING_OUT
};

void delete_keyframes(QVector<EffectField *> &selected_key_fields, QVector<int> &selected_keys);

/**
 * @brief Apply an easing preset to a single keyframe, without an undo action
 */
void apply_keyframe_easing(EffectField* field, int key, KeyframeEasing easing);

/**
 * @brief Apply an easing preset to a selection of keyframes as one undoable action
 *
 * `fields` and `keys` are parallel arrays, as used by the keyframe and graph views.
 */
void apply_keyframe_easing(QVector<EffectField *> &fields, QVector<int> &keys, KeyframeEasing easing);

#endif // KEYFRAME_H
