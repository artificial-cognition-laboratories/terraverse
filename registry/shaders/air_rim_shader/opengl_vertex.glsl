// A shell round a body; the fragment shader works out what a sight line meets.
uniform mat4 mWorld;

VARYING_ highp vec3 varWorld;

void main(void)
{
	varWorld = (mWorld * inVertexPosition).xyz;
	gl_Position = mWorldViewProj * inVertexPosition;
}
