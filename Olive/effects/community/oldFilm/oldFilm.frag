#version 110

// Old 16mm film: gate weave, sepia tone, grain, scratches and exposure flicker

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float grain;
uniform float flicker;
uniform float scratches;
uniform float sepia;
uniform float jitter;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main(void) {
	// quantize time to 24fps so the grain flickers like film, not like video
	float t = floor(time * 24.0);

	vec2 uv = vTexCoord;
	uv += (vec2(hash(vec2(t, 1.0)), hash(vec2(t, 7.0))) - 0.5) * jitter * 0.004;

	vec4 src = texture2D(tex, uv);
	vec3 c = src.rgb;

	vec3 sep = vec3(dot(c, vec3(0.393, 0.769, 0.189)),
	                dot(c, vec3(0.349, 0.686, 0.168)),
	                dot(c, vec3(0.272, 0.534, 0.131)));
	c = mix(c, sep, sepia);

	c += (hash(gl_FragCoord.xy + vec2(t, t * 0.7)) - 0.5) * grain * 0.3;

	// vertical scratches that come and go
	float s = hash(vec2(floor(uv.x * resolution.x * 0.2), t));
	c += step(0.9975, s) * scratches * 0.5;

	// exposure flicker
	c *= 1.0 - flicker * 0.22 * hash(vec2(t, 3.0));

	gl_FragColor = vec4(clamp(c, 0.0, 1.0), src.a);
}
