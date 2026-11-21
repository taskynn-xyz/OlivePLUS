#version 110

// Threshold: crushes the image to two tones. `softness` widens the ramp so it can also act as a
// gentle high-contrast matte generator.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float threshold;
uniform float softness;
uniform bool invert;
uniform vec3 thresholdcolor;

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	float luma = dot(base.rgb, vec3(0.2126, 0.7152, 0.0722));
	float value = smoothstep(threshold - softness, threshold + softness, luma);

	if (invert) {
		value = 1.0 - value;
	}

	gl_FragColor = vec4(thresholdcolor * value, base.a);
}
