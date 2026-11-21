#version 110

// Spotlight: a soft circular pool of light with the rest of the frame darkened, for hero text and
// product reveals.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;

uniform vec3 lightcolor;
uniform float centerx;
uniform float centery;
uniform float radius;
uniform float softness;
uniform float intensity;
uniform float darken;

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	// keep the circle round on non-square footage
	vec2 aspect = vec2(resolution.x / max(resolution.y, 1.0), 1.0);
	float d = length((vTexCoord - vec2(centerx, centery)) * aspect);

	float mask = 1.0 - smoothstep(max(radius, 0.0), max(radius, 0.0) + max(softness, 0.001), d);

	vec3 darkened = col.rgb * (1.0 - clamp(darken, 0.0, 1.0) * (1.0 - mask));
	vec3 lit = darkened + lightcolor * mask * intensity;

	gl_FragColor = vec4(lit, col.a);
}
