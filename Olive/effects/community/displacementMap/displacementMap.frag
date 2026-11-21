#version 110

// Displacement Map: pushes every pixel of the layer around with fractal noise, the classic
// heat-haze / liquid-glass warp.
//
// Strength is in pixels, Scale is the noise frequency, Angle rotates the displacement and Speed
// animates it, which makes it a usable motion-graphics warp as well as a still one.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec2 resolution;
uniform float time;

uniform float strength;
uniform float scale;
uniform float angle;
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

	for (int i = 0; i < 5; i++) {
		value += amplitude * value_noise(p);
		p *= 2.03;
		amplitude *= 0.5;
	}

	return value;
}

void main(void) {
	vec2 uv = vTexCoord;
	vec2 flow = vec2(time * speed);

	vec2 noise_position = (uv * max(scale, 0.01)) + flow;

	float n1 = fractal_noise(noise_position);
	float n2 = fractal_noise(noise_position + vec2(5.2, 1.3));

	vec2 offset = (vec2(n1, n2) - vec2(0.5)) * 2.0 * (max(strength, 0.0) / max(resolution, vec2(1.0, 1.0)));

	const float to_radians = 0.0174532925;
	float radians_value = angle * to_radians;
	float c = cos(radians_value);
	float s = sin(radians_value);
	vec2 rotated = vec2((offset.x * c) - (offset.y * s), (offset.x * s) + (offset.y * c));

	vec2 source = uv + rotated;
	if (edgeClamp != 0) {
		source = clamp(source, 0.0, 1.0);
	}

	gl_FragColor = texture2D(tex, source);
}
