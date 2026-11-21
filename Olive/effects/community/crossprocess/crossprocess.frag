#version 110

// Cross Process: each channel is run through a different curve, the way C-41 film develops in
// E-6 chemistry, then the shadows and highlights are split-toned.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float amount;
uniform float contrast;
uniform vec3 shadowtint;
uniform vec3 highlighttint;

float channel_curve(float value, float black, float white) {
	return clamp(smoothstep(black, white, value) * (1.0 + contrast) - (contrast * 0.5), 0.0, 1.0);
}

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	vec3 graded;
	graded.r = channel_curve(base.r, 0.0, 0.85);
	graded.g = channel_curve(base.g, 0.05, 1.0);
	graded.b = channel_curve(base.b, 0.15, 1.0);

	float luma = dot(graded, vec3(0.2126, 0.7152, 0.0722));
	graded += (shadowtint - vec3(0.5)) * (1.0 - luma) * 0.25;
	graded += (highlighttint - vec3(0.5)) * luma * 0.25;

	gl_FragColor = vec4(mix(base.rgb, clamp(graded, 0.0, 1.0), amount), base.a);
}
