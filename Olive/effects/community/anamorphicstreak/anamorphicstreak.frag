#version 110

// Anamorphic Streak: horizontal flares pulled out of the bright areas of the frame, like the
// horizontal flare of anamorphic lenses.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float threshold;
uniform float length;
uniform float intensity;
uniform float falloff;
uniform vec3 streakcolor;

const int TAPS = 32;

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	vec3 accumulation = vec3(0.0);

	for (int i=1;i<=TAPS;i++) {
		float progress = float(i) / float(TAPS);
		float weight = pow(1.0 - progress, falloff);
		float offset = progress * length;

		vec3 left = texture2D(tex, clamp(vTexCoord - vec2(offset, 0.0), 0.0, 1.0)).rgb;
		vec3 right = texture2D(tex, clamp(vTexCoord + vec2(offset, 0.0), 0.0, 1.0)).rgb;

		float left_luma = dot(left, vec3(0.2126, 0.7152, 0.0722));
		float right_luma = dot(right, vec3(0.2126, 0.7152, 0.0722));

		accumulation += ((left * max(left_luma - threshold, 0.0))
						 + (right * max(right_luma - threshold, 0.0))) * weight;
	}

	accumulation = (accumulation / float(TAPS)) * streakcolor * intensity;

	gl_FragColor = vec4(base.rgb + accumulation, base.a);
}
