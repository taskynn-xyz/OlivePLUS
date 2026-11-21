#version 110

// Pixel Stretch: smears the bright parts of the frame into long horizontal
// streaks, the "pixel sorting" look of datamoshed edits.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float threshold;
uniform float stretch;
uniform float reverse;
uniform float smear;

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);
	float lum = dot(base.rgb, vec3(0.299, 0.587, 0.114));
	float gate = smoothstep(threshold, threshold + 0.08, lum);

	float dir = mix(1.0, -1.0, clamp(reverse, 0.0, 1.0));
	vec3 acc = base.rgb;

	for (int i = 1; i <= 32; i++) {
		float f = float(i) / 32.0;
		vec2 uv = vTexCoord + vec2(f * stretch * 0.6 * dir, 0.0);
		if (uv.x >= 0.0 && uv.x <= 1.0) {
			vec4 s = texture2D(tex, uv);
			float sl = dot(s.rgb, vec3(0.299, 0.587, 0.114));
			float m = smoothstep(threshold, threshold + 0.35, sl) * (1.0 - f);
			acc += max(s.rgb - base.rgb, vec3(0.0)) * m * gate * smear;
		}
	}

	gl_FragColor = vec4(clamp(acc, 0.0, 1.0), base.a);
}
