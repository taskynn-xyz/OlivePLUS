#version 110

// Inner Glow: glows inwards from the edge of the layer's alpha.
//
// Every sample looks at the alpha around the pixel, so the glow only appears next to the boundary
// of the layer and fades as it moves inside. The finished glow is premultiplied by the layer's own
// alpha, which keeps it strictly inside a text layer instead of haloing around it.
//
// `method` chooses how the inside/outside boundary is gathered:
//   0 Soft        - many jittered directions, averaged: a smooth falloff with no visible spokes
//   1 Crisp       - the nearest outside alpha wins in every direction, for a hard inner line
//   2 Directional - the soft gather, weighted towards light coming from the top left
//
// The per-pixel angle jitter is what removes the streaking: with a fixed set of directions the
// gather leaves a star of spokes around every glyph.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec2 resolution;

uniform vec3 colour;
uniform float radius;
uniform float intensity;
uniform float amount;
uniform int method;

const float TWO_PI = 6.28318531;
const int DIRECTIONS = 16;
const int STEPS = 8;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);
	float a = col.a;

	vec2 pixel = vec2(1.0) / max(resolution, vec2(1.0, 1.0));
	float radius_px = max(radius, 1.0);

	float jitter = hash(gl_FragCoord.xy) * TWO_PI;

	float total = 0.0;
	float total_weight = 0.0;

	for (int i = 0; i < DIRECTIONS; i++) {
		float angle = jitter + (float(i) * (TWO_PI / float(DIRECTIONS)));
		vec2 direction = vec2(cos(angle), sin(angle));

		float direction_weight = 1.0;
		if (method == 2) {
			// light coming from the top left, i.e. from (-1, -1)
			float facing = max(dot(direction, vec2(-0.70710678, -0.70710678)), 0.0);
			direction_weight = 0.2 + (0.8 * facing);
		}

		float falloff = 0.0;
		float falloff_weight = 0.0;

		for (int j = 1; j <= STEPS; j++) {
			float distance_px = (float(j) / float(STEPS)) * radius_px;
			float neighbour = texture2D(tex, clamp(vTexCoord + direction * (pixel * distance_px), 0.0, 1.0)).a;
			float inside = 1.0 - neighbour;
			float weight = 1.0 - ((float(j) - 1.0) / float(STEPS));

			if (method == 1) {
				// crisp: the closest piece of outside wins, which gives a harder inner edge
				falloff = max(falloff, inside * weight);
			} else {
				falloff += inside * weight;
				falloff_weight += weight;
			}
		}

		if (method != 1) {
			falloff /= max(falloff_weight, 0.0001);
		}

		total += falloff * direction_weight;
		total_weight += direction_weight;
	}

	float glow = total / max(total_weight, 0.0001);

	vec3 rgb = col.rgb / max(a, 0.0001);
	vec3 styled = mix(rgb, rgb + (colour * glow * intensity), clamp(amount, 0.0, 1.0));

	gl_FragColor = vec4(styled * a, a);
}
