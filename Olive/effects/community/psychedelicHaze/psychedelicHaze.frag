#version 110

// Psychedelic Haze: two drifting value-noise fields warp the image while a cycling hue tints it.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec2 resolution;
uniform float time;

uniform float amount;
uniform float scale;
uniform float speed;
uniform float hue;
uniform float saturation;

vec3 hsv2rgb(vec3 c) {
	vec3 p = abs(fract(c.xxx + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
	return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}

float valueNoise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);

	float a = fract(sin(dot(i, vec2(127.1, 311.7))) * 43758.5453);
	float b = fract(sin(dot(i + vec2(1.0, 0.0), vec2(127.1, 311.7))) * 43758.5453);
	float c = fract(sin(dot(i + vec2(0.0, 1.0), vec2(127.1, 311.7))) * 43758.5453);
	float d = fract(sin(dot(i + vec2(1.0, 1.0), vec2(127.1, 311.7))) * 43758.5453);

	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	vec2 p = vTexCoord * scale;
	float t = time * speed;

	float n1 = valueNoise(p + vec2(t, -t * 0.7));
	float n2 = valueNoise(p * 1.7 + vec2(-t * 0.5, t * 0.9));

	vec2 warp = (vec2(n1, n2) - 0.5) * amount * 0.2;
	vec4 warped = texture2D(tex, clamp(vTexCoord + warp, 0.0, 1.0));

	float h = fract(hue / 360.0 + (n1 + n2) * 0.25 + time * 0.02);
	vec3 tint = hsv2rgb(vec3(h, clamp(saturation, 0.0, 1.0), 1.0));

	vec3 rgb = mix(warped.rgb, warped.rgb * tint * 2.0, amount);

	gl_FragColor = vec4(rgb * warped.a, warped.a);
}
