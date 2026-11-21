#version 110

// Zoom Blur: samples the frame along the line towards a centre point, which gives the classic
// motion-graphics "whip/zoom" smear without needing any motion analysis.

uniform sampler2D tex;
varying vec2 vTexCoord;

uniform float amount;
uniform float centerx;
uniform float centery;

const int SAMPLES = 24;

void main(void) {
	vec2 centre = vec2(centerx, centery);
	vec2 dir = vTexCoord - centre;

	vec4 sum = vec4(0.0);
	float total = 0.0;

	for (int i = 0; i < SAMPLES; i++) {
		float t = float(i) / float(SAMPLES - 1);

		// weight the samples near the original position more heavily so the image stays readable
		float w = 1.0 - (t * 0.65);

		sum += texture2D(tex, vTexCoord - (dir * t * amount)) * w;
		total += w;
	}

	gl_FragColor = sum / max(total, 0.0001);
}
