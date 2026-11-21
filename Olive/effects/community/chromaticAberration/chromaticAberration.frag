#version 110

// Chromatic Aberration: offsets the red and blue channels apart along an angle, the lens-fringe
// look used for intros, glitch sections and logo hits.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform float amount;
uniform float angle;

void main(void) {
	float a = radians(angle);
	vec2 dir = vec2(cos(a), sin(a)) * amount * 0.015;

	vec4 base = texture2D(tex, vTexCoord);

	vec4 col;
	col.r = texture2D(tex, vTexCoord + dir).r;
	col.g = base.g;
	col.b = texture2D(tex, vTexCoord - dir).b;
	col.a = base.a;

	gl_FragColor = col;
}
