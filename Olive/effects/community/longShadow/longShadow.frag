#version 110

// Long Shadow: flat hard shadow cast from the clip's silhouette at an angle - the classic
// motion-graphics "flat design" look. Works on transparent clips, logos and text.

uniform sampler2D tex;
varying vec2 vTexCoord;
uniform vec2 resolution;
uniform float time;

uniform float angle;
uniform float length_s;
uniform float opacity;
uniform vec3 shadow_color;

void main(void) {
	vec4 src = texture2D(tex, vTexCoord);

	float a = radians(angle);
	vec2 dir = vec2(cos(a), sin(a)) * (length_s * 0.0006);

	float cover = 0.0;

	for (int i = 1; i <= 32; i++) {
		vec2 uv = vTexCoord + dir * float(i);
		if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
			break;
		}
		cover = max(cover, texture2D(tex, uv).a);
		if (cover > 0.9) {
			break;
		}
	}

	float shadow = cover * (1.0 - src.a) * clamp(opacity, 0.0, 1.0);

	// premultiplied output
	vec3 rgb = src.rgb + shadow_color * shadow;
	float alpha = clamp(src.a + shadow, 0.0, 1.0);

	gl_FragColor = vec4(rgb, alpha);
}
