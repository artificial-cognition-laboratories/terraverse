// The air over space: a quad over the whole screen, and each corner's way out.
uniform vec3 airAhead;
uniform vec3 airRight;
uniform vec3 airUp;

VARYING_ highp vec3 varRay;

void main(void)
{
	gl_Position = vec4(inVertexPosition.xy, 0.0, 1.0);
	varRay = airAhead + airRight * inVertexPosition.x + airUp * inVertexPosition.y;
}
