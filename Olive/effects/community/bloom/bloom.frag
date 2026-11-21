#version 110

/***
    Port of https://www.shadertoy.com/view/Ms2Xz3
    Adaption and Implementation by CGVIRUS for Olive-Editor Community

    `method` chooses the kernel used for the highlight pass:
      0 Gaussian - many jittered taps with a Gaussian falloff, the smoothest option
      1 Box      - the original 3x3 mip-blurred box
      2 Bokeh    - a flat disc of taps, for round bokeh highlights
***/

uniform sampler2D tex;
varying vec2 uTexCoord;
uniform vec2 resolution;

const vec2 renderScale = vec2(1.,1.);
uniform float Threshold;
uniform float Intensity;
uniform float BlurSize;
uniform int method;

const float TWO_PI = 6.28318531;
const float GOLDEN_ANGLE = 2.39996323;
const int TAPS = 20;
const int RINGS = 4;

float hash(vec2 p)
{
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 BlurColor (in vec2 Coord, in sampler2D Tex, in float MipBias)
{
	vec2 TexelSize = MipBias/resolution.xy;

    vec4  Color = texture2D(Tex, Coord, MipBias);
    Color += texture2D(Tex, Coord + vec2(TexelSize.x,0.0), MipBias);
    Color += texture2D(Tex, Coord + vec2(-TexelSize.x,0.0), MipBias);
    Color += texture2D(Tex, Coord + vec2(0.0,TexelSize.y), MipBias);
    Color += texture2D(Tex, Coord + vec2(0.0,-TexelSize.y), MipBias);
    Color += texture2D(Tex, Coord + vec2(TexelSize.x,TexelSize.y), MipBias);
    Color += texture2D(Tex, Coord + vec2(-TexelSize.x,TexelSize.y), MipBias);
    Color += texture2D(Tex, Coord + vec2(TexelSize.x,-TexelSize.y), MipBias);
    Color += texture2D(Tex, Coord + vec2(-TexelSize.x,-TexelSize.y), MipBias);

    return Color/9.0;
}

// Rotational kernel: flat weights make it a bokeh disc, a Gaussian falloff makes it a soft blur.
// The per-pixel rotation keeps the rings from lining up into spokes between neighbouring pixels.
vec4 BlurRing (in vec2 Coord, in float RadiusPixels, in int Flat)
{
	float jitter = hash(gl_FragCoord.xy) * TWO_PI;

	vec4 sum = vec4(0.0);
	float weight_sum = 0.0;

	for (int ring=1;ring<=RINGS;ring++) {
		float ring_fraction = float(ring) / float(RINGS);
		float weight = (Flat == 1) ? 1.0 : exp(-0.5 * ring_fraction * ring_fraction * 4.0);
		float ring_rotation = jitter + (float(ring) * GOLDEN_ANGLE);

		for (int i=0;i<TAPS;i++) {
			float angle = ring_rotation + (float(i) * (TWO_PI / float(TAPS)));
			vec2 offset = vec2(cos(angle), sin(angle)) * (ring_fraction * RadiusPixels) / resolution;

			sum += texture2D(tex, Coord + offset) * weight;
			weight_sum += weight;
		}
	}

	return sum / max(weight_sum, 0.0001);
}

void main(void)
{
	vec2 uv = gl_FragCoord.xy/resolution.xy;

    vec4 Color = texture2D(tex, uv);

    vec4 Blurred;
    if (method == 1) {
        Blurred = BlurColor(uv, tex, log2(max(BlurSize*renderScale.x, 1.0)));
    } else if (method == 2) {
        Blurred = BlurRing(uv, max(BlurSize, 1.0), 1);
    } else {
        Blurred = BlurRing(uv, max(BlurSize, 1.0), 0);
    }

    vec4 Highlight = clamp(Blurred-(Threshold*0.1),0.0,1.0)*1.0/(1.0-(Threshold*0.1));

    gl_FragColor = 1.0-(1.0-Color)*(1.0-Highlight*(Intensity*0.1)); //Screen Blend Mode
}
