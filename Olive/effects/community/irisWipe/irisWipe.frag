#version 110

// Iris Wipe: circular reveal / close, with a soft edge and an offset centre.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float progress;
uniform float softness;
uniform float center_x;
uniform float center_y;
uniform float invert;

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	vec2 p = vTexCoord - vec2(0.5 + center_x, 0.5 + center_y);
	p.x *= resolution.x / resolution.y;

	float radius = clamp(progress * 0.01, 0.0, 1.0);
	float soft = max(0.001, softness * 0.002);
	float mask = 1.0 - smoothstep(radius - soft, radius + soft, length(p));

	if (invert > 0.5) {
		mask = 1.0 - mask;
	}

	gl_FragColor = vec4(col.rgb, col.a * mask);
}
