#version 110

// Halftone: replaces the image with a rotated dot screen whose dot size follows brightness, the
// print/comic look used in motion-graphics transitions.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;

uniform float cellsize;
uniform float angle;
uniform float strength;
uniform float softness;

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	vec2 pixel = vTexCoord * resolution;

	float a = radians(angle);
	mat2 rotation = mat2(cos(a), -sin(a), sin(a), cos(a));
	vec2 rotated = rotation * pixel;

	float cell = max(cellsize, 2.0);
	vec2 cell_uv = fract(rotated / cell) - vec2(0.5);
	float distance_to_centre = length(cell_uv);

	float luma = dot(col.rgb, vec3(0.2126, 0.7152, 0.0722));
	float dot_radius = (1.0 - luma) * 0.5;

	float edge = max(softness, 0.0) * 0.15;
	float dots = 1.0 - smoothstep(dot_radius - edge, dot_radius + edge, distance_to_centre);

	gl_FragColor = vec4(mix(col.rgb, vec3(dots), clamp(strength, 0.0, 1.0)), col.a);
}
