// A star: its way out, moved in clip space by whole pixels, so it's round on
// screen wherever the view's perspective puts it.
uniform vec2 starPixel;

VARYING_ vec2 varCorner;
VARYING_ lowp vec4 varColour;

void main(void)
{
	varCorner = inTexCoord0.xy;
	varColour = inVertexColor;
	vec4 at = mWorldViewProj * inVertexPosition;
	at.xy += inTexCoord0.xy * inVertexNormal.x * starPixel * at.w;
	gl_Position = at;
}
