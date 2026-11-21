#version 110

// Pencil Sketch: Sobel edges turned into graphite lines on grainy paper. `strength` mixes the
// sketch with the original, `thickness` is the edge sample distance in pixels.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float strength;
uniform float thickness;
uniform float paper;
uniform vec3 papercolor;

float luma_at(vec2 uv) {
	vec3 color = texture2D(tex, clamp(uv, 0.0, 1.0)).rgb;
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	vec2 offset = vec2(thickness) / resolution;

	float top_left = luma_at(vTexCoord + vec2(-offset.x, -offset.y));
	float top = luma_at(vTexCoord + vec2(0.0, -offset.y));
	float top_right = luma_at(vTexCoord + vec2(offset.x, -offset.y));
	float left = luma_at(vTexCoord + vec2(-offset.x, 0.0));
	float right = luma_at(vTexCoord + vec2(offset.x, 0.0));
	float bottom_left = luma_at(vTexCoord + vec2(-offset.x, offset.y));
	float bottom = luma_at(vTexCoord + vec2(0.0, offset.y));
	float bottom_right = luma_at(vTexCoord + vec2(offset.x, offset.y));

	float gradient_x = (top_right + (2.0 * right) + bottom_right)
			- (top_left + (2.0 * left) + bottom_left);
	float gradient_y = (bottom_left + (2.0 * bottom) + bottom_right)
			- (top_left + (2.0 * top) + top_right);

	float edge = clamp(sqrt((gradient_x * gradient_x) + (gradient_y * gradient_y)) * strength, 0.0, 1.0);

	// pencil lines are graphite on paper, so the edge darkens the paper colour
	vec3 line_color = mix(papercolor, vec3(0.12, 0.11, 0.10), edge);

	float grain = hash(floor(vTexCoord * resolution * 0.5));
	line_color *= 1.0 - (paper * grain * 0.25);

	gl_FragColor = vec4(mix(base.rgb, line_color, clamp(strength + 0.2, 0.0, 1.0)), base.a);
}
