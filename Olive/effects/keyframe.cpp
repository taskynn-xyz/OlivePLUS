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

#include "keyframe.h"

#include <QVector>

#include "effectfields.h"
#include "undo/undo.h"
#include "undo/undostack.h"
#include "panels/panels.h"

namespace {

/// Handle length used when a keyframe has no neighbour to measure against
const double kAutoHandleFallback = 40;

/// Finds the closest keyframe before (or after) `key` in time. Returns -1 if there is none.
int find_neighbour_keyframe(const QVector<EffectKeyframe>& keys, int key, bool after) {
  int found = -1;

  for (int i=0;i<keys.size();i++) {
    if (i == key) {
      continue;
    }

    if (after) {
      if (keys.at(i).time > keys.at(key).time
          && (found == -1 || keys.at(i).time < keys.at(found).time)) {
        found = i;
      }
    } else {
      if (keys.at(i).time < keys.at(key).time
          && (found == -1 || keys.at(i).time > keys.at(found).time)) {
        found = i;
      }
    }
  }

  return found;
}

/**
 * @brief Build one side of a keyframe's bezier handles
 *
 * An eased side gets a flat handle (pointing at the keyframe's own value), so the curve arrives
 * or leaves at rest. A side that is not eased is aimed along the straight line to the neighbour,
 * which keeps that half of the segment at a constant speed instead of easing it too.
 */
void set_side_handle(EffectKeyframe& key,
                     bool post,
                     const EffectKeyframe* neighbour,
                     bool eased,
                     bool numeric) {
  double x = post ? kAutoHandleFallback : -kAutoHandleFallback;
  double y = 0;

  if (neighbour != nullptr) {
    const double gap = double(neighbour->time) - double(key.time);

    if (!qFuzzyIsNull(gap)) {
      // A third of the way to the neighbour is the longest handle that cannot overshoot it
      x = gap / 3.0;

      if (!eased && numeric) {
        y = (neighbour->data.toDouble() - key.data.toDouble()) / 3.0;
      }
    }
  }

  if (post) {
    key.post_handle_x = x;
    key.post_handle_y = y;
  } else {
    key.pre_handle_x = x;
    key.pre_handle_y = y;
  }
}

struct EasedKeyframe {
  int type;
  double pre_handle_x;
  double pre_handle_y;
  double post_handle_x;
  double post_handle_y;
  bool rebuild_handles;
};

EasedKeyframe compute_eased_keyframe(EffectField* field, int key, KeyframeEasing easing) {
  const EffectKeyframe& current = field->keyframes.at(key);

  EasedKeyframe result;
  result.type = current.type;
  result.pre_handle_x = current.pre_handle_x;
  result.pre_handle_y = current.pre_handle_y;
  result.post_handle_x = current.post_handle_x;
  result.post_handle_y = current.post_handle_y;
  result.rebuild_handles = false;

  if (easing == KEYFRAME_EASING_LINEAR) {
    result.type = EFFECT_KEYFRAME_LINEAR;
    return result;
  }

  if (easing == KEYFRAME_EASING_HOLD) {
    result.type = EFFECT_KEYFRAME_HOLD;
    return result;
  }

  // Smoothing a keyframe that is already bezier must not throw away handles the user has dragged
  if (easing == KEYFRAME_EASING_SMOOTH && current.type == EFFECT_KEYFRAME_BEZIER) {
    return result;
  }

  const int prev = find_neighbour_keyframe(field->keyframes, key, false);
  const int next = find_neighbour_keyframe(field->keyframes, key, true);

  const bool ease_in = (easing != KEYFRAME_EASING_OUT);
  const bool ease_out = (easing != KEYFRAME_EASING_IN);
  const bool numeric = (field->type() == EffectField::EFFECT_FIELD_DOUBLE);

  EffectKeyframe working = current;
  set_side_handle(working, false, (prev > -1) ? &field->keyframes.at(prev) : nullptr, ease_in, numeric);
  set_side_handle(working, true, (next > -1) ? &field->keyframes.at(next) : nullptr, ease_out, numeric);

  result.type = EFFECT_KEYFRAME_BEZIER;
  result.pre_handle_x = working.pre_handle_x;
  result.pre_handle_y = working.pre_handle_y;
  result.post_handle_x = working.post_handle_x;
  result.post_handle_y = working.post_handle_y;
  result.rebuild_handles = true;
  return result;
}

}

EffectKeyframe::EffectKeyframe()
{
  pre_handle_x = -40;
  pre_handle_y = 0;
  post_handle_x = 40;
  post_handle_y = 0;
}

void delete_keyframes(QVector<EffectField *>& selected_key_fields, QVector<int> &selected_keys) {
  QVector<EffectField*> fields;
  QVector<int> key_indices;

  for (int i=0;i<selected_keys.size();i++) {
    bool added = false;
    for (int j=0;j<key_indices.size();j++) {
      if (key_indices.at(j) < selected_keys.at(i)) {
        key_indices.insert(j, selected_keys.at(i));
        fields.insert(j, selected_key_fields.at(i));
        added = true;
        break;
      }
    }
    if (!added) {
      key_indices.append(selected_keys.at(i));
      fields.append(selected_key_fields.at(i));
    }
  }

  if (fields.size() > 0) {
    ComboAction* ca = new ComboAction();
    for (int i=0;i<key_indices.size();i++) {
      ca->append(new KeyframeDelete(fields.at(i), key_indices.at(i)));
    }
    olive::UndoStack.push(ca);
    selected_keys.clear();
    selected_key_fields.clear();
    update_ui(false);
  }
}

void apply_keyframe_easing(EffectField* field, int key, KeyframeEasing easing) {
  if (field == nullptr || key < 0 || key >= field->keyframes.size()) {
    return;
  }

  const EasedKeyframe result = compute_eased_keyframe(field, key, easing);
  EffectKeyframe& keyframe = field->keyframes[key];

  keyframe.type = result.type;

  if (result.rebuild_handles) {
    keyframe.pre_handle_x = result.pre_handle_x;
    keyframe.pre_handle_y = result.pre_handle_y;
    keyframe.post_handle_x = result.post_handle_x;
    keyframe.post_handle_y = result.post_handle_y;
  }
}

void apply_keyframe_easing(QVector<EffectField *> &fields, QVector<int> &keys, KeyframeEasing easing) {
  if (fields.isEmpty() || fields.size() != keys.size()) {
    return;
  }

  ComboAction* ca = new ComboAction();

  for (int i=0;i<fields.size();i++) {
    EffectField* field = fields.at(i);
    const int index = keys.at(i);

    if (field == nullptr || index < 0 || index >= field->keyframes.size()) {
      continue;
    }

    const EasedKeyframe result = compute_eased_keyframe(field, index, easing);
    EffectKeyframe& keyframe = field->keyframes[index];

    if (keyframe.type != result.type) {
      ca->append(new SetInt(&keyframe.type, result.type));
    }

    if (result.rebuild_handles) {
      ca->append(new SetDouble(&keyframe.pre_handle_x, keyframe.pre_handle_x, result.pre_handle_x));
      ca->append(new SetDouble(&keyframe.pre_handle_y, keyframe.pre_handle_y, result.pre_handle_y));
      ca->append(new SetDouble(&keyframe.post_handle_x, keyframe.post_handle_x, result.post_handle_x));
      ca->append(new SetDouble(&keyframe.post_handle_y, keyframe.post_handle_y, result.post_handle_y));
    }
  }

  if (ca->hasActions()) {
    olive::UndoStack.push(ca);
    update_ui(false);
  } else {
    delete ca;
  }
}
