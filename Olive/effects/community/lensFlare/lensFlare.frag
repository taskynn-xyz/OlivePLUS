#version 110

// Lens Flare: picks up the bright parts of the frame and adds an anamorphic
// streak plus a chain of ghost reflections through the frame centre.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float threshold;
uniform float streak;
uniform float ghosts;
uniform float glow;

float luma(vec3 c) {
	return dot(c, vec3(0.299, 0.587, 0.114));
}

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);
	vec3 col = base.rgb;

	float ar = resolution.x / resolution.y;
	vec2 dir = vec2(0.5) - vTexCoord;

	// anamorphic streak
	for (int i = 1; i <= 16; i++) {
		float f = float(i) / 16.0;
		vec2 o = vec2(f * streak * 0.6 / ar, 0.0);
		float b = smoothstep(threshold, threshold + 0.3, luma(texture2D(tex, clamp(vTexCoord + o, vec2(0.0), vec2(1.0))).rgb))
		        + smoothstep(threshold, threshold + 0.3, luma(texture2D(tex, clamp(vTexCoord - o, vec2(0.0), vec2(1.0))).rgb));
		col += vec3(0.3, 0.55, 1.0) * b * (1.0 - f) * 0.22;
	}

	// ghost reflections along the line through the centre
	for (int i = 1; i <= 6; i++) {
		float f = float(i) / 6.0;
		vec2 guv = vTexCoord + dir * f * ghosts * 2.0;
		vec4 g = texture2D(tex, clamp(guv, vec2(0.0), vec2(1.0)));
		float gl = smoothstep(threshold, threshold + 0.4, luma(g.rgb));
		col += g.rgb * gl * (1.0 - f) * 0.14;
	}

	// warm halo around the centre
	float halo = pow(max(0.0, 1.0 - length(dir) * 1.6), 4.0);
	col += vec3(1.0, 0.82, 0.55) * halo * glow * 0.25;

	gl_FragColor = vec4(clamp(col, vec3(0.0), vec3(1.0)), base.a);
}
