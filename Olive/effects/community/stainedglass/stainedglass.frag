#version 110

// Stained Glass: Voronoi cells of flat colour with leaded grout lines between them.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float cells;
uniform float grout;
uniform vec3 groutcolor;
uniform float flatness;

vec2 hash2(vec2 p) {
	return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	vec2 position = vTexCoord * cells;
	vec2 cell = floor(position);
	vec2 local = fract(position);

	float nearest = 8.0;
	float second = 8.0;
	vec2 nearest_offset = vec2(0.0);

	for (int y=-1;y<=1;y++) {
		for (int x=-1;x<=1;x++) {
			vec2 neighbour = vec2(float(x), float(y));
			vec2 point = neighbour + hash2(cell + neighbour) - local;
			float distance_to_point = length(point);

			if (distance_to_point < nearest) {
				second = nearest;
				nearest = distance_to_point;
				nearest_offset = point;
			} else if (distance_to_point < second) {
				second = distance_to_point;
			}
		}
	}

	// the cell edge is where the two closest seeds are equally far away
	float border = smoothstep(0.0, max(grout, 0.001), second - nearest);

	// one flat colour per shard, taken from the middle of that shard
	vec2 shard = (cell + ((hash2(cell) * 0.5) + local - nearest_offset)) / cells;
	vec3 flat_color = texture2D(tex, clamp(shard, 0.0, 1.0)).rgb;

	vec3 color = mix(base.rgb, flat_color, flatness);
	color = mix(groutcolor, color, border);

	gl_FragColor = vec4(color, base.a);
}
