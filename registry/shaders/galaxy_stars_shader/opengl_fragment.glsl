uniform float starFade;

VARYING_ vec2 varCorner;
VARYING_ lowp vec4 varColour;

void main(void)
{
	// a point and its glow: a core about a pixel across, and a faint halo
	// out to the quad's edge, so a star reads as light, not a dot of paint
	float d = dot(varCorner, varCorner);
	float dot_ = (exp(-d * 14.0) + 0.12 * exp(-d * 3.5)) * (1.0 - smoothstep(0.6, 1.0, d));
	gl_FragColor = vec4(varColour.rgb, varColour.a * dot_ * starFade);
}
