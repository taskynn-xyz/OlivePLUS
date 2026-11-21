#version 120

/*
  Motion blur accumulation pass for Olive's internal Transform and Shake effects.

  For every sub-frame sample the CPU computes an affine map from the current
  frame's texture coordinates to the texture coordinates the same image
  content had at that sample time (see motionblur::BuildSamples()). Averaging
  the samples gives a box-filter (uniform shutter) motion blur.

  The texture is premultiplied, so straight averaging of RGB and A is correct.
*/

#define MAX_SAMPLES 64

uniform sampler2D tex;

uniform int mbCount;
uniform vec4 mbMat[MAX_SAMPLES]; // (m00, m01, m10, m11) in texture space
uniform vec3 mbOff[MAX_SAMPLES]; // (offset.x, offset.y, unused)

varying vec2 vTexCoord;

void main(void) {
  vec4 center = texture2D(tex, vTexCoord);

  if (mbCount < 2) {
    gl_FragColor = center;
    return;
  }

  vec4 sum = vec4(0.0);
  float weight = 0.0;

  for (int i = 0; i < MAX_SAMPLES; i++) {
    if (i >= mbCount) break;

    vec4 m = mbMat[i];
    vec2 uv = vec2(m.x * vTexCoord.x + m.y * vTexCoord.y,
                   m.z * vTexCoord.x + m.w * vTexCoord.y) + mbOff[i].xy;

    // Samples that fall outside the frame are dropped instead of clamped so
    // the clip's edges don't smear into the surrounding transparent area.
    if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0) {
      sum += texture2D(tex, uv);
      weight += 1.0;
    }
  }

  if (weight > 0.0) {
    gl_FragColor = sum / weight;
  } else {
    gl_FragColor = center;
  }
}
