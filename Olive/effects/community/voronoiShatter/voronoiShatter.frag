#version 110

// Voronoi Shatter: the frame is broken into animated cells and each shard is
// displaced towards its own centre, with dark cracks between the pieces.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float cells;
uniform float amount;
uniform float cracks;
uniform float animate;

vec2 hash2(vec2 p) {
	return fract(sin(vec2(dot(p, vec2(127.1, 311.7)),
	                      dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}

void main(void) {
	vec2 uv = vTexCoord;
	float ar = resolution.x / resolution.y;
	vec2 p = vec2(uv.x * ar, uv.y) * cells;
	vec2 ip = floor(p);
	vec2 fp = fract(p);

	float t = time * animate;

	float best = 8.0;
	vec2 best_r = vec2(0.0);

	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 g = vec2(float(x), float(y));
			vec2 o = 0.5 + 0.5 * sin(t + 6.2831853 * hash2(ip + g));
			vec2 r = g + o - fp;
			float d = dot(r, r);
			if (d < best) {
				best = d;
				best_r = r;
			}
		}
	}

	// push the shard inwards, in frame units
	vec2 disp = best_r * (amount / cells) * 1.6;
	vec2 suv = clamp(uv + vec2(disp.x / ar, disp.y), vec2(0.0), vec2(1.0));

	vec4 col = texture2D(tex, suv);

	// cracks: darken the borders between cells
	float edge = smoothstep(0.0, 0.09, length(best_r));
	col.rgb *= mix(1.0, edge, cracks);

	// per-shard brightness so the pieces read separately
	col.rgb *= 0.9 + 0.1 * sin(dot(ip, vec2(12.9, 78.2)) + t);

	gl_FragColor = col;
}
