#version 110

// Grid Collage: repeats the clip as a grid of zoomed panels with gapped borders.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform float columns;
uniform float rows;
uniform float gap;
uniform float zoom;

void main(void) {
	vec2 grid = vec2(max(columns, 1.0), max(rows, 1.0));

	vec2 cell = floor(vTexCoord * grid);
	vec2 local = fract(vTexCoord * grid);

	// gapped border between panels
	vec2 border = vec2(gap * 0.5);
	if (local.x < border.x || local.x > 1.0 - border.x
			|| local.y < border.y || local.y > 1.0 - border.y) {
		gl_FragColor = vec4(0.0);
		return;
	}

	vec2 centred = ((local - vec2(0.5)) / max(zoom, 0.001)) + vec2(0.5);
	vec2 uv = (cell + centred) / grid;

	gl_FragColor = texture2D(tex, uv);
}
