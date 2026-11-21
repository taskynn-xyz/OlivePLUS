#include "clippropertiesdialog.h"

#include <QGridLayout>
#include <QLabel>
#include <QDialogButtonBox>

#include "panels/panels.h"
#include "undo/undo.h"

ClipPropertiesDialog::ClipPropertiesDialog(QWidget *parent, QVector<Clip *> clips) :
  QDialog(parent)
{
  setWindowTitle((clips.size() == 1) ?
                   tr("\"%1\" Properties").arg(clips.at(0)->name()) :
                   tr("Multiple Clip Properties"));

  clips_ = clips;

  QGridLayout* layout = new QGridLayout(this);

  int row = 0;

  // Clip Name field
  layout->addWidget(new QLabel(tr("Name:")), row, 0);

  clip_name_field_ = new QLineEdit();

  layout->addWidget(clip_name_field_, row, 1);

  row++;

  // Clip Duration field
  layout->addWidget(new QLabel(tr("Duration:")), row, 0);

  duration_field_ = new LabelSlider();
  duration_field_->SetDisplayType(LabelSlider::FrameNumber);
  duration_field_->SetMinimum(1);
  layout->addWidget(duration_field_, row, 1);

  row++;

  // Clip Parent field (any video clip can be a parent, most often a null layer)
  layout->addWidget(new QLabel(tr("Parent:")), row, 0);

  parent_field_ = new QComboBox();
  layout->addWidget(parent_field_, row, 1);

  row++;

  // Dialog buttons (OK and Cancel)
  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->setCenterButtons(true);
  layout->addWidget(buttons, row, 0, 1, 2);

  connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
  connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));

  // analyze list of clips for default values

  bool all_clips_have_same_name = true;
  bool all_clips_have_same_duration = true;

  for (int i=1;i<clips.size();i++) {
    if (clips.at(i-1)->name() != clips.at(i)->name()) {
      all_clips_have_same_name = false;
    }
    if (clips.at(i-1)->length() != clips.at(i)->length()) {
      all_clips_have_same_duration = false;
    }
  }

  Clip* first_clip = clips_.first();

  if (all_clips_have_same_name) {
    // if there's only one clip selected, set all defaults to that clip's properties
    clip_name_field_->setText(first_clip->name());
  } else {
    // if there are multiple clips, use different properties
    clip_name_field_->setPlaceholderText(tr("(multiple)"));
  }

  // it's assumed all the clips come from the same sequence
  duration_field_->SetFrameRate(first_clip->sequence->frame_rate);

  // fill the parent list with this sequence's null layers
  parent_field_->addItem(tr("(None)"), -1);

  // parent choices are limited to the layers directly above and below the edited clips, which is
  // where a rig sits in the timeline. An existing parent stays in the list wherever it is, so
  // opening the dialog never silently drops the link.
  int parent_track_min = 0;
  int parent_track_max = 0;
  bool have_parent_track = false;
  for (int i=0;i<clips_.size();i++) {
    const int clip_track = clips_.at(i)->track();
    if (clip_track >= 0) {
      continue;
    }
    if (!have_parent_track) {
      parent_track_min = parent_track_max = clip_track;
      have_parent_track = true;
    } else {
      parent_track_min = qMin(parent_track_min, clip_track);
      parent_track_max = qMax(parent_track_max, clip_track);
    }
  }

  if (first_clip->sequence != nullptr) {
    for (int i=0;i<first_clip->sequence->clips.size();i++) {
      Clip* candidate = first_clip->sequence->clips.at(i).get();

      // audio clips carry no transform, and the clips being edited can't parent themselves
      if (candidate == nullptr || candidate->track() >= 0 || clips_.contains(candidate)) {
        continue;
      }

      if (candidate != first_clip->parent()
          && !track_is_adjacent_to(candidate->track(), parent_track_min, parent_track_max)) {
        continue;
      }

      // parenting to your own descendant would make a loop for the compositor to walk
      bool would_loop = false;
      Clip* ancestor = candidate->parent();
      int guard = 0;
      while (ancestor != nullptr && guard++ < 64) {
        if (clips_.contains(ancestor)) {
          would_loop = true;
          break;
        }
        ancestor = ancestor->parent();
      }
      if (would_loop) {
        continue;
      }

      parent_candidates_.append(candidate);
      parent_field_->addItem(candidate->name(), parent_candidates_.size() - 1);
    }
  }

  bool all_clips_have_same_parent = true;
  for (int i=0;i<clips_.size();i++) {
    if (clips_.at(i)->parent() != first_clip->parent()) {
      all_clips_have_same_parent = false;
      break;
    }
  }

  if (all_clips_have_same_parent) {
    if (first_clip->parent() == nullptr) {
      parent_field_->setCurrentIndex(0);
    } else {
      int parent_index = parent_candidates_.indexOf(first_clip->parent());
      parent_field_->setCurrentIndex(parent_index > -1 ? parent_index + 1 : 0);
    }
  } else {
    parent_field_->setCurrentIndex(-1);
    parent_field_->setPlaceholderText(tr("(multiple)"));
  }

  if (all_clips_have_same_duration) {
    duration_field_->SetDefault(first_clip->length());
    duration_field_->SetValue(first_clip->length());
    duration_field_->SetMaximum(first_clip->media_length());
  } else {
    duration_field_->SetDefault(qSNaN());
    duration_field_->SetValue(qSNaN());
  }
}

void ClipPropertiesDialog::accept()
{
  const QString& clip_name = clip_name_field_->text();
  double clip_duration = duration_field_->value();

  const int parent_selection = parent_field_->currentIndex() > 0
      ? parent_field_->currentData().toInt()
      : -1;
  const bool change_parent = (parent_field_->currentIndex() != -1);

  ComboAction* ca = new ComboAction();

  for (int i=0;i<clips_.size();i++) {
    Clip* clip = clips_.at(i);

    // If the user entered a Clip name and the name is different from the Clip's name, create a rename command
    if (!clip_name.isEmpty() && clip_name != clip->name()) {
      ca->append(new RenameClipCommand(clip, clip_name));
    }

    // If the user picked a parent, and it differs from the clip's current one, reparent it
    if (change_parent) {
      Clip* new_parent = (parent_selection >= 0 && parent_selection < parent_candidates_.size())
          ? parent_candidates_.at(parent_selection)
          : nullptr;

      if (clip->parent() != new_parent) {
        ca->append(new SetClipParentCommand(clip, new_parent));
      }
    }

    // If the user entered a clip duration (and the duration has changed), create a "clip move" command
    if (!qIsNaN(clip_duration)) {
      long clip_duration_rounded = qRound(clip_duration);

      if (clip->length() != clip_duration_rounded) {
        clip->move(ca,
                   clip->timeline_in(),
                   clip->timeline_in() + clip_duration_rounded,
                   clip->clip_in(),
                   clip->track());
      }

    }
  }

  if (ca->hasActions()) {
    olive::UndoStack.push(ca);
    update_ui(false);
  } else {
    delete ca;
  }

  QDialog::accept();
}
