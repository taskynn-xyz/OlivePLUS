#version 110

// Gradient Sweep: paints a linear gradient across the layer, inside the layer's alpha only.
//
// "Keep Luminance" multiplies the gradient by the layer's own luminance, so the glyph shapes and
// any shading in the text still read through the colour; turning it off gives a flat gradient fill.
// Alpha is passed through untouched, so nothing is drawn outside the layer.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec3 colorA;
uniform vec3 colorB;
uniform float angle;
uniform float offset;
uniform int keepLuma;
uniform float amount;

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);
	float a = col.a;

	vec3 rgb = col.rgb / max(a, 0.0001);

	const float to_radians = 0.0174532925;
	float radians_value = angle * to_radians;
	vec2 direction = vec2(cos(radians_value), sin(radians_value));

	float t = clamp(dot(vTexCoord - vec2(0.5), direction) + 0.5 + offset, 0.0, 1.0);
	vec3 sweep = mix(colorA, colorB, t);

	if (keepLuma != 0) {
		float l = clamp(dot(rgb, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
		sweep *= l;
	}

	vec3 styled = mix(rgb, sweep, clamp(amount, 0.0, 1.0));

	gl_FragColor = vec4(styled * a, a);
}
