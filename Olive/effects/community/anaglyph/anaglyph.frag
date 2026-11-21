#version 110

// Anaglyph 3D: offsets the red and cyan channels in opposite directions so the
// footage can be viewed with red/cyan glasses.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;

uniform float distance_px;
uniform float angle;
uniform float swap;

void main(void) {
	vec2 d = vec2(cos(angle), sin(angle)) * distance_px / resolution;

	vec4 l = texture2D(tex, clamp(vTexCoord - d, vec2(0.0), vec2(1.0)));
	vec4 r = texture2D(tex, clamp(vTexCoord + d, vec2(0.0), vec2(1.0)));

	vec4 out_col;
	if (swap > 0.5) {
		out_col = vec4(r.r, l.g, l.b, max(l.a, r.a));
	} else {
		out_col = vec4(l.r, r.g, r.b, max(l.a, r.a));
	}

	gl_FragColor = out_col;
}
