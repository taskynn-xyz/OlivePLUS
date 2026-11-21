#version 110

// Paint: Kuwahara-style filter that keeps the flattest of four windows, giving
// an oil painting look with soft edges and flat colour areas.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;

uniform float radius;
uniform float mix_amount;

void window(vec2 uv, vec2 o, out vec3 mean, out float variance) {
	vec3 s = vec3(0.0);
	vec3 s2 = vec3(0.0);

	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec3 c = texture2D(tex, uv + vec2(float(x) * o.x, float(y) * o.y)).rgb;
			s += c;
			s2 += c * c;
		}
	}

	mean = s / 9.0;
	vec3 v = s2 / 9.0 - mean * mean;
	variance = v.r + v.g + v.b;
}

void main(void) {
	vec2 o = vec2(radius) / resolution;
	vec2 q = o * 0.5;

	vec3 m1, m2, m3, m4;
	float v1, v2, v3, v4;

	window(vTexCoord + vec2(-q.x, -q.y), o, m1, v1);
	window(vTexCoord + vec2( q.x, -q.y), o, m2, v2);
	window(vTexCoord + vec2(-q.x,  q.y), o, m3, v3);
	window(vTexCoord + vec2( q.x,  q.y), o, m4, v4);

	vec3 paint_col = m1;
	float best = v1;

	if (v2 < best) { best = v2; paint_col = m2; }
	if (v3 < best) { best = v3; paint_col = m3; }
	if (v4 < best) { paint_col = m4; }

	vec4 orig = texture2D(tex, vTexCoord);

	gl_FragColor = vec4(mix(orig.rgb, paint_col, clamp(mix_amount, 0.0, 1.0)), orig.a);
}
