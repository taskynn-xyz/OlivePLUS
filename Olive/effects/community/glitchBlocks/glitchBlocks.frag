#version 110

// Glitch Blocks: slices the frame into horizontal blocks and shifts each one sideways by a random
// amount that changes over time, with an RGB split on the displaced blocks.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform float amount;
uniform float blocksize;
uniform float speed;
uniform float time;

float hash(float n) {
	return fract(sin(n) * 43758.5453);
}

void main(void) {
	float size = max(blocksize, 0.001);
	float block = floor(vTexCoord.y / size);

	// new random pattern `speed` times a second
	float tick = floor(time * speed);
	float seed = hash(block * 12.9898 + tick * 78.233);

	float shift = (seed - 0.5) * 2.0 * amount * 0.08;

	// only some blocks actually move, so the frame still reads as the image most of the time
	float active = step(0.55, seed);

	vec2 uv = vTexCoord + vec2(shift * active, 0.0);
	vec4 col = texture2D(tex, uv);

	// split the channels of the displaced blocks
	float split = active * amount * 0.01;
	col.r = texture2D(tex, uv + vec2(split, 0.0)).r;
	col.b = texture2D(tex, uv - vec2(split, 0.0)).b;

	gl_FragColor = col;
}
