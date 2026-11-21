#version 110

// Linear Wipe: directional wipe with a soft edge. Keyframe Progress for a clean reveal.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float progress;
uniform float angle;
uniform float softness;
uniform float reverse;

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	float a = radians(angle);
	vec2 dir = vec2(cos(a), sin(a));

	// normalise the projection so 0-100% always sweeps the whole frame
	float range = 0.5 * abs(dir.x) + 0.5 * abs(dir.y);
	float d = dot(vTexCoord - 0.5, dir) / max(range, 0.0001) * 0.5 + 0.5;

	float p = clamp(progress * 0.01, 0.0, 1.0);
	float soft = max(0.0005, softness * 0.002);

	float mask;
	if (reverse > 0.5) {
		mask = smoothstep(p - soft, p + soft, d);
	} else {
		mask = 1.0 - smoothstep(p - soft, p + soft, d);
	}

	gl_FragColor = vec4(col.rgb, col.a * mask);
}
