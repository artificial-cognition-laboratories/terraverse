// The unit sphere round the camera: each point is its own way out.
VARYING_ highp vec3 varWay;

void main(void)
{
	varWay = inVertexPosition.xyz;
	gl_Position = mWorldViewProj * inVertexPosition;
}
