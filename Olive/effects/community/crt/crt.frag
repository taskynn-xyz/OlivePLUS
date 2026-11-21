#version 110

// CRT / VHS monitor simulation: curved glass, chroma bleed, scanlines, rolling bar, static

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float lines;
uniform float chroma;
uniform float noise_amount;
uniform float vignette_amount;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main(void) {
	vec2 c = vTexCoord - 0.5;

	// barrel curvature of the tube
	vec2 uv = c * (1.0 + 0.07 * dot(c, c)) + 0.5;

	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
		gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	// channel separation
	vec2 offset = vec2(chroma, 0.0) / resolution;
	vec4 col;
	col.r = texture2D(tex, uv + offset).r;
	col.g = texture2D(tex, uv).g;
	col.b = texture2D(tex, uv - offset).b;
	col.a = texture2D(tex, uv).a;

	// scanlines drifting downwards
	float scan = 0.5 + 0.5 * sin((uv.y * lines - time * 60.0) * 3.14159265);
	col.rgb *= mix(1.0, scan, 0.55);

	// rolling bright band from a badly synced tape
	float band = smoothstep(0.05, 0.0, abs(fract(uv.y * 0.5 - time * 0.12) - 0.5));
	col.rgb += band * 0.07;

	// static
	col.rgb += (hash(uv * resolution + time * 137.0) - 0.5) * noise_amount;

	// vignette
	float vig = smoothstep(0.9, 0.2, length(c) * 1.45);
	col.rgb *= mix(1.0, vig, vignette_amount);

	gl_FragColor = vec4(clamp(col.rgb, 0.0, 1.0), col.a);
}
