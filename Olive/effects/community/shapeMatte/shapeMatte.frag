#version 110

// Shape Matte: alpha matte in the shape of a circle, rounded box or diamond, for building
// motion-graphics cards, avatars or icon containers.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float shape;      // 0 = circle, 1 = box, 2 = diamond
uniform float width_s;    // % of frame width
uniform float height_s;   // % of frame height
uniform float roundness;  // box corner radius, 0..1
uniform float feather;
uniform float invert;
uniform float center_x;
uniform float center_y;

float sd_box(vec2 p, vec2 b, float r) {
	vec2 d = abs(p) - b + r;
	return min(max(d.x, d.y), 0.0) + length(max(d, vec2(0.0))) - r;
}

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	float ar = resolution.x / resolution.y;
	vec2 p = vTexCoord - vec2(0.5 + center_x, 0.5 + center_y);
	p.x *= ar;

	float hw = width_s * 0.005 * ar;
	float hh = height_s * 0.005;
	float radius = 0.5 * min(hw, hh);

	float dist;
	if (shape < 0.5) {
		dist = length(p) - radius;
	} else if (shape < 1.5) {
		dist = sd_box(p, vec2(hw, hh), roundness * min(hw, hh));
	} else {
		vec2 q = abs(p) / max(vec2(hw, hh), vec2(0.0001));
		dist = (q.x + q.y - 1.0) * min(hw, hh);
	}

	float f = max(0.0005, feather * 0.002);
	float mask = 1.0 - smoothstep(-f, f, dist);

	if (invert > 0.5) {
		mask = 1.0 - mask;
	}

	gl_FragColor = vec4(col.rgb, col.a * mask);
}
