#version 110

// Mirror World: folds the frame into mirrored wedges and concentric rings so
// the image repeats into itself.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float segments;
uniform float zoom;
uniform float spin;
uniform float rings;

void main(void) {
	float ar = resolution.x / resolution.y;
	vec2 c = (vTexCoord - 0.5) * vec2(ar, 1.0);

	float a = atan(c.y, c.x) + time * spin * 6.2831853;
	float rad = length(c) * zoom;

	float seg = 6.2831853 / max(2.0, floor(segments));
	a = abs(mod(a + seg * 0.5, seg) - seg * 0.5);

	if (rings > 0.5) {
		rad = abs(fract(rad + time * 0.15) - 0.5) * 2.0;
	}

	vec2 p = vec2(cos(a), sin(a)) * rad * 0.8;
	vec2 uv = 0.5 + vec2(p.x / ar, p.y);

	gl_FragColor = texture2D(tex, clamp(uv, vec2(0.0), vec2(1.0)));
}
