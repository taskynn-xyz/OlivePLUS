#version 110

// Turbulent Warp: a domain-warped distortion. Two noise fields bend the layer in different
// directions and a radial twist pulls it around the centre, which smears and melts shapes instead
// of sliding them around like a plain offset. Animate Speed for a boiling, liquid look.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec2 resolution;
uniform float time;

uniform float strength;
uniform float scale;
uniform float twist;
uniform float speed;
uniform int edgeClamp;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float value_noise(vec2 p) {
	vec2 cell = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);

	float a = hash(cell);
	float b = hash(cell + vec2(1.0, 0.0));
	float c = hash(cell + vec2(0.0, 1.0));
	float d = hash(cell + vec2(1.0, 1.0));

	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fractal_noise(vec2 p) {
	float value = 0.0;
	float amplitude = 0.5;

	for (int i = 0; i < 4; i++) {
		value += amplitude * value_noise(p);
		p *= 2.11;
		amplitude *= 0.5;
	}

	return value;
}

void main(void) {
	vec2 uv = vTexCoord;
	vec2 flow = vec2(time * speed, time * speed * 0.6);

	vec2 warp_position = (uv * max(scale, 0.01)) + flow;

	// two decorrelated noise fields, so the warp bends in both directions
	float warp_x = fractal_noise(warp_position);
	float warp_y = fractal_noise(warp_position + vec2(3.1, 7.7));

	vec2 warp = (vec2(warp_x, warp_y) - vec2(0.5)) * 2.0;

	vec2 centred = uv - vec2(0.5);
	vec2 twist_offset = centred * (twist * length(centred) * 4.0);

	vec2 source = uv + ((warp * 2.0) + twist_offset) * (max(strength, 0.0) / max(resolution, vec2(1.0, 1.0)));

	if (edgeClamp != 0) {
		source = clamp(source, 0.0, 1.0);
	}

	gl_FragColor = texture2D(tex, source);
}
