#version 110

// ASCII Mosaic: reduces the image to a grid of cells and paints each cell as a little dot-matrix
// glyph whose lit dots follow the cell's luminance. A deterministic hash picks the glyph pattern,
// so neighbouring cells with the same brightness look like different characters.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec2 resolution;

uniform float cellsize;
uniform float contrast;
uniform vec3 ink;
uniform vec3 paper;
uniform bool colorize;

float luma(vec3 c) {
	return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main(void) {
	vec2 cell = vec2(max(2.0, cellsize));
	vec2 grid = max(vec2(1.0), resolution / cell);

	vec2 g = floor(vTexCoord * grid);
	vec2 local = fract(vTexCoord * grid);

	// sample the centre of the cell
	vec4 src = texture2D(tex, (g + 0.5) / grid);
	float l = clamp((luma(src.rgb) - 0.5) * (1.0 + contrast) + 0.5, 0.0, 1.0);

	// 3x5 dot matrix, lit dots selected by a hash of the cell and the brightness level
	vec2 dotpos = floor(local * vec2(3.0, 5.0));
	float idx = dotpos.y * 3.0 + dotpos.x;
	float level = floor(l * 6.0);
	float pattern = fract(sin(idx * 12.9898 + level * 78.233 + g.x * 37.719 + g.y * 11.135) * 43758.5453);
	float on = step(pattern, l);

	vec3 rgb = colorize ? mix(paper, ink, on) : vec3(on * l);

	float alpha = src.a;
	gl_FragColor = vec4(rgb * alpha, alpha);
}
