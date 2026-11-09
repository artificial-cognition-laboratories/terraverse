// The far view on an atlas planet (src/far/layout.cpp, GlobeLayout): its
// tiles are built on the planet, round each tile's middle, and drawn as the
// near world's blocks are (AtlasView: the eye's one map), so no lens here.
// Vertex colours are each face's colour and shade, as the near mesh's are
// in full daylight; they're lit here as the near shader lights them. The
// first texture coordinate is the vertex's height over the sea, in blocks.
uniform mat4 mWorld;
uniform vec3 dayLight;
uniform float farCut;  // the near world's range from the eye (scene units)
uniform float farCell; // the tile's cells (blocks)

const float BS = 10.0;
// Beside the near world, a tile's land sinks under the true ground: a cell is
// its middle's height across its width, as much as a cell or more off the
// blocks beside it (most while coarse stand-ins fill in), and higher, its
// edge stood as a wall over them. Sunk by this many of its cells, easing out
// over a tile's width past the cut.
const float SINK = 1.5;
const float BAND = 32.0; // cells: a tile's

VARYING_ lowp vec4 varColor;
VARYING_ highp vec3 eyeVec;
VARYING_ highp float varAltitude; // m above the sea
VARYING_ lowp vec3 varDay;        // the daylight, for the air
VARYING_ highp float varSeen;     // how far off it's seen, scene units

void main(void)
{
	vec4 pos = inVertexPosition;
	float seen = length((mWorldView * pos).xyz);
	float sink = farCut > 0.0 ? SINK * farCell * BS * (1.0 - smoothstep(farCut, farCut + BAND * max(farCell, 1.0) * BS, seen)) : 0.0; // (none without a near world)
	// down in the scene (the eye's up near it), in the tile's own frame: the
	// world matrix is a rotation, so its transpose takes it back
	vec3 down = -normalize(vec3(mWorld[0][1], mWorld[1][1], mWorld[2][1]));
	pos.xyz += down * sink;
	varAltitude = inTexCoord0.x;
	varDay = dayLight;
	gl_Position = mWorldViewProj * pos;
	eyeVec = -(mWorldView * pos).xyz;
	varSeen = length(eyeVec);

	// as nodes_shader lights a block in full sun
	vec4 color = inVertexColor;
	color.a = 1.0;
	color.rgb *= dayLight;
	float brightness = (color.r + color.g + color.b) / 3.0;
	color.b += max(0.0, 0.021 - abs(0.2 * brightness - 0.021) + 0.07 * brightness);
	varColor = clamp(color, 0.0, 1.0);
}
