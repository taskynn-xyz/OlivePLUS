#version 110

// Volumetric Fog: drifting procedural haze lit by a point light, with the density of the fog
// breaking the light up into soft shafts.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float time;

uniform float density;
uniform float scale;
uniform float speed;
uniform float lightX;
uniform float lightY;
uniform float falloff;
uniform float intensity;
uniform float shafts;
uniform vec3 fogcolor;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float value_noise(vec2 p) {
	vec2 whole = floor(p);
	vec2 part = fract(p);

	part = part * part * (3.0 - (2.0 * part));

	float a = hash(whole);
	float b = hash(whole + vec2(1.0, 0.0));
	float c = hash(whole + vec2(0.0, 1.0));
	float d = hash(whole + vec2(1.0, 1.0));

	return mix(mix(a, b, part.x), mix(c, d, part.x), part.y);
}

float fbm(vec2 p) {
	float value = 0.0;
	float amplitude = 0.5;

	for (int i=0;i<5;i++) {
		value += amplitude * value_noise(p);
		p *= 2.0;
		amplitude *= 0.5;
	}

	return value;
}

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	vec2 drift = vec2(time * speed, time * speed * 0.6);
	vec2 p = (vTexCoord * scale) + drift;

	float fog = fbm(p) * fbm((p * 0.7) + 3.1);
	fog = clamp(fog * density * 3.0, 0.0, 1.0);

	float distance_to_light = distance(vTexCoord, vec2(lightX, lightY));
	float light = exp(-distance_to_light * falloff) * intensity;
	light = light * mix(1.0, fog, shafts);

	// Keep the added fog inside the layer silhouette so it does not paint over tracks below.
	gl_FragColor = vec4(base.rgb + (fogcolor * light * fog * base.a), base.a);
}
