#version 110

// Deep Glow: a pyramid of progressively blurred copies of the bright pass.
//
// The bright parts of the frame are blurred with a real Gaussian at Radius, then again with a
// larger Gaussian, and so on up to Deep Radius; the levels are added together. Each level is
// softer than the one before, so the glow has the continuous falloff of a deep bloom instead of
// the fixed set of blobs that scattered point sampling gives.
//
// `passes` is how many levels are added: more levels means a finer, smoother gradient between
// Radius and Deep Radius. `deepmix` weights the softer levels down so the tail stays gentle.
//
// `method` picks the kernel every level uses:
//   0 Gaussian   - a rotationally symmetric Gaussian, the softest and most even falloff
//   1 Bokeh      - the same rings weighted flat, so the glow reads as round bokeh discs
//   2 Anamorphic - the same kernel stretched horizontally, for a wide lens-flare smear
//
// Every sample pattern is also rotated by a per-pixel, per-level angle from gl_FragCoord. A ring
// kernel with a fixed set of directions leaves visible spokes (the "streaks"), because neighbouring
// pixels all sample the same few radial lines; the jitter turns that structure into noise.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float threshold;
uniform float radius;
uniform float deepradius;
uniform float deepmix;
uniform float passes;
uniform float exposure;
uniform float gamma;
uniform float saturation;
uniform vec3 tint;
uniform int method;

const int RINGS = 5;
const int SAMPLES_PER_RING = 16;
const int MAX_PASSES = 64;

const float TWO_PI = 6.28318531;

// Irrational rotations. Rings and levels started at multiples of a full ring step would put every
// tap on the same few radial directions, which smears the glow into hard streaks like a lens
// flare; rotating each ring by the golden angle turns the taps into a spiral instead.
const float GOLDEN_ANGLE = 2.39996323;

float luma(vec3 c) {
	return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// the part of a pixel bright enough to glow; the knee keeps the transition soft so the glow
// doesn't switch on along a hard edge
vec3 bright_pass(vec2 uv) {
	vec3 color = texture2D(tex, clamp(uv, 0.0, 1.0)).rgb;
	float l = luma(color);
	float mask = smoothstep(threshold, min(threshold + 0.25, 1.0), l);
	return color * mask;
}

vec3 blur_level(vec2 uv, float sigma, float salt) {
	vec3 sum = bright_pass(uv);
	float weight_sum = 1.0;

	// per-pixel (and per-level) rotation: neighbouring pixels no longer share a sample pattern, so
	// the rings cannot line up into spokes
	float jitter = hash(gl_FragCoord.xy + vec2(salt * 37.0, salt * 17.0)) * TWO_PI;

	// a Gaussian is rotationally symmetric, so sampling it on concentric rings needs far fewer taps
	// than a square kernel would, as long as the rings don't line up with each other
	for (int ring=0;ring<RINGS;ring++) {
		float ring_fraction = (float(ring) + 1.0) / float(RINGS);
		float ring_pixels = ring_fraction * sigma * 1.6;
		float weight = exp(-0.5 * ring_fraction * ring_fraction * 2.56);

		// a bokeh disc is evenly lit: flat weights up to the rim, which makes the discs read as
		// round highlights instead of a soft haze
		if (method == 1) {
			weight = 1.0;
		}

		float ring_rotation = jitter + (float(ring) * GOLDEN_ANGLE);

		for (int sample_index=0;sample_index<SAMPLES_PER_RING;sample_index++) {
			// a little radial stagger within the ring stops the samples from forming a visible
			// dotted circumference at large radii
			float stagger = (float(sample_index) / float(SAMPLES_PER_RING)) - 0.5;
			float angle = ring_rotation + (float(sample_index) * (TWO_PI / float(SAMPLES_PER_RING)));
			float distance_pixels = ring_pixels * (1.0 + (stagger * 0.12));

			vec2 offset = vec2(cos(angle), sin(angle)) * distance_pixels / resolution;

			// anamorphic: the same kernel stretched sideways
			if (method == 2) {
				offset.x *= 4.0;
			}

			sum += bright_pass(uv + offset) * weight;
			weight_sum += weight;
		}
	}

	return sum / weight_sum;
}

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	float levels = max(passes, 1.0);
	float base_radius = max(radius, 0.5);
	float top_radius = max(deepradius, base_radius);

	// geometric steps, so consecutive levels are each a noticeably softer blur than the last
	float growth = pow(top_radius / base_radius, 1.0 / max(levels - 1.0, 1.0));

	vec3 glow = vec3(0.0);
	float weight_sum = 0.0;
	float sigma = base_radius;

	for (int i=0;i<MAX_PASSES;i++) {
		// GLSL 1.10 needs a constant loop bound, so the slider decides where we stop
		if (float(i) >= levels) {
			break;
		}

		float progress = (levels > 1.0) ? (float(i) / (levels - 1.0)) : 0.0;
		float weight = mix(1.0, deepmix, progress);

		glow += blur_level(vTexCoord, sigma, float(i)) * weight;
		weight_sum += weight;

		sigma *= growth;
	}

	glow /= max(weight_sum, 0.0001);
	glow *= exposure;
	glow = pow(max(glow, 0.0), vec3(gamma));

	float glow_luma = luma(glow);
	glow = mix(vec3(glow_luma), glow, saturation);
	glow *= tint;

	// add the glow, lifting the result towards white rather than clipping to a flat colour
	vec3 result = base.rgb + glow * (vec3(1.0) - base.rgb * 0.5);

	gl_FragColor = vec4(result, base.a);
}
