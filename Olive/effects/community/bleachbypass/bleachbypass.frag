#version 110

// Bleach Bypass: the silver-retention look - desaturated, contrasty and slightly metallic.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float amount;
uniform float saturation;
uniform float contrast;
uniform float lift;

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	float luma = dot(base.rgb, vec3(0.2126, 0.7152, 0.0722));
	vec3 graded = mix(vec3(luma), base.rgb, saturation);

	// an S-curve around mid grey gives the crunchy highlight roll-off
	graded = ((graded - 0.5) * (1.0 + contrast)) + 0.5 + lift;

	// a touch of metallic blue in the shadows
	graded *= vec3(0.98, 0.99, 1.03);
	graded = clamp(graded, 0.0, 1.0);

	gl_FragColor = vec4(mix(base.rgb, graded, amount), base.a);
}
