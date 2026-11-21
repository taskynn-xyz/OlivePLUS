#version 110

// Clip blending modes.
//
// The integer uniform `blendmode` is an index into the BlendMode enum in effects/effect.h, so the
// constants below must stay in the same order. Both textures are premultiplied, so colours are
// unpremultiplied for the blend maths and the result is premultiplied again on output.
//
// The compositor renders this into the sequence buffer with GL blending disabled, because the
// output already contains the background it read.

uniform sampler2D background;
uniform sampler2D foreground;

uniform int blendmode;
uniform float opacity;

varying vec2 vTexCoord;

const int BLEND_NORMAL = 0;
const int BLEND_DARKEN = 1;
const int BLEND_MULTIPLY = 2;
const int BLEND_COLOR_BURN = 3;
const int BLEND_LINEAR_BURN = 4;
const int BLEND_DARKER_COLOR = 5;
const int BLEND_LIGHTEN = 6;
const int BLEND_SCREEN = 7;
const int BLEND_COLOR_DODGE = 8;
const int BLEND_LINEAR_DODGE = 9;
const int BLEND_LIGHTER_COLOR = 10;
const int BLEND_OVERLAY = 11;
const int BLEND_SOFT_LIGHT = 12;
const int BLEND_HARD_LIGHT = 13;
const int BLEND_VIVID_LIGHT = 14;
const int BLEND_LINEAR_LIGHT = 15;
const int BLEND_PIN_LIGHT = 16;
const int BLEND_HARD_MIX = 17;
const int BLEND_DIFFERENCE = 18;
const int BLEND_EXCLUSION = 19;
const int BLEND_SUBTRACT = 20;
const int BLEND_DIVIDE = 21;
const int BLEND_HUE = 22;
const int BLEND_SATURATION = 23;
const int BLEND_COLOR = 24;
const int BLEND_LUMINOSITY = 25;

float lum(vec3 c) {
	return dot(c, vec3(0.3, 0.59, 0.11));
}

float sat(vec3 c) {
	return max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b);
}

vec3 clip_color(vec3 c) {
	float l = lum(c);
	float n = min(min(c.r, c.g), c.b);
	float x = max(max(c.r, c.g), c.b);

	if (n < 0.0) {
		c = l + ((c - l) * l) / (l - n + 0.0001);
	}
	if (x > 1.0) {
		c = l + ((c - l) * (1.0 - l)) / (x - l + 0.0001);
	}

	return c;
}

vec3 set_lum(vec3 c, float l) {
	return clip_color(c + (l - lum(c)));
}

vec3 set_sat(vec3 c, float s) {
	float mn = min(min(c.r, c.g), c.b);
	float mx = max(max(c.r, c.g), c.b);

	if (mx > mn) {
		return (c - mn) * s / (mx - mn);
	}

	return vec3(0.0);
}

float b_darken(float b, float s) {
	return min(b, s);
}

float b_multiply(float b, float s) {
	return b * s;
}

float b_color_burn(float b, float s) {
	return (s <= 0.0) ? 0.0 : (1.0 - min(1.0, (1.0 - b) / s));
}

float b_linear_burn(float b, float s) {
	return max(b + s - 1.0, 0.0);
}

float b_lighten(float b, float s) {
	return max(b, s);
}

float b_screen(float b, float s) {
	return b + s - (b * s);
}

float b_color_dodge(float b, float s) {
	return (s >= 1.0) ? 1.0 : min(1.0, b / (1.0 - s));
}

float b_linear_dodge(float b, float s) {
	return min(b + s, 1.0);
}

float b_overlay(float b, float s) {
	return (b < 0.5) ? (2.0 * b * s) : (1.0 - (2.0 * (1.0 - b) * (1.0 - s)));
}

float b_soft_light(float b, float s) {
	// the W3C compositing formula, which is what Photoshop/After Effects use
	if (s <= 0.5) {
		return b - ((1.0 - 2.0 * s) * b * (1.0 - b));
	}

	float d = (b <= 0.25) ? ((((16.0 * b) - 12.0) * b + 4.0) * b) : sqrt(b);
	return b + ((2.0 * s) - 1.0) * (d - b);
}

float b_hard_light(float b, float s) {
	return b_overlay(s, b);
}

float b_vivid_light(float b, float s) {
	return (s < 0.5) ? b_color_burn(b, 2.0 * s) : b_color_dodge(b, 2.0 * (s - 0.5));
}

float b_linear_light(float b, float s) {
	return (s < 0.5) ? b_linear_burn(b, 2.0 * s) : b_linear_dodge(b, 2.0 * (s - 0.5));
}

float b_pin_light(float b, float s) {
	return (s < 0.5) ? b_darken(b, 2.0 * s) : b_lighten(b, 2.0 * (s - 0.5));
}

float b_hard_mix(float b, float s) {
	return (b_vivid_light(b, s) < 0.5) ? 0.0 : 1.0;
}

float b_difference(float b, float s) {
	return abs(b - s);
}

float b_exclusion(float b, float s) {
	return b + s - (2.0 * b * s);
}

float b_subtract(float b, float s) {
	return max(b - s, 0.0);
}

float b_divide(float b, float s) {
	return (s <= 0.0) ? 1.0 : min(1.0, b / s);
}

vec3 blend_rgb(vec3 b, vec3 s) {
	if (blendmode == BLEND_DARKEN) {
		return vec3(b_darken(b.r, s.r), b_darken(b.g, s.g), b_darken(b.b, s.b));
	} else if (blendmode == BLEND_MULTIPLY) {
		return vec3(b_multiply(b.r, s.r), b_multiply(b.g, s.g), b_multiply(b.b, s.b));
	} else if (blendmode == BLEND_COLOR_BURN) {
		return vec3(b_color_burn(b.r, s.r), b_color_burn(b.g, s.g), b_color_burn(b.b, s.b));
	} else if (blendmode == BLEND_LINEAR_BURN) {
		return vec3(b_linear_burn(b.r, s.r), b_linear_burn(b.g, s.g), b_linear_burn(b.b, s.b));
	} else if (blendmode == BLEND_DARKER_COLOR) {
		return (lum(b) < lum(s)) ? b : s;
	} else if (blendmode == BLEND_LIGHTEN) {
		return vec3(b_lighten(b.r, s.r), b_lighten(b.g, s.g), b_lighten(b.b, s.b));
	} else if (blendmode == BLEND_SCREEN) {
		return vec3(b_screen(b.r, s.r), b_screen(b.g, s.g), b_screen(b.b, s.b));
	} else if (blendmode == BLEND_COLOR_DODGE) {
		return vec3(b_color_dodge(b.r, s.r), b_color_dodge(b.g, s.g), b_color_dodge(b.b, s.b));
	} else if (blendmode == BLEND_LINEAR_DODGE) {
		return vec3(b_linear_dodge(b.r, s.r), b_linear_dodge(b.g, s.g), b_linear_dodge(b.b, s.b));
	} else if (blendmode == BLEND_LIGHTER_COLOR) {
		return (lum(b) > lum(s)) ? b : s;
	} else if (blendmode == BLEND_OVERLAY) {
		return vec3(b_overlay(b.r, s.r), b_overlay(b.g, s.g), b_overlay(b.b, s.b));
	} else if (blendmode == BLEND_SOFT_LIGHT) {
		return vec3(b_soft_light(b.r, s.r), b_soft_light(b.g, s.g), b_soft_light(b.b, s.b));
	} else if (blendmode == BLEND_HARD_LIGHT) {
		return vec3(b_hard_light(b.r, s.r), b_hard_light(b.g, s.g), b_hard_light(b.b, s.b));
	} else if (blendmode == BLEND_VIVID_LIGHT) {
		return vec3(b_vivid_light(b.r, s.r), b_vivid_light(b.g, s.g), b_vivid_light(b.b, s.b));
	} else if (blendmode == BLEND_LINEAR_LIGHT) {
		return vec3(b_linear_light(b.r, s.r), b_linear_light(b.g, s.g), b_linear_light(b.b, s.b));
	} else if (blendmode == BLEND_PIN_LIGHT) {
		return vec3(b_pin_light(b.r, s.r), b_pin_light(b.g, s.g), b_pin_light(b.b, s.b));
	} else if (blendmode == BLEND_HARD_MIX) {
		return vec3(b_hard_mix(b.r, s.r), b_hard_mix(b.g, s.g), b_hard_mix(b.b, s.b));
	} else if (blendmode == BLEND_DIFFERENCE) {
		return vec3(b_difference(b.r, s.r), b_difference(b.g, s.g), b_difference(b.b, s.b));
	} else if (blendmode == BLEND_EXCLUSION) {
		return vec3(b_exclusion(b.r, s.r), b_exclusion(b.g, s.g), b_exclusion(b.b, s.b));
	} else if (blendmode == BLEND_SUBTRACT) {
		return vec3(b_subtract(b.r, s.r), b_subtract(b.g, s.g), b_subtract(b.b, s.b));
	} else if (blendmode == BLEND_DIVIDE) {
		return vec3(b_divide(b.r, s.r), b_divide(b.g, s.g), b_divide(b.b, s.b));
	} else if (blendmode == BLEND_HUE) {
		return set_lum(set_sat(s, sat(b)), lum(b));
	} else if (blendmode == BLEND_SATURATION) {
		return set_lum(set_sat(b, sat(s)), lum(b));
	} else if (blendmode == BLEND_COLOR) {
		return set_lum(s, lum(b));
	} else if (blendmode == BLEND_LUMINOSITY) {
		return set_lum(b, lum(s));
	}

	return s;
}

void main(void) {
	vec4 bg_color = texture2D(background, vTexCoord);
	vec4 fg_color = texture2D(foreground, vTexCoord);

	vec3 bg_rgb = (bg_color.a > 0.0001) ? bg_color.rgb / bg_color.a : bg_color.rgb;
	vec3 fg_rgb = (fg_color.a > 0.0001) ? fg_color.rgb / fg_color.a : fg_color.rgb;

	float coverage = clamp(fg_color.a * opacity, 0.0, 1.0);

	vec3 blended = blend_rgb(bg_rgb, fg_rgb);
	vec3 composite = mix(bg_rgb, blended, coverage);
	float alpha = coverage + (bg_color.a * (1.0 - coverage));

	gl_FragColor = vec4(composite * alpha, alpha);
}
