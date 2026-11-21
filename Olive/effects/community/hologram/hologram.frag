#version 110

// Hologram: cyan projection with chromatic separation, tape tracking bands and
// a flickering scan.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float chroma;
uniform float scanlines;
uniform float bands;
uniform float flicker;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main(void) {
	vec2 uv = vTexCoord;

	// horizontal tracking bands that jump around
	float band = floor(uv.y * 36.0 + time * 2.0);
	float shift = (hash(vec2(band, floor(time * 7.0))) - 0.5) * bands * 0.03;
	uv.x = clamp(uv.x + shift, 0.0, 1.0);

	vec2 o = vec2(chroma / resolution.x, 0.0);

	vec4 col;
	col.r = texture2D(tex, uv + o).r;
	col.g = texture2D(tex, uv).g;
	col.b = texture2D(tex, uv - o).b;
	col.a = texture2D(tex, uv).a;

	// cool projection tint
	col.rgb = mix(col.rgb, col.rgb * vec3(0.55, 1.0, 1.08), 0.6);

	float scan = 0.72 + 0.28 * sin(uv.y * scanlines * 3.14159265 + time * 6.0);
	col.rgb *= scan;

	col.rgb *= 1.0 - flicker * 0.2 * (0.5 + 0.5 * sin(time * 47.0));

	gl_FragColor = col;
}
