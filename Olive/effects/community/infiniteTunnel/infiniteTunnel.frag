#version 110

// Infinite Tunnel: log-polar tunnel pattern with animated depth, twist and a two-colour tint.
// Multiplied over the clip so alpha (and therefore the layer silhouette) is preserved.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec2 resolution;
uniform float time;

uniform float speed;
uniform float twist;
uniform float rings;
uniform float glow;
uniform vec3 tinta;
uniform vec3 tintb;

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	vec2 p = vTexCoord - 0.5;
	p.x *= resolution.x / max(1.0, resolution.y);

	float r = length(p);
	float a = atan(p.y, p.x);

	// 1/r makes evenly spaced rings fall away toward the centre: the tunnel.
	float depth = 1.0 / max(r, 0.0001);
	float ring = sin(depth * rings + a * twist + time * speed * 2.0);
	float band = smoothstep(0.0, 0.35, abs(ring));

	vec3 tint = mix(tinta, tintb, 0.5 + 0.5 * sin(a * 2.0 + time * speed));
	float edge = smoothstep(0.0, 0.9, r * 2.0);

	vec3 rgb = col.rgb * (0.45 + 0.55 * band) + tint * glow * (1.0 - band) * edge;

	gl_FragColor = vec4(rgb * col.a, col.a);
}
