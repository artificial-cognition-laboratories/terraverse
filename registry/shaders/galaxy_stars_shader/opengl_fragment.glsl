uniform float starFade;

VARYING_ vec2 varCorner;
VARYING_ lowp vec4 varColour;

void main(void)
{
	float d = dot(varCorner, varCorner);
	float dot_ = exp(-d * 5.0) * (1.0 - smoothstep(0.7, 1.0, d));
	gl_FragColor = vec4(varColour.rgb, varColour.a * dot_ * starFade);
}
