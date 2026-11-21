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

#ifndef EFFECT_H
#define EFFECT_H

#include <memory>
#include <QObject>
#include <QString>
#include <QVector>
#include <QColor>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMutex>
#include <QThread>
#include <QLabel>
#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QMouseEvent>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <random>

#include "ui/collapsiblewidget.h"
#include "effectrow.h"
#include "effectgizmo.h"

class Clip;

class Effect;
using EffectPtr = std::shared_ptr<Effect>;

struct EffectMeta {
  QString name;
  QString category;
  QString filename;
  QString path;
  QString tooltip;
  int internal;
  int type;
  int subtype;
};
extern QVector<EffectMeta> effects;

double log_volume(double linear);

enum EffectType {
  EFFECT_TYPE_INVALID,
  EFFECT_TYPE_VIDEO,
  EFFECT_TYPE_AUDIO,
  EFFECT_TYPE_EFFECT,
  EFFECT_TYPE_TRANSITION
};

enum EffectKeyframeType {
  EFFECT_KEYFRAME_LINEAR,
  EFFECT_KEYFRAME_BEZIER,
  EFFECT_KEYFRAME_HOLD
};

/**
 * @brief Clip blending modes
 *
 * The order of this enum is the contract with effects/internal/blending.frag, which implements the
 * modes as the integer uniform `blendmode`. Modes are the standard set every editor and compositor
 * offers (After Effects / Photoshop names and maths).
 *
 * BLEND_MODE_NORMAL (0) is the default and bypasses the blend shader entirely, so clips that don't
 * use blending pay nothing for it.
 */
enum BlendMode {
  BLEND_MODE_NORMAL = 0,
  BLEND_MODE_DARKEN,
  BLEND_MODE_MULTIPLY,
  BLEND_MODE_COLOR_BURN,
  BLEND_MODE_LINEAR_BURN,
  BLEND_MODE_DARKER_COLOR,
  BLEND_MODE_LIGHTEN,
  BLEND_MODE_SCREEN,
  BLEND_MODE_COLOR_DODGE,
  BLEND_MODE_LINEAR_DODGE,
  BLEND_MODE_LIGHTER_COLOR,
  BLEND_MODE_OVERLAY,
  BLEND_MODE_SOFT_LIGHT,
  BLEND_MODE_HARD_LIGHT,
  BLEND_MODE_VIVID_LIGHT,
  BLEND_MODE_LINEAR_LIGHT,
  BLEND_MODE_PIN_LIGHT,
  BLEND_MODE_HARD_MIX,
  BLEND_MODE_DIFFERENCE,
  BLEND_MODE_EXCLUSION,
  BLEND_MODE_SUBTRACT,
  BLEND_MODE_DIVIDE,
  BLEND_MODE_HUE,
  BLEND_MODE_SATURATION,
  BLEND_MODE_COLOR,
  BLEND_MODE_LUMINOSITY,
  BLEND_MODE_COUNT
};

enum EffectInternal {
  EFFECT_INTERNAL_TRANSFORM,
  EFFECT_INTERNAL_TEXT,
  EFFECT_INTERNAL_SOLID,
  EFFECT_INTERNAL_NOISE,
  EFFECT_INTERNAL_VOLUME,
  EFFECT_INTERNAL_PAN,
  EFFECT_INTERNAL_TONE,
  EFFECT_INTERNAL_SHAKE,
  EFFECT_INTERNAL_TIMECODE,
  EFFECT_INTERNAL_MASK,
  EFFECT_INTERNAL_FILLLEFTRIGHT,
  EFFECT_INTERNAL_VST,
  EFFECT_INTERNAL_CORNERPIN,
  EFFECT_INTERNAL_FREI0R,
  EFFECT_INTERNAL_RICHTEXT,
  EFFECT_INTERNAL_TRANSFORM_3D,
  EFFECT_INTERNAL_TITLE,
  EFFECT_INTERNAL_SHAPE,
  EFFECT_INTERNAL_CAMERA,
  EFFECT_INTERNAL_COUNT
};

struct GLTextureCoords {
  int grid_size;

  int vertexTopLeftX;
  int vertexTopLeftY;
  int vertexTopLeftZ;
  int vertexTopRightX;
  int vertexTopRightY;
  int vertexTopRightZ;
  int vertexBottomLeftX;
  int vertexBottomLeftY;
  int vertexBottomLeftZ;
  int vertexBottomRightX;
  int vertexBottomRightY;
  int vertexBottomRightZ;

  float textureTopLeftX;
  float textureTopLeftY;
  float textureTopLeftQ;
  float textureTopRightX;
  float textureTopRightY;
  float textureTopRightQ;
  float textureBottomRightX;
  float textureBottomRightY;
  float textureBottomRightQ;
  float textureBottomLeftX;
  float textureBottomLeftY;
  float textureBottomLeftQ;

  int blendmode;
  float opacity;

  // preview resolution divisor the framebuffer being drawn into was created with (1 = full).
  // Effect shaders need it to describe their render target accurately.
  int divider;
};

const EffectMeta* get_meta_from_name(const QString& input);

/**
 * @brief Localized display name for a blend mode (index into the BlendMode enum)
 */
QString blend_mode_name(int mode);

qint16 mix_audio_sample(qint16 a, qint16 b);

class Effect : public QObject {
  Q_OBJECT
public:
  Effect(Clip *c, const EffectMeta* em);
  ~Effect();

  Clip* parent_clip;
  const EffectMeta* meta;
  int id;
  QString name;

  void AddRow(EffectRow* row);

  EffectRow* row(int i);
  int row_count();

  EffectGizmo* add_gizmo(int type);
  EffectGizmo* gizmo(int i);
  int gizmo_count();

  bool IsEnabled();
  bool IsExpanded();

  virtual void refresh();

  virtual EffectPtr copy(Clip* c);
  void copy_field_keyframes(EffectPtr e);

  virtual void load(QXmlStreamReader& stream);
  virtual void custom_load(QXmlStreamReader& stream);
  virtual void save(QXmlStreamWriter& stream);

  void load_from_string(const QByteArray &s);
  QByteArray save_to_string();

  // glsl handling
  bool is_open();
  void open();
  void close();
  bool is_glsl_linked();
  virtual void startEffect();
  virtual void endEffect();

  enum VideoEffectFlags {
    ShaderFlag        = 0x1,
    CoordsFlag        = 0x2,
    SuperimposeFlag   = 0x4,
    ImageFlag         = 0x8,

    // Run the shader pass after the rest of the clip's effects instead of at this effect's position
    // in the chain. Used by effects that need the fully composited clip texture (e.g. generator
    // clips, where the image is produced by a later effect).
    PostShaderFlag    = 0x10
  };
  int Flags();
  void SetFlags(int flags);

  int getIterations();
  void setIterations(int i);

  /**
   * @brief Override the buffer size reported to the shader's `resolution` uniform
   *
   * Effects that run after the transform render into a sequence-sized buffer rather than the
   * clip's own framebuffer, so their shaders need the size of that buffer. Passing 0 restores the
   * default (the clip's media size).
   */
  void SetShaderTargetSize(int width, int height);

  const char* ffmpeg_filter;

  virtual void process_image(double timecode, uint8_t* input, uint8_t* output, int size);
  virtual void process_shader(double timecode, GLTextureCoords&, int iteration);
  virtual void process_coords(double timecode, GLTextureCoords& coords, int data);

  /**
   * @brief Get the transform this effect applies to its clip, as a 4x4 matrix
   *
   * Used for parenting: a child clip has its parent's transform applied on top of its own, so a
   * parent null layer moves, rotates and scales every clip parented to it. Only transform effects
   * return true here.
   *
   * @param timecode
   *
   * Clip time to evaluate any keyframes at.
   *
   * @param matrix
   *
   * 16 doubles to write the matrix into, in the column-major order glMultMatrixd() expects.
   *
   * @return
   *
   * TRUE if this effect transforms the clip, FALSE (the default) if it leaves geometry alone.
   */
  virtual bool GetTransformMatrix(double timecode, double* matrix);
  virtual GLuint process_superimpose(double timecode);
  virtual void process_audio(double timecode_start, double timecode_end, quint8* samples, int nb_bytes, int channel_count);

  virtual void gizmo_draw(double timecode, GLTextureCoords& coords);
  void gizmo_move(EffectGizmo* sender, int x_movement, int y_movement, double timecode, bool done);
  void gizmo_world_to_screen();
  bool are_gizmos_enabled();

  template <typename T>
  T randomNumber()
  {
    static std::random_device device;
    static std::mt19937 generator(device());
    static std::uniform_int_distribution<> distribution(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    return distribution(generator);
  }

  static EffectPtr Create(Clip *c, const EffectMeta *em);
  static const EffectMeta* GetInternalMeta(int internal_id, int type);
public slots:
  void FieldChanged();
  void SetEnabled(bool b);
  void SetExpanded(bool e);
signals:
  void EnabledChanged(bool);
private slots:
  void delete_self();
  void move_up();
  void move_down();
  void save_to_file();
  void load_from_file();
protected:
  // glsl effect
  QOpenGLShaderProgram* glslProgram;
  QString vertPath;
  QString fragPath;

  // superimpose effect
  QImage img;
  QOpenGLTexture* texture;

  // enable effect to update constantly
  virtual bool AlwaysUpdate();

private:
  bool isOpen;
  QVector<EffectRow*> rows;
  QVector<EffectGizmo*> gizmos;
  bool bound;
  int iterations;
  int shader_target_width;
  int shader_target_height;

  bool enabled_;
  bool expanded_;

  int flags_;

  QVector<KeyframeDataChange*> gizmo_dragging_actions_;

  // superimpose functions
  virtual void redraw(double timecode);
  bool valueHasChanged(double timecode);
  QVector<QVariant> cachedValues;
  void delete_texture();
  void validate_meta_path();
};

#endif // EFFECT_H
