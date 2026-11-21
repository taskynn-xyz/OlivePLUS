#version 110

// Remaps the image onto a thermal camera palette

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;

uniform float contrast;
uniform float glow;
uniform float shift;

vec3 thermal(float v) {
	v = clamp(v + shift, 0.0, 1.0);
	vec3 c0 = vec3(0.02, 0.02, 0.10);
	vec3 c1 = vec3(0.45, 0.05, 0.35);
	vec3 c2 = vec3(0.85, 0.15, 0.15);
	vec3 c3 = vec3(1.0, 0.55, 0.05);
	vec3 c4 = vec3(1.0, 0.98, 0.75);

	if (v < 0.25) return mix(c0, c1, v / 0.25);
	if (v < 0.50) return mix(c1, c2, (v - 0.25) / 0.25);
	if (v < 0.75) return mix(c2, c3, (v - 0.50) / 0.25);
	return mix(c3, c4, (v - 0.75) / 0.25);
}

float luma(vec3 c) {
	return dot(c, vec3(0.299, 0.587, 0.114));
}

void main(void) {
	vec2 px = 2.0 / resolution;

	vec3 c = texture2D(tex, vTexCoord).rgb;

	// cheap bloom so hot areas bleed
	vec3 spread = vec3(0.0);
	spread += texture2D(tex, vTexCoord + vec2(px.x, 0.0)).rgb;
	spread += texture2D(tex, vTexCoord - vec2(px.x, 0.0)).rgb;
	spread += texture2D(tex, vTexCoord + vec2(0.0, px.y)).rgb;
	spread += texture2D(tex, vTexCoord - vec2(0.0, px.y)).rgb;
	spread *= 0.25;

	float heat = pow(clamp(luma(c) * contrast, 0.0, 1.0), 1.4);
	float bleeding = luma(spread) * glow;

	gl_FragColor = vec4(thermal(heat + bleeding), texture2D(tex, vTexCoord).a);
}
