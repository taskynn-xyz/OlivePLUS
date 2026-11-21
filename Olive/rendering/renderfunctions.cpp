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

#include "renderfunctions.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <QOpenGLFramebufferObject>
#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>

#ifdef OLIVE_OCIO
#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;
#endif

#include "timeline/clip.h"
#include "timeline/sequence.h"
#include "project/media.h"
#include "effects/effect.h"
#include "project/footage.h"
#include "effects/transition.h"

#include "ui/collapsiblewidget.h"

#include "rendering/audio.h"

#include "global/math.h"
#include "global/config.h"

#include "panels/timeline.h"
#include "panels/viewer.h"

namespace {

/// Applies an affine matrix to the four corners of a GLTextureCoords
void transform_coords(GLTextureCoords& coords, const double* matrix) {
  int* xs[4] = { &coords.vertexTopLeftX, &coords.vertexTopRightX,
                 &coords.vertexBottomRightX, &coords.vertexBottomLeftX };
  int* ys[4] = { &coords.vertexTopLeftY, &coords.vertexTopRightY,
                 &coords.vertexBottomRightY, &coords.vertexBottomLeftY };

  for (int i=0;i<4;i++) {
    double x, y;
    mat4_transform_point(matrix, *xs[i], *ys[i], &x, &y);
    *xs[i] = qRound(x);
    *ys[i] = qRound(y);
  }
}

/// True for effects that should be composited on top of the clip's transform, in sequence space.
/// These are the plain image effects (blur, colour, glow, ...): geometric effects (Transform, 3D
/// Transform, Corner Pin, Shake), generators (text, solids, shapes) and effects that need the clip's
/// own framebuffer (motion blur) stay in the pre-transform chain.
bool effect_runs_after_transform(Effect* e) {
  if (e == nullptr || !e->IsEnabled()) {
    return false;
  }

  const int flags = e->Flags();

  return (flags & Effect::ShaderFlag)
      && !(flags & (Effect::CoordsFlag
                    | Effect::SuperimposeFlag
                    | Effect::ImageFlag
                    | Effect::PostShaderFlag));
}

// Like get_timecode(), but accepts a fractional playhead so motion blur can evaluate a parent
// transform at the sub-frame times its shutter samples. Mirrors playhead_to_clip_frame().
double parent_timecode_at(Clip* c, double playhead) {
  const double clip_frame = qMax(0.0, playhead - double(c->timeline_in(true)))
      + double(c->clip_in(true));
  return clip_frame / c->sequence->frame_rate;
}

}

bool clip_has_3d_transform(Clip* clip) {
  if (clip == nullptr) {
    return false;
  }

  for (int i=0;i<clip->effects.size();i++) {
    Effect* e = clip->effects.at(i).get();
    if (e != nullptr && e->IsEnabled() && e->meta != nullptr
        && e->meta->internal == EFFECT_INTERNAL_TRANSFORM_3D) {
      return true;
    }
  }

  return false;
}

bool build_clip_flat_matrix(Clip* clip, double timecode, double* out_matrix) {
  mat4_identity(out_matrix);

  if (clip == nullptr) {
    return false;
  }

  bool found_transform = false;

  for (int i=0;i<clip->effects.size();i++) {
    Effect* e = clip->effects.at(i).get();

    if (e == nullptr || !e->IsEnabled() || e->meta == nullptr
        || e->meta->internal != EFFECT_INTERNAL_TRANSFORM) {
      continue;
    }

    double effect_matrix[16];
    if (e->GetTransformMatrix(timecode, effect_matrix)) {
      double result[16];
      mat4_multiply(out_matrix, effect_matrix, result);
      for (int j=0;j<16;j++) {
        out_matrix[j] = result[j];
      }
      found_transform = true;
    }
  }

  return found_transform;
}

/**
 * @brief Accumulate the transform of a clip's parent chain
 *
 * Walks up from the clip's parent, multiplying each parent's Transform and 3D Transform effects
 * together. The result can be handed straight to glMultMatrixd() and applies the whole chain on top
 * of the clip's own transform, which is the order parenting works in After Effects.
 *
 * @param playhead
 *
 * Sequence playhead, which may be fractional (motion blur samples parents at sub-frame times).
 */
bool build_parent_matrix(Clip* clip, double playhead, double* out_matrix, bool for_3d,
                         bool* out_used_3d_parent) {
  double accumulated[16];
  mat4_identity(accumulated);

  bool used_3d_parent = false;

  if (out_used_3d_parent != nullptr) {
    *out_used_3d_parent = false;
  }

  // A clip with a 3D Transform does its own parenting, in scene space (see
  // ThreeDTransformEffect::Project), so the compositor must not also fold a parent matrix into the
  // modelview: that was the faux-3D behaviour, and it would transform the child twice.
  if (!for_3d && clip_has_3d_transform(clip)) {
    return false;
  }

  bool found_transform = false;

  QVector<Clip*> visited;
  visited.append(clip);

  Clip* current = clip->parent();

  while (current != nullptr && visited.size() < 32) {
    if (visited.contains(current)) {
      // a parent chain that loops back on itself: stop instead of spinning forever
      break;
    }
    visited.append(current);

    double local[16];
    mat4_identity(local);

    const double parent_timecode = parent_timecode_at(current, playhead);

    const bool has_3d_transform = clip_has_3d_transform(current);

    // A 3D null only drives 3D layers: a clip without a 3D Transform is composited flat, so a 3D
    // parent is skipped instead of being flattened into the modelview.
    if (!for_3d && has_3d_transform) {
      current = current->parent();
      continue;
    }

    if (has_3d_transform) {
      used_3d_parent = true;
    }

    for (int i=0;i<current->effects.size();i++) {
      Effect* e = current->effects.at(i).get();

      if (e == nullptr || !e->IsEnabled() || e->meta == nullptr) {
        continue;
      }

      if (has_3d_transform && e->meta->internal == EFFECT_INTERNAL_TRANSFORM) {
        continue;
      }

      double effect_matrix[16];
      if (e->GetTransformMatrix(parent_timecode, effect_matrix)) {
        double result[16];
        mat4_multiply(local, effect_matrix, result);
        for (int j=0;j<16;j++) {
          local[j] = result[j];
        }
        found_transform = true;
      }
    }

    // the closest ancestor is applied last, so it ends up innermost
    double result[16];
    mat4_multiply(local, accumulated, result);
    for (int j=0;j<16;j++) {
      accumulated[j] = result[j];
    }

    current = current->parent();
  }

  for (int i=0;i<16;i++) {
    out_matrix[i] = accumulated[i];
  }

  if (out_used_3d_parent != nullptr) {
    *out_used_3d_parent = used_3d_parent;
  }

  return found_transform;
}


void PrepareToDraw(QOpenGLFunctions* f) {
  // Everything drawn through this helper is a 1:1 full-frame blit, so mipmaps are not needed here.
  // Building a mip chain on every pass used to cost a full texture reduction per clip per effect,
  // which was the single biggest GPU cost of playback. The viewer still builds mipmaps for the
  // final displayed frame (see ViewerWidget::paintGL) so downscaled display stays smooth.
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
}

void full_blit() {
  PrepareToDraw(QOpenGLContext::currentContext()->functions());

  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, 1, 0, 1, -1, 1);

  glBegin(GL_QUADS);
  glTexCoord2f(0, 0); // top left
  glVertex2f(0, 0); // top left
  glTexCoord2f(1, 0); // top right
  glVertex2f(1, 0); // top right
  glTexCoord2f(1, 1); // bottom right
  glVertex2f(1, 1); // bottom right
  glTexCoord2f(0, 1); // bottom left
  glVertex2f(0, 1); // bottom left
  glEnd();

  glPopMatrix();
}

void draw_clip(QOpenGLContext* ctx, GLuint fbo, GLuint texture, bool clear) {
  ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

  if (clear) {
    glClear(GL_COLOR_BUFFER_BIT);
  }

  glBindTexture(GL_TEXTURE_2D, texture);

  full_blit();

  glBindTexture(GL_TEXTURE_2D, 0);

  ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

GLuint draw_clip(QOpenGLFramebufferObject* fbo, GLuint texture, bool clear) {
  fbo->bind();

  if (clear) {
    glClear(GL_COLOR_BUFFER_BIT);
  }

  glBindTexture(GL_TEXTURE_2D, texture);

  full_blit();

  glBindTexture(GL_TEXTURE_2D, 0);

  fbo->release();

  return fbo->texture();
}

void process_effect(Clip* c,
                    Effect* e,
                    double timecode,
                    GLTextureCoords& coords,
                    GLuint& composite_texture,
                    bool& fbo_switcher,
                    bool& texture_failed,
                    int data) {
  if (e->IsEnabled()) {
    if (e->Flags() & Effect::CoordsFlag) {
      e->process_coords(timecode, coords, data);
    }
    bool can_process_shaders = ((e->Flags() & Effect::ShaderFlag) && olive::CurrentRuntimeConfig.shaders_are_enabled);
    if (can_process_shaders || (e->Flags() & Effect::SuperimposeFlag)) {
      e->startEffect();
      if (can_process_shaders && e->is_glsl_linked()) {
        for (int i=0;i<e->getIterations();i++) {
          e->process_shader(timecode, coords, i);
          composite_texture = draw_clip(c->fbo[fbo_switcher], composite_texture, true);
          fbo_switcher = !fbo_switcher;
        }
      }
      if (e->Flags() & Effect::SuperimposeFlag) {
        GLuint superimpose_texture = e->process_superimpose(timecode);

        if (superimpose_texture == 0) {
          qWarning() << "Superimpose texture was nullptr, retrying...";
          texture_failed = true;
        } else if (composite_texture == 0) {
          // if there is no previous texture, just return the superimposes texture
          // UNLESS this is a shader-extended superimpose effect in which case,
          // we'll need to draw it below
          composite_texture = superimpose_texture;
        } else {
          // if the source texture is not already a framebuffer texture,
          // we'll need to make it one before drawing a superimpose effect on it
          if (composite_texture != c->fbo[0]->texture() && composite_texture != c->fbo[1]->texture()) {
            draw_clip(c->fbo[!fbo_switcher], composite_texture, true);
          }

          composite_texture = draw_clip(c->fbo[!fbo_switcher], superimpose_texture, false);
        }
      }
      e->endEffect();
    }
  }
}

GLuint olive::rendering::compose_sequence(ComposeSequenceParams &params) {
//  qint64 time = QDateTime::currentMSecsSinceEpoch();

  GLuint final_fbo = params.main_buffer;

  // preview resolution divisor (1 = full resolution)
  const int div = qMax(1, params.divider);

  Sequence* s = params.seq;
  long playhead = s->playhead;

  if (!params.nests.isEmpty()) {
    for (int i=0;i<params.nests.size();i++) {
      s = params.nests.at(i)->media()->to_sequence().get();
      playhead += params.nests.at(i)->clip_in(true) - params.nests.at(i)->timeline_in(true);
      playhead = rescale_frame_number(playhead, params.nests.at(i)->sequence->frame_rate, s->frame_rate);
    }

    if (params.video && params.nests.last()->fbo != nullptr) {
      params.nests.last()->fbo[0]->bind();
      glClear(GL_COLOR_BUFFER_BIT);
      final_fbo = params.nests.last()->fbo[0]->handle();
    }
  }

  int audio_track_count = 0;

  QVector<Clip*> current_clips;

  // loop through clips, find currently active, and sort by track
  for (int i=0;i<s->clips.size();i++) {

    Clip* c = s->clips.at(i).get();

    if (c != nullptr) {

      // if clip is video and we're processing video
      if ((c->track() < 0) == params.video) {

        bool clip_is_active = false;

        // is the clip a "footage" clip?
        if (c->media() != nullptr && c->media()->get_type() == MEDIA_TYPE_FOOTAGE) {
          Footage* m = c->media()->to_footage();

          // does the clip have a valid media source?
          if (!m->invalid && !(c->track() >= 0 && !is_audio_device_set())) {

            // is the media process and ready?
            if (m->ready) {
              const FootageStream* ms = c->media_stream();

              // does the media have a valid media stream source and is it active?
              if (ms != nullptr && c->IsActiveAt(playhead)) {

                // open if not open
                if (!c->IsOpen()) {
                  c->Open();
                }

                clip_is_active = true;

                // increment audio track count
                if (c->track() >= 0) audio_track_count++;

              } else if (c->IsOpen()) {

                // close the clip if it isn't active anymore
                c->Close(false);

              }
            } else {

              // media wasn't ready, schedule a redraw
              params.texture_failed = true;

            }
          }
        } else {
          // if the clip is a nested sequence or null clip, just open it

          if (c->IsActiveAt(playhead)) {
            if (!c->IsOpen()) {
              c->Open();
            }
            clip_is_active = true;
          } else if (c->IsOpen()) {
            c->Close(false);
          }
        }

        // if the clip is active, added it to "current_clips", sorted by track
        if (clip_is_active) {
          bool added = false;

          // track sorting is only necessary for video clips
          // audio clips are mixed equally, so we skip sorting for those
          if (params.video) {

            // insertion sort by track
            for (int j=0;j<current_clips.size();j++) {
              if (current_clips.at(j)->track() < c->track()) {
                current_clips.insert(j, c);
                added = true;
                break;
              }
            }

          }

          if (!added) {
            current_clips.append(c);
          }
        }
      }
    }
  }

  if (params.video) {

    // set default coordinates based on the sequence, with 0 in the direct center
    glPushMatrix();
    glLoadIdentity();

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    int half_width = s->width/2;
    int half_height = s->height/2;
    // Wide depth range: 3D Transform / 3D null rotations move geometry far outside a small
    // near/far range, which used to clip parts of the quad away (black bars).
    glOrtho(-half_width, half_width, -half_height, half_height, -1.0e6, 1.0e6);

  }

  // loop through current clips

  for (int i=0;i<current_clips.size();i++) {
    Clip* c = current_clips.at(i);

    bool got_mutex = true;

    if (params.wait_for_mutexes) {
      // wait for clip to finish opening
      c->state_change_lock.lock();
    } else {
      got_mutex = c->state_change_lock.tryLock();
    }

    if (got_mutex && c->IsOpen()) {
      // if clip is a video clip
      if (c->track() < 0) {

        // reset OpenGL to full color
        glColor4f(1.0, 1.0, 1.0, 1.0);

        // textureID variable contains texture to be drawn on screen at the end
        GLuint textureID = 0;

        // store video source dimensions
        int video_width = c->media_width();
        int video_height = c->media_height();

        // if media is footage
        if (c->media() != nullptr && c->media()->get_type() == MEDIA_TYPE_FOOTAGE) {

          // retrieve video frame from cache and store it in c->texture
          c->Cache(qMax(playhead, c->timeline_in()), false, params.nests, params.playback_speed);
          if (!c->Retrieve()) {
            params.texture_failed = true;
          } else {
            // retrieve ID from c->texture
            textureID = c->texture->textureId();
          }

          if (textureID == 0) {
            qWarning() << "Failed to create texture";
          }
        }

        // An adjustment clip has no image of its own. Instead it takes whatever has already been
        // composited below it in this frame (the sequence buffer, since clips are drawn bottom-up)
        // as its source texture, so its effect chain processes the tracks underneath it.
        if (c->IsAdjustmentLayer()) {
          video_width = s->width;
          video_height = s->height;

          bool has_effects = false;
          for (int j=0;j<c->effects.size();j++) {
            if (c->effects.at(j)->IsEnabled()) {
              has_effects = true;
              break;
            }
          }

          if (has_effects) {
            textureID = params.nests.isEmpty()
                ? params.main_attachment
                : params.nests.last()->fbo[0]->texture();
          } else {
            // nothing to adjust: leave the frame untouched rather than drawing it over itself
            textureID = 0;
          }
        }

        // prepare framebuffers for backend drawing operations
        if (c->fbo == nullptr || c->fbo_divider != div) {
          // 3 fbos for nested sequences, 2 for most clips
          const int fbo_count = (c->media() != nullptr && c->media()->get_type() == MEDIA_TYPE_SEQUENCE) ? 3 : 2;

          if (c->fbo != nullptr) {
            // the preview resolution changed while this clip was open, so the old buffers are the
            // wrong size - destroy them and recreate below
            for (int j=0;j<fbo_count;j++) {
              delete c->fbo[j];
            }

            delete [] c->fbo;
            c->fbo = nullptr;
          }

          c->fbo = new QOpenGLFramebufferObject* [size_t(fbo_count)];

          for (int j=0;j<fbo_count;j++) {
            c->fbo[j] = new QOpenGLFramebufferObject(qMax(1, video_width/div), qMax(1, video_height/div));
          }

          c->fbo_divider = div;
        }

        // if clip should actually be shown on screen in this frame
        if (playhead >= c->timeline_in(true)
            && playhead < c->timeline_out(true)) {
          glPushMatrix();

          // Parenting: a clip with a parent null layer inherits that null's transform. Multiplying
          // the parent matrix in here (before the clip's own effects run) makes the clip transform
          // first and the parent transform on top of it, which is the After Effects order.
          double parent_matrix[16];
          const bool has_parent_transform = build_parent_matrix(c, playhead, parent_matrix);
          if (has_parent_transform) {
            glMultMatrixd(parent_matrix);
          }

          // simple bool for switching between the two framebuffers
          bool fbo_switcher = false;

          glViewport(0, 0, qMax(1, video_width/div), qMax(1, video_height/div));

          if (c->media() != nullptr) {
            if (c->media()->get_type() == MEDIA_TYPE_SEQUENCE) {
              // for a nested sequence, run this function again on that sequence and retrieve the texture

              // add nested sequence to nest list
              params.nests.append(c);

              // compose sequence
              textureID = compose_sequence(params);

              // remove sequence from nest list
              params.nests.removeLast();

              // compose_sequence() would have written to this clip's fbo[0], so we switch to fbo[1]
              fbo_switcher = true;
            } else if (c->media()->get_type() == MEDIA_TYPE_FOOTAGE) {

              if (!c->media()->to_footage()->alpha_is_premultiplied) {
                // alpha is not premultiplied, we'll need to multiply it for the rest of the pipeline
                params.premultiply_program->bind();

                textureID = draw_clip(c->fbo[0], textureID, true);

                params.premultiply_program->release();

                fbo_switcher = true;
              }

#ifdef OLIVE_OCIO
              // convert to linear colorspace
              bool linear_convert = true;
              if (linear_convert)
              {

              }
#endif

            }
          }

          // set up default coordinates for drawing the clip
          GLTextureCoords coords;
          coords.grid_size = 1;
          coords.vertexTopLeftX = coords.vertexBottomLeftX = -video_width/2;
          coords.vertexTopLeftY = coords.vertexTopRightY = -video_height/2;
          coords.vertexTopRightX = coords.vertexBottomRightX = video_width/2;
          coords.vertexBottomLeftY = coords.vertexBottomRightY = video_height/2;
          coords.vertexBottomLeftZ = coords.vertexBottomRightZ = coords.vertexTopLeftZ = coords.vertexTopRightZ = 1;
          coords.textureTopLeftY = coords.textureTopRightY = coords.textureTopLeftX = coords.textureBottomLeftX = 0.0;
          coords.textureBottomLeftY = coords.textureBottomRightY = coords.textureTopRightX = coords.textureBottomRightX = 1.0;
          // Every corner needs a Q of 1. The bottom right corner used to be left uninitialised here,
          // which put garbage into the perspective divide of that corner and sheared/warped every
          // clip (and title) into a fake 3D plane. Effects that want perspective (3D Transform)
          // overwrite all four corners themselves.
          coords.textureTopLeftQ = coords.textureTopRightQ = coords.textureBottomRightQ = coords.textureBottomLeftQ = 1.0f;
          coords.blendmode = -1;
          coords.opacity = 1.0;
          coords.divider = div;

          // == EFFECT CODE START ==

          // get current sequence time in seconds (used for effects)
          double timecode = get_timecode(c, playhead);

          // A null layer generates no image of its own - it exists to carry a transform that other
          // clips are parented to. Its coords are still evaluated (so its gizmos can be dragged in
          // the viewer) but no shader pass runs, since there is no source texture for one to read,
          // and nothing is drawn into the sequence.
          const bool is_null_layer = c->IsNullLayer() || c->IsCameraLayer();

          // Plain image effects are deferred until the clip has been drawn with its transform, but
          // only when the sequence-space pass below can actually run. Nested sequences render into
          // their own buffer, so there the effects stay in the normal chain.
          const bool defer_effects = params.nests.isEmpty()
              && params.backend_buffer2 != 0
              && params.backend_attachment2 != 0
              && olive::CurrentRuntimeConfig.shaders_are_enabled;

          // run through all of the clip's effects
          for (int j=0;j<c->effects.size();j++) {

            Effect* e = c->effects.at(j).get();

            if (is_null_layer) {
              if (e->IsEnabled() && (e->Flags() & Effect::CoordsFlag)) {
                e->process_coords(timecode, coords, kTransitionNone);
              }
            } else if (defer_effects && effect_runs_after_transform(e)) {
              // Held back until the clip has been drawn with its Transform / 3D Transform applied
              // (see the post-transform pass below).
            } else {
              process_effect(c, e, timecode, coords, textureID, fbo_switcher, params.texture_failed, kTransitionNone);
            }

          }

          // if the clip has an opening transition, process that now
          if (!is_null_layer && c->opening_transition != nullptr) {
            int transition_progress = playhead - c->timeline_in(true);
            if (transition_progress < c->opening_transition->get_length()) {
              process_effect(c, c->opening_transition.get(), double(transition_progress)/double(c->opening_transition->get_length()), coords, textureID, fbo_switcher, params.texture_failed, kTransitionOpening);
            }
          }

          // if the clip has a closing transition, process that now
          if (!is_null_layer && c->closing_transition != nullptr) {
            int transition_progress = playhead - (c->timeline_out(true) - c->closing_transition->get_length());
            if (transition_progress >= 0 && transition_progress < c->closing_transition->get_length()) {
              process_effect(c, c->closing_transition.get(), double(transition_progress)/double(c->closing_transition->get_length()), coords, textureID, fbo_switcher, params.texture_failed, kTransitionClosing);
            }
          }

          // == EFFECT CODE END ==

          // Effects flagged for a post pass (e.g. Transform's motion blur) run here, on the fully
          // composited clip texture. This matters for generator clips: their image is produced by a
          // later effect in the chain, so a shader pass at the Transform's own position would have
          // nothing to work on.
          for (int j=0;j<c->effects.size();j++) {

            Effect* e = c->effects.at(j).get();

            if (!is_null_layer
                && e->IsEnabled()
                && (e->Flags() & Effect::PostShaderFlag)
                && olive::CurrentRuntimeConfig.shaders_are_enabled) {
              e->startEffect();

              if (e->is_glsl_linked()) {
                for (int k=0;k<e->getIterations();k++) {
                  e->process_shader(timecode, coords, k);
                  textureID = draw_clip(c->fbo[fbo_switcher], textureID, true);
                  fbo_switcher = !fbo_switcher;
                }
              }

              e->endEffect();
            }

          }


          // Check whether the parent clip is auto-scaledc
          if (c->autoscaled()
              && (video_width != s->width
                  && video_height != s->height)) {
            float width_multiplier = float(s->width) / float(video_width);
            float height_multiplier = float(s->height) / float(video_height);
            float scale_multiplier = qMin(width_multiplier, height_multiplier);
            glScalef(scale_multiplier, scale_multiplier, 1);
          }

          // Configure effect gizmos if they exist
          if (params.gizmos != nullptr) {
            if (has_parent_transform) {
              // put the gizmos in the same space the clip is drawn in, so they sit on the image
              GLTextureCoords gizmo_coords = coords;
              transform_coords(gizmo_coords, parent_matrix);
              params.gizmos->gizmo_draw(timecode, gizmo_coords); // set correct gizmo coords
            } else {
              params.gizmos->gizmo_draw(timecode, coords); // set correct gizmo coords
            }
            params.gizmos->gizmo_world_to_screen(); // convert gizmo coords to screen coords
          }



          if (textureID > 0 && !is_null_layer) {
            // set viewport to sequence size
            params.ctx->functions()->glViewport(0, 0, qMax(1, s->width/div), qMax(1, s->height/div));



            // == START RENDER CLIP IN CONTEXT OF SEQUENCE ==



            // use clip textures for nested sequences, otherwise use main frame buffers
            GLuint back_buffer_1;
            GLuint backend_tex_1;
            if (params.nests.size() > 0) {
              back_buffer_1 = params.nests.last()->fbo[1]->handle();
              backend_tex_1 = params.nests.last()->fbo[1]->texture();
            } else {
              back_buffer_1 = params.backend_buffer1;
              backend_tex_1 = params.backend_attachment1;
            }

            // render a backbuffer
            params.ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, back_buffer_1);

            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT);

            // bind final clip texture
            glBindTexture(GL_TEXTURE_2D, textureID);

            // set texture filter to bilinear
            PrepareToDraw(params.ctx->functions());

            // draw clip on screen according to gl coordinates
            glBegin(GL_QUADS);

            // glTexCoord4f carries the (already perspective-weighted) Q value, so effects like the
            // 3D Transform can supply corner-dependent perspective. Ordinary effects keep Q at 1.
            glTexCoord4f(coords.textureTopLeftX, coords.textureTopLeftY, 0.0f, coords.textureTopLeftQ); // top left
            glVertex2f(coords.vertexTopLeftX, coords.vertexTopLeftY); // top left
            glTexCoord4f(coords.textureTopRightX, coords.textureTopRightY, 0.0f, coords.textureTopRightQ); // top right
            glVertex2f(coords.vertexTopRightX, coords.vertexTopRightY); // top right
            glTexCoord4f(coords.textureBottomRightX, coords.textureBottomRightY, 0.0f, coords.textureBottomRightQ); // bottom right
            glVertex2f(coords.vertexBottomRightX, coords.vertexBottomRightY); // bottom right
            glTexCoord4f(coords.textureBottomLeftX, coords.textureBottomLeftY, 0.0f, coords.textureBottomLeftQ); // bottom left
            glVertex2f(coords.vertexBottomLeftX, coords.vertexBottomLeftY); // bottom left

            glEnd();

            // release final clip texture
            glBindTexture(GL_TEXTURE_2D, 0);

            params.ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);



            // == END RENDER CLIP IN CONTEXT OF SEQUENCE ==


            // == EFFECTS ON TOP OF THE TRANSFORM ==

            // The clip has now been drawn into sequence space with its Transform / 3D Transform and
            // parent matrix applied. Plain image effects run here, on that result, so they wrap the
            // transformed image instead of being baked inside the clip's source rectangle.
            GLuint clip_layer_tex = backend_tex_1;
            GLuint clip_layer_buf = back_buffer_1;

            if (defer_effects) {
              GLuint post_src_tex = backend_tex_1;
              bool write_to_second = true;

              glViewport(0, 0, qMax(1, s->width/div), qMax(1, s->height/div));
              glDisable(GL_BLEND);
              glColor4f(1.0, 1.0, 1.0, 1.0);

              for (int j=0;j<c->effects.size();j++) {
                Effect* e = c->effects.at(j).get();

                if (!effect_runs_after_transform(e)) {
                  continue;
                }

                e->startEffect();

                if (e->is_glsl_linked()) {
                  e->SetShaderTargetSize(qMax(1, s->width/div), qMax(1, s->height/div));

                  for (int k=0;k<e->getIterations();k++) {
                    e->process_shader(timecode, coords, k);

                    const GLuint dst_buf = write_to_second ? params.backend_buffer2 : params.backend_buffer1;
                    const GLuint dst_tex = write_to_second ? params.backend_attachment2 : params.backend_attachment1;

                    draw_clip(params.ctx, dst_buf, post_src_tex, true);

                    post_src_tex = dst_tex;
                    clip_layer_tex = dst_tex;
                    clip_layer_buf = dst_buf;

                    write_to_second = !write_to_second;
                  }

                  e->SetShaderTargetSize(0, 0);
                }

                e->endEffect();
              }

              glEnable(GL_BLEND);
            }




            //
            //
            // PROCESS POST-SHADERS
            //
            //



            // The old shader-based blending path copied the whole frame into a second back buffer for
            // every clip. That path is disabled (blending is done with fixed-function GL blending
            // above) and nothing reads that buffer, so the copy is gone: it was a full-frame blit per
            // clip per frame.



            // == START FINAL DRAW ON SEQUENCE BUFFER ==




            // bind front buffer as draw buffer
            params.ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, final_fbo);

            const bool can_blend = (coords.blendmode > 0
                                    && params.blend_mode_program != nullptr
                                    && params.backend_buffer2 != 0
                                    && params.backend_attachment2 != 0);

            if (can_blend) {
              // Blend modes need the frame so far as a texture while the result is written, and
              // reading the buffer that is being drawn to is undefined. Snapshot the background
              // into the second back buffer first, then let the blend shader combine it with the
              // clip and write the finished frame.
              const GLuint background_texture = params.nests.isEmpty()
                  ? params.main_attachment
                  : params.nests.last()->fbo[0]->texture();

              // Snapshot the background into whichever backend buffer isn't holding the clip layer
              // (post-transform effects can leave it in either one).
              const GLuint blend_bg_buffer = (clip_layer_buf == params.backend_buffer2)
                  ? params.backend_buffer1
                  : params.backend_buffer2;
              const GLuint blend_bg_tex = (clip_layer_buf == params.backend_buffer2)
                  ? params.backend_attachment1
                  : params.backend_attachment2;

              params.ctx->functions()->glActiveTexture(GL_TEXTURE0);
              params.ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, blend_bg_buffer);

              glDisable(GL_BLEND);

              glBindTexture(GL_TEXTURE_2D, background_texture);
              full_blit();
              glBindTexture(GL_TEXTURE_2D, 0);

              params.ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, final_fbo);

              params.ctx->functions()->glActiveTexture(GL_TEXTURE0);
              glBindTexture(GL_TEXTURE_2D, blend_bg_tex);
              params.ctx->functions()->glActiveTexture(GL_TEXTURE1);
              glBindTexture(GL_TEXTURE_2D, clip_layer_tex);

              params.blend_mode_program->bind();
              params.blend_mode_program->setUniformValue("blendmode", coords.blendmode);
              params.blend_mode_program->setUniformValue("opacity", coords.opacity);
              params.blend_mode_program->setUniformValue("background", 0);
              params.blend_mode_program->setUniformValue("foreground", 1);

              full_blit();

              params.blend_mode_program->release();

              glBindTexture(GL_TEXTURE_2D, 0);
              params.ctx->functions()->glActiveTexture(GL_TEXTURE0);
              glBindTexture(GL_TEXTURE_2D, 0);

              glEnable(GL_BLEND);
            } else {
              // some GPUs don't like the blending shader, so we provide a pure GL fallback here

              params.ctx->functions()->glBindTexture(GL_TEXTURE_2D, clip_layer_tex);

              glColor4f(coords.opacity, coords.opacity, coords.opacity, coords.opacity);

              full_blit();

              params.ctx->functions()->glBindTexture(GL_TEXTURE_2D, 0);
            }
              /*
            } else {
              // load background texture into texture unit 0
              params.ctx->functions()->glActiveTexture(GL_TEXTURE0 + 0); // Texture unit 0
              params.ctx->functions()->glBindTexture(GL_TEXTURE_2D, backend_tex_2);

              // load foreground texture into texture unit 1
              params.ctx->functions()->glActiveTexture(GL_TEXTURE0 + 1); // Texture unit 1
              params.ctx->functions()->glBindTexture(GL_TEXTURE_2D, backend_tex_1);

              // bind and configure blending mode shader
              params.blend_mode_program->bind();
              params.blend_mode_program->setUniformValue("blendmode", coords.blendmode);
              params.blend_mode_program->setUniformValue("opacity", coords.opacity);
              params.blend_mode_program->setUniformValue("background", 0);
              params.blend_mode_program->setUniformValue("foreground", 1);

              glClear(GL_COLOR_BUFFER_BIT);

              full_blit();

              // release blend mode shader
              params.blend_mode_program->release();

              // unbind texture from texture unit 1
              params.ctx->functions()->glBindTexture(GL_TEXTURE_2D, 0);

              // unbind texture from texture unit 0
              params.ctx->functions()->glActiveTexture(GL_TEXTURE0 + 0); // Texture unit 0
              params.ctx->functions()->glBindTexture(GL_TEXTURE_2D, 0);
            }
            */

            // unbind framebuffer
            params.ctx->functions()->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);




            // == END FINAL DRAW ON SEQUENCE BUFFER ==
          }

          glPopMatrix();
        }
      } else {
        if (c->media() != nullptr && c->media()->get_type() == MEDIA_TYPE_SEQUENCE) {
          params.nests.append(c);
          compose_sequence(params);
          params.nests.removeLast();
        } else {
          // Check whether cacher is currently active, if not activate it now

          bool got_mutex2 = false;

          if (params.wait_for_mutexes) {
            c->cache_lock.lock();
            got_mutex2 = true;
          } else {
            got_mutex2 = c->cache_lock.tryLock(got_mutex2);
          }

          if (got_mutex2) {

            c->cache_lock.unlock();

            c->Cache(playhead,
                     (params.viewer != nullptr && !params.viewer->playing),
                     params.nests,
                     params.playback_speed);

          }
        }
      }
    } else {
      params.texture_failed = true;
    }

    if (got_mutex) {
      c->state_change_lock.unlock();
    }
  }

  if (audio_track_count == 0) {
    WakeAudioWakeObject();
  }

  if (params.video) {
    glPopMatrix();
  }

//  qDebug() << "compose sequence took" << QDateTime::currentMSecsSinceEpoch() - time;

  if (!params.nests.isEmpty() && params.nests.last()->fbo != nullptr) {
    // returns nested clip's texture
    return params.nests.last()->fbo[0]->texture();
  }

  return 0;
}

void olive::rendering::compose_audio(Viewer* viewer, Sequence* seq, int playback_speed, bool wait_for_mutexes) {
  ComposeSequenceParams params;
  params.viewer = viewer;
  params.ctx = nullptr;
  params.seq = seq;
  params.video = false;
  params.gizmos = nullptr;
  params.wait_for_mutexes = wait_for_mutexes;
  params.playback_speed = playback_speed;
  params.blend_mode_program = nullptr;
  params.divider = 1;
  compose_sequence(params);
}

long rescale_frame_number(long framenumber, double source_frame_rate, double target_frame_rate) {
  return qRound((double(framenumber)/source_frame_rate)*target_frame_rate);
}

double get_timecode(Clip* c, long playhead) {
  return double(playhead_to_clip_frame(c, playhead))/c->sequence->frame_rate;
}

long playhead_to_clip_frame(Clip* c, long playhead) {
  return (qMax(0L, playhead - c->timeline_in(true)) + c->clip_in(true));
}

double playhead_to_clip_seconds(Clip* c, long playhead) {
  // returns time in seconds
  long clip_frame = playhead_to_clip_frame(c, playhead);

  if (c->reversed()) {
    clip_frame = c->media_length() - clip_frame - 1;
  }

  double secs = (double(clip_frame)/c->sequence->frame_rate)*c->speed().value;
  if (c->media() != nullptr && c->media()->get_type() == MEDIA_TYPE_FOOTAGE) {
    secs *= c->media()->to_footage()->speed;
  }

  return secs;
}

int64_t seconds_to_timestamp(Clip *c, double seconds) {
  return qRound64(seconds * av_q2d(av_inv_q(c->time_base())));
}

int64_t playhead_to_timestamp(Clip* c, long playhead) {
  return seconds_to_timestamp(c, playhead_to_clip_seconds(c, playhead));
}

void close_active_clips(Sequence* s) {
  if (s != nullptr) {
    for (int i=0;i<s->clips.size();i++) {
      Clip* c = s->clips.at(i).get();
      if (c != nullptr) {
        c->Close(true);
      }
    }
  }
}
