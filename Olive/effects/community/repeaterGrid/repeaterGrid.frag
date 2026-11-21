#version 110

// Repeater Grid: tiles a copy of the frame into a rounded, gapped grid. Animate the offsets for
// an endlessly scrolling wall of screens.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float columns;
uniform float rows;
uniform float gap;
uniform float roundness;
uniform float offset_x;
uniform float offset_y;

void main(void) {
	vec2 grid = vec2(max(1.0, floor(columns)), max(1.0, floor(rows)));

	vec2 gv = vTexCoord * grid + vec2(offset_x, offset_y);
	vec2 t = fract(gv);

	float g = clamp(gap, 0.0, 0.9) * 0.5;
	float r = clamp(roundness, 0.0, 1.0) * 0.18;

	vec2 q = abs(t - 0.5) - (vec2(0.5 - g) - vec2(r));
	float sd = min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;

	float mask = 1.0 - smoothstep(-0.0015, 0.0015, sd);

	vec4 col = texture2D(tex, clamp(t, vec2(0.0), vec2(1.0)));

	gl_FragColor = vec4(col.rgb * mask, col.a * mask);
}
