#version 110

// Traces the picture in glowing neon lines using a sobel edge detector.
//
// `method` chooses how the detected edge becomes light:
//   0 Soft Halo  - the edge spread over a wide ring kernel, so the lines glow instead of aliasing
//   1 Crisp Lines - the plain edge lines, hard and graphic
//   2 Wide Bloom - a crisp core plus a much wider, softer spread around it
//
// The halo is gathered from jittered rings: a fixed set of directions would leave a star of spokes
// between neighbouring pixels, which is what made the glow look streaky.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;

uniform float thickness;
uniform float threshold;
uniform vec3 glow_color;
uniform float intensity;
uniform float base_amount;
uniform int method;

const float TWO_PI = 6.28318531;
const float GOLDEN_ANGLE = 2.39996323;

const int TAPS = 20;
const int RINGS = 4;

float luma(vec3 c) {
	return dot(c, vec3(0.299, 0.587, 0.114));
}

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float edge_at(vec2 uv) {
	vec2 px = thickness / resolution;

	float tl = luma(texture2D(tex, uv + vec2(-px.x, -px.y)).rgb);
	float t  = luma(texture2D(tex, uv + vec2(0.0, -px.y)).rgb);
	float tr = luma(texture2D(tex, uv + vec2(px.x, -px.y)).rgb);
	float ml = luma(texture2D(tex, uv + vec2(-px.x, 0.0)).rgb);
	float mr = luma(texture2D(tex, uv + vec2(px.x, 0.0)).rgb);
	float bl = luma(texture2D(tex, uv + vec2(-px.x, px.y)).rgb);
	float bb = luma(texture2D(tex, uv + vec2(0.0, px.y)).rgb);
	float br = luma(texture2D(tex, uv + vec2(px.x, px.y)).rgb);

	float gx = (tr + 2.0 * mr + br) - (tl + 2.0 * ml + bl);
	float gy = (bl + 2.0 * bb + br) - (tl + 2.0 * t + tr);

	return smoothstep(threshold, threshold + 0.35, length(vec2(gx, gy)));
}

float halo(vec2 uv, float radius_px) {
	float jitter = hash(gl_FragCoord.xy) * TWO_PI;

	float total = 0.0;
	float weight_sum = 0.0;

	for (int ring=1;ring<=RINGS;ring++) {
		float ring_fraction = float(ring) / float(RINGS);
		float weight = 1.0 - (ring_fraction * 0.5);
		float ring_rotation = jitter + (float(ring) * GOLDEN_ANGLE);

		for (int i=0;i<TAPS;i++) {
			float angle = ring_rotation + (float(i) * (TWO_PI / float(TAPS)));
			vec2 offset = vec2(cos(angle), sin(angle)) * (ring_fraction * radius_px) / resolution;

			total += edge_at(uv + offset) * weight;
			weight_sum += weight;
		}
	}

	return total / max(weight_sum, 0.0001);
}

void main(void) {
	vec4 src = texture2D(tex, vTexCoord);

	float edge;
	if (method == 1) {
		edge = edge_at(vTexCoord);
	} else if (method == 2) {
		edge = max(edge_at(vTexCoord) * 0.7, halo(vTexCoord, thickness * 14.0) * 1.4);
	} else {
		edge = max(edge_at(vTexCoord) * 0.5, halo(vTexCoord, thickness * 5.0) * 1.6);
	}

	vec3 col = src.rgb * base_amount + glow_color * edge * intensity;
	gl_FragColor = vec4(clamp(col, 0.0, 1.0), src.a);
}
