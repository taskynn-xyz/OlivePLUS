#version 110

// Melts the image with scrolling value noise, like paint running down the frame

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float amount;
uniform float scale;
uniform float speed;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float vnoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	float a = hash(i);
	float b = hash(i + vec2(1.0, 0.0));
	float c = hash(i + vec2(0.0, 1.0));
	float d = hash(i + vec2(1.0, 1.0));
	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main(void) {
	vec2 t = vec2(time * speed);
	float nx = vnoise(vTexCoord * scale + t);
	float ny = vnoise(vTexCoord * scale + t + vec2(31.7, 11.3));
	vec2 offset = (vec2(nx, ny) - 0.5) * 2.0 * amount / resolution;
	gl_FragColor = texture2D(tex, vTexCoord + offset);
}
