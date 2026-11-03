// A body's land from its tiles, lit from where its star is (planetarium/lit.h).
uniform mat4 mWorld;

VARYING_ lowp vec4 varColor;
VARYING_ highp vec3 varWorld;

void main(void)
{
	varWorld = (mWorld * inVertexPosition).xyz;
	varColor = inVertexColor;
	gl_Position = mWorldViewProj * inVertexPosition;
}
