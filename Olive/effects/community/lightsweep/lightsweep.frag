#version 110

// Light Sweep: a soft band of light travelling across the frame. Keyframe Position to sweep it,
// which is the standard motion-graphics shine over a logo or title.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float position;
uniform float width;
uniform float angle;
uniform float hardness;
uniform float intensity;
uniform vec3 sweepcolor;

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	float angle_radians = radians(angle);
	vec2 direction = vec2(cos(angle_radians), sin(angle_radians));

	// distance from the centre of the band, measured along the sweep direction
	float along = dot(vTexCoord - vec2(0.5), direction) + 0.5;
	float band = 1.0 - smoothstep(0.0, max(width, 0.001), abs(along - position));

	band = pow(clamp(band, 0.0, 1.0), mix(8.0, 1.0, hardness));

	gl_FragColor = vec4(base.rgb + (sweepcolor * band * intensity * base.a), base.a);
}
