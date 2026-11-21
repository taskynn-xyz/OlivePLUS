#version 110

// Shockwave: concentric rings travelling out from a point, bending the image
// and splitting the channels as they pass.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float cx;
uniform float cy;
uniform float strength;
uniform float frequency;
uniform float speed;
uniform float chroma;

void main(void) {
	float ar = resolution.x / resolution.y;
	vec2 center = vec2(0.5 + cx, 0.5 + cy);
	vec2 d = vTexCoord - center;

	float dist = length(vec2(d.x * ar, d.y));
	float phase = dist * frequency - time * speed * 6.2831853;
	float amp = sin(phase) * strength * 0.06 * exp(-dist * 2.5);

	vec2 dir = vec2(0.0);
	if (dist > 0.00001) {
		dir = vec2(d.x, d.y) / dist;
	}

	vec2 uv = vTexCoord + dir * amp;
	vec2 co = vec2(chroma / resolution.x, 0.0);

	vec4 col;
	col.r = texture2D(tex, clamp(uv + co, vec2(0.0), vec2(1.0))).r;
	col.g = texture2D(tex, clamp(uv, vec2(0.0), vec2(1.0))).g;
	col.b = texture2D(tex, clamp(uv - co, vec2(0.0), vec2(1.0))).b;
	col.a = texture2D(tex, clamp(uv, vec2(0.0), vec2(1.0))).a;

	// faint blue crest on the rings
	col.rgb += vec3(0.15, 0.35, 0.9) * max(0.0, sin(phase)) * strength * 0.06;

	gl_FragColor = col;
}
