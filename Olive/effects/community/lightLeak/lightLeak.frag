#version 110

// Light Leak: warm film-leak streaks drifting across the frame, for transitions, title cards and
// overlays.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float intensity;
uniform float width;
uniform float speed;
uniform vec3 tint;

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	float t = time * speed;
	float w = max(0.2, width);

	float band_a = exp(-pow((vTexCoord.x + vTexCoord.y * 0.45 - fract(t * 0.1) * 1.6 + 0.3) * w, 2.0));
	float band_b = exp(-pow((vTexCoord.x * 1.3 - vTexCoord.y * 0.5 - fract(t * 0.07 + 0.4) * 1.8) * w * 1.7, 2.0));

	float leak = (band_a * 0.8 + band_b * 0.5) * intensity * 0.7;

	vec3 rgb = col.rgb + tint * leak * (1.0 - col.rgb);

	gl_FragColor = vec4(clamp(rgb, vec3(0.0), vec3(1.0)), col.a);
}
