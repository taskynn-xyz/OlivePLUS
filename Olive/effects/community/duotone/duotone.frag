#version 110

// Duotone: maps luminance onto two colours, the flat two-colour motion-graphics look.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec3 shadow;
uniform vec3 highlight;
uniform float amount;
uniform float contrast;

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	float l = dot(col.rgb, vec3(0.2126, 0.7152, 0.0722));
	l = clamp((l - 0.5) * (1.0 + contrast) + 0.5, 0.0, 1.0);

	vec3 duo = mix(shadow, highlight, l);

	gl_FragColor = vec4(mix(col.rgb, duo, clamp(amount, 0.0, 1.0)), col.a);
}
