#version 110

// Bubble Wrap: a wobbling field of glass bubbles. Each cell finds its nearest jittered centre,
// refracts the image inside the bubble and adds a specular rim highlight.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform vec2 resolution;
uniform float time;

uniform float bubblesize;
uniform float refraction;
uniform float highlight;
uniform float speed;

vec2 hash2(vec2 p) {
	p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
	return fract(sin(p) * 43758.5453);
}

void main(void) {
	vec4 col = texture2D(tex, vTexCoord);

	vec2 px = vTexCoord * resolution;
	float cell = max(4.0, bubblesize);
	vec2 g = px / cell;
	vec2 base = floor(g);
	vec2 frac = fract(g) - 0.5;

	float best = 1.0e9;
	vec2 bestc = vec2(0.0);

	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			vec2 o = vec2(float(x), float(y));
			vec2 c = o + hash2(base + o) * 0.8 + 0.1;
			vec2 d = frac - c + 0.5;
			float dist = length(d);

			if (dist < best) {
				best = dist;
				bestc = d;
			}
		}
	}

	float r = best * 2.0;
	float bubble = smoothstep(0.5, 0.0, r);
	float wobble = 0.5 + 0.5 * sin(time * speed * 2.0 + hash2(base).x * 6.2832);

	vec2 offset = bestc * refraction * 0.05 * (0.5 + wobble);
	vec4 refracted = texture2D(tex, clamp(vTexCoord + offset, 0.0, 1.0));

	vec3 rgb = mix(col.rgb, refracted.rgb * (1.0 + bubble * 0.6), refraction);
	rgb += vec3(highlight) * bubble * pow(max(0.0, 1.0 - r), 3.0) * 0.5;

	float alpha = col.a;
	gl_FragColor = vec4(rgb * alpha, alpha);
}
