#version 110

// God Rays: light shafts radiating from a source point.
//
// Every pixel marches towards the light source, accumulating the bright parts of the image it
// passes and attenuating them with `decay`, which is the classic radial-blur shafts look.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float sourceX;
uniform float sourceY;
uniform float threshold;
uniform float length;
uniform float decay;
uniform float intensity;
uniform vec3 raycolor;
uniform float screen;

const int SAMPLES = 48;

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	vec2 source = vec2(sourceX, sourceY);
	vec2 delta = (vTexCoord - source) * (length / float(SAMPLES));
	vec2 coord = vTexCoord;

	vec3 accumulation = vec3(0.0);
	float weight = 1.0;
	float total = 0.0;

	for (int i=0;i<SAMPLES;i++) {
		coord -= delta;

		vec3 sample_color = texture2D(tex, clamp(coord, 0.0, 1.0)).rgb;
		float luma = dot(sample_color, vec3(0.2126, 0.7152, 0.0722));
		float bright = max(luma - threshold, 0.0) / max(1.0 - threshold, 0.001);

		accumulation += sample_color * bright * weight;
		total += weight;
		weight *= decay;
	}

	accumulation = (accumulation / max(total, 0.001)) * raycolor * intensity;

	// `screen` 0 adds the shafts, 1 screens them over the image
	vec3 screened = vec3(1.0) - (vec3(1.0) - base.rgb) * (vec3(1.0) - clamp(accumulation, 0.0, 1.0));
	vec3 result = base.rgb + accumulation * (1.0 - screen) + (screened - base.rgb) * screen;

	gl_FragColor = vec4(result, base.a);
}
