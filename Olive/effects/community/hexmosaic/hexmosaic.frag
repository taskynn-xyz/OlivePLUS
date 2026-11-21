#version 110

// Hex Mosaic: the image rebuilt out of flat hexagonal tiles, with dark edges between them.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float cells;
uniform float edgewidth;
uniform float mixamount;
uniform vec3 edgecolor;

// nearest hexagon centre in a pointy-top hex grid expressed in cell units
vec2 hex_center(vec2 p) {
	vec2 size = vec2(1.0, 1.7320508);
	vec4 corners = floor(vec4(p, p - vec2(0.5, 1.0)) / vec4(size, size)) + 0.5;
	vec4 offsets = vec4(p - (corners.xy * size), p - ((corners.zw + 0.5) * size));

	if (dot(offsets.xy, offsets.xy) < dot(offsets.zw, offsets.zw)) {
		return corners.xy * size;
	}

	return (corners.zw + 0.5) * size;
}

// distance to the hexagon boundary, three folded axes 60 degrees apart
float hex_distance(vec2 p) {
	vec2 absolute = abs(p);
	float first = absolute.x;
	float second = dot(absolute, vec2(0.5, 0.8660254));
	float third = dot(absolute, vec2(-0.5, 0.8660254));
	return max(first, max(second, third));
}

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	vec2 position = vTexCoord * cells;
	vec2 center = hex_center(position);
	vec2 local = position - center;

	vec3 tile_color = texture2D(tex, clamp(center / cells, 0.0, 1.0)).rgb;

	float edge = smoothstep(0.5 - max(edgewidth, 0.001), 0.5, hex_distance(local));
	vec3 color = mix(tile_color, edgecolor, edge);

	gl_FragColor = vec4(mix(base.rgb, color, mixamount), base.a);
}
