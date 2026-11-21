#version 110

// Alpha Bevel: lights the edge of the layer's alpha, which gives text and shapes an embossed,
// dimensional look.
//
// The bevel comes from the gradient of the alpha channel, so it can only appear where the alpha
// actually changes, and the result is premultiplied by the centre alpha. Text therefore gets a
// lit/shadowed rim on the glyphs and nothing at all is drawn outside them.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec2 resolution;

uniform vec3 highlight;
uniform vec3 shadow;
uniform float angle;
uniform float depth;
uniform float strength;
uniform float amount;

float layer_alpha(vec2 uv) {
	return texture2D(tex, clamp(uv, 0.0, 1.0)).a;
}

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);
	float a = col.a;

	vec2 texel = vec2(max(depth, 0.5)) / max(resolution, vec2(1.0, 1.0));

	float gx = layer_alpha(vTexCoord + vec2(texel.x, 0.0)) - layer_alpha(vTexCoord - vec2(texel.x, 0.0));
	float gy = layer_alpha(vTexCoord + vec2(0.0, texel.y)) - layer_alpha(vTexCoord - vec2(0.0, texel.y));

	const float to_radians = 0.0174532925;
	float radians_value = angle * to_radians;
	vec2 light_direction = vec2(cos(radians_value), sin(radians_value));

	// how strongly this pixel faces the light
	float lit = (gx * light_direction.x + gy * light_direction.y) * strength;

	// only the rim of a glyph is lit: the flat middle of the alpha has no gradient at all
	float rim = smoothstep(0.0, 0.6, abs(gx) + abs(gy));

	vec3 shade = mix(shadow, highlight, clamp(0.5 + (lit * 0.5), 0.0, 1.0));

	vec3 rgb = col.rgb / max(a, 0.0001);
	vec3 styled = rgb * mix(vec3(1.0), shade * 2.0, clamp(amount, 0.0, 1.0) * rim);

	gl_FragColor = vec4(styled * a, a);
}
