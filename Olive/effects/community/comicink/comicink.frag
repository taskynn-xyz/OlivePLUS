#version 110

// Comic Ink: posterised colours with black ink outlines, the classic cel-shaded comic look.

uniform sampler2D tex;
uniform vec2 resolution;
varying vec2 vTexCoord;

uniform float levels;
uniform float ink;
uniform float linewidth;
uniform float saturation;

float luma_at(vec2 uv) {
	vec3 color = texture2D(tex, clamp(uv, 0.0, 1.0)).rgb;
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main(void) {
	vec4 base = texture2D(tex, vTexCoord);

	vec2 offset = vec2(linewidth) / resolution;

	float gradient_x = (luma_at(vTexCoord + vec2(offset.x, 0.0)) - luma_at(vTexCoord - vec2(offset.x, 0.0)));
	float gradient_y = (luma_at(vTexCoord + vec2(0.0, offset.y)) - luma_at(vTexCoord - vec2(0.0, offset.y)));
	float edge = clamp(sqrt((gradient_x * gradient_x) + (gradient_y * gradient_y)) * ink, 0.0, 1.0);

	float luma = dot(base.rgb, vec3(0.2126, 0.7152, 0.0722));
	vec3 saturated = mix(vec3(luma), base.rgb, saturation);

	// hard steps per channel produce the flat comic fills
	vec3 posterized = floor(clamp(saturated, 0.0, 1.0) * levels + 0.5) / levels;

	gl_FragColor = vec4(posterized * (1.0 - edge), base.a);
}
