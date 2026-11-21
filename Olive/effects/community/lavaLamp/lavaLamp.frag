#version 110

// Lava Lamp: drifting metaball blobs warp the image and tint it, the classic 90s motion backdrop.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec2 resolution;
uniform float time;

uniform float amount;
uniform float blobs;
uniform float speed;
uniform float mixamount;
uniform vec3 colora;
uniform vec3 colorb;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float blobField(vec2 p, float t) {
	float f = 0.0;

	for (int i = 0; i < 5; i++) {
		float fi = float(i);
		vec2 c = vec2(0.5 + 0.42 * sin(t * 0.7 + fi * 2.1),
		              0.5 + 0.42 * cos(t * 0.53 + fi * 1.7));
		float d = length(p - c);
		f += blobs / (d * d * 40.0 + 1.0);
	}

	return f;
}

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	vec2 p = vTexCoord;
	p.x *= resolution.x / max(1.0, resolution.y);

	float t = time * speed;
	float f = blobField(p, t);

	vec2 warp = vec2(sin(f * 3.0 + t), cos(f * 2.0 - t)) * amount * 0.06;
	vec4 warped = texture2D(tex, clamp(vTexCoord + warp, 0.001, 0.999));

	vec3 tint = mix(colora, colorb, clamp(f * 0.25, 0.0, 1.0));
	vec3 rgb = mix(warped.rgb, tint * f * 0.35 + warped.rgb, clamp(mixamount, 0.0, 1.0));

	gl_FragColor = vec4(rgb * warped.a, warped.a);
}
