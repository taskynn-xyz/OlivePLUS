#version 110

// Crushes color depth with a 4x4 ordered (bayer) dither - retro bitmap look

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;

uniform float levels;
uniform float dither;
uniform float pixel_size;

float bayer2(vec2 a) {
	a = floor(a);
	return fract(a.x / 2.0 + a.y * a.y * 0.75);
}

float bayer4(vec2 a) {
	return bayer2(0.5 * a) * 0.25 + bayer2(a);
}

void main(void) {
	vec2 px = pixel_size / resolution;
	vec2 uv = floor(vTexCoord / px) * px + px * 0.5;

	vec4 col = texture2D(tex, uv);

	float d = (bayer4(gl_FragCoord.xy) - 0.5) * (dither / levels);
	vec3 crushed = floor(col.rgb * levels + d + 0.5) / levels;

	gl_FragColor = vec4(clamp(crushed, 0.0, 1.0), col.a);
}
