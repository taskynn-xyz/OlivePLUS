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

#ifndef CLIP_H
#define CLIP_H

#include <memory>
#include <QWaitCondition>
#include <QMutex>
#include <QVector>
#include <QOpenGLFramebufferObject>
#include <QOpenGLTexture>

#include "rendering/cacher.h"

#include "effects/effect.h"
#include "effects/transition.h"
#include "undo/comboaction.h"
#include "project/media.h"
#include "project/footage.h"

#include "marker.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
}

struct ClipSpeed {
  ClipSpeed();
  double value;
  bool maintain_audio_pitch;
};

using ClipPtr = std::shared_ptr<Clip>;

class Sequence;

/**
 * @brief Whether a clip on candidate_track is directly above or below a block of video tracks
 *
 * Video tracks are numbered 0, -1, -2... from the top of the timeline, so the track above a block
 * is max_track + 1 and the track below it is min_track - 1. Parent choices are limited to those
 * two tracks, which keeps rigging local to the layers around the selection.
 */
bool track_is_adjacent_to(int candidate_track, int min_track, int max_track);

class Clip {
public:
  Clip(Sequence *s);
  ~Clip();
  ClipPtr copy(Sequence *s);

  bool IsActiveAt(long timecode);
  bool IsSelected(bool containing = true);

  const QColor& color();
  void set_color(int r, int g, int b);
  void set_color(const QColor& c);

  Media* media();
  FootageStream* media_stream();
  int media_stream_index();
  int media_width();
  int media_height();
  double media_frame_rate();
  long media_length();
  void set_media(Media* m, int s);

  bool enabled();
  void set_enabled(bool e);

  void move(ComboAction* ca,
            long iin,
            long iout,
            long iclip_in,
            int itrack,
            bool verify_transitions = true,
            bool relative = false);

  long clip_in(bool with_transition = false);
  void set_clip_in(long c);

  long timeline_in(bool with_transition = false);
  void set_timeline_in(long t);

  long timeline_out(bool with_transition = false);
  void set_timeline_out(long t);

  int track();
  void set_track(int t);

  bool reversed();
  void set_reversed(bool r);

  bool autoscaled();
  void set_autoscaled(bool b);

  /**
   * @brief Whether this clip is an adjustment layer
   *
   * An adjustment layer has no media of its own. When it is composited it takes the frame that has
   * already been rendered below it on the timeline and runs its effect chain on that, so effects
   * placed on the adjustment layer apply to all the video tracks underneath it.
   */
  bool IsAdjustmentLayer();
  void set_adjustment_layer(bool b);

  /**
   * @brief Whether this clip is a null layer
   *
   * A null layer has no media and draws nothing. It exists to carry a transform that other clips
   * can be parented to (see parent()), which is how motion-graphics rigs are built.
   */
  bool IsNullLayer();
  void set_null_layer(bool b);

  /**
   * @brief Whether this null layer was created as a 3D null
   *
   * A 3D null is the same as a regular null (it draws nothing and exists to be parented to) but it
   * starts with a 3D Transform effect instead of the 2D one, so its position has a Z value and it
   * can be rotated on all three axes.
   */
  bool Is3DNullLayer();
  void set_3d_null_layer(bool b);

  /**
   * @brief Whether this clip is a camera layer
   *
   * A camera layer draws nothing. Its Camera effect drives the 3D Transform of every clip that has
   * one, so the camera only affects layers that are actually in 3D.
   */
  bool IsCameraLayer();
  void set_camera_layer(bool b);

  /**
   * @brief The clip whose transform this clip inherits, or nullptr
   *
   * Set from the Clip Properties dialog. While parenting is active the parent's Transform and 3D
   * Transform effects are applied on top of this clip's own transform, exactly like After Effects.
   */
  Clip* parent();
  void set_parent(Clip* c);

  double cached_frame_rate();
  void set_cached_frame_rate(double d);

  const QString& name();
  void set_name(const QString& s);

  const ClipSpeed& speed();
  void set_speed(const ClipSpeed& s);

  AVRational time_base();

  void reset_audio();
  void refresh();

  long length();

  void refactor_frame_rate(ComboAction* ca, double multiplier, bool change_timeline_points);
  Sequence* sequence;

  // markers
  QVector<Marker>& get_markers();

  // other variables (should be deep copied/duplicated in copy())
  int IndexOfEffect(Effect* e);
  QList<EffectPtr> effects;
  QVector<int> linked;
  TransitionPtr opening_transition;
  TransitionPtr closing_transition;

  // playback functions
  void Open();
  void Cache(long playhead, bool scrubbing, QVector<Clip*> &nests, int playback_speed);
  bool Retrieve();
  void Close(bool wait);
  bool IsOpen();

  bool UsesCacher();

  // temporary variables
  int load_id;
  bool undeletable;
  bool replaced;

  // parent clip's load_id while a project is being loaded, resolved once every clip exists
  int load_parent_id;

  // caching functions
  QMutex state_change_lock;
  QMutex cache_lock;

  // video playback variables
  QOpenGLFramebufferObject** fbo;

  // preview resolution divisor the framebuffers in fbo were created with (-1 = none yet).
  // When the user changes the playback quality mid-session the buffers have to be rebuilt at the
  // new size, otherwise the clip is drawn into a buffer of the wrong resolution and gets cropped.
  int fbo_divider;
  QOpenGLTexture* texture;
  long texture_frame;

private:
  // timeline variables (should be copied in copy())
  bool enabled_;
  long clip_in_;
  long timeline_in_;
  long timeline_out_;
  int track_;
  QString name_;
  Media* media_;
  int media_stream_;
  ClipSpeed speed_;
  double cached_fr_;
  bool reverse_;
  bool autoscale_;
  bool adjustment_;
  bool null_;
  bool null_3d_;
  bool camera_;

  Clip* parent_;

  Cacher cacher;
  long cacher_frame;

  QVector<Marker> markers;
  QColor color_;
  bool open_;
};

#endif // CLIP_H
