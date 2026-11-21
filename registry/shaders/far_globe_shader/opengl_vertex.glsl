// The far view on an atlas planet (src/far/layout.cpp, GlobeLayout): its
// tiles are built on the planet, round each tile's middle, and drawn as the
// near world's blocks are (AtlasView: the eye's one map), so no lens here.
// Vertex colours are each face's colour and shade, as the near mesh's are
// in full daylight; they're lit here as the near shader lights them. The
// first texture coordinate is the vertex's height over the sea, in blocks; the
// second where it is in its tile's grid, in cells (far/mesh.h), for the mask.
uniform vec3 dayLight;
uniform float farBehind; // 1: a stand-in, where a finer tile isn't built yet

// The depth range split in two: the land in front, and behind it all, in a
// thin slice at the back, what only fills gaps: a tile's skirts (alpha 0,
// hung below its edges: far/mesh.cpp) and the stand-ins. So they're drawn in
// the land's own pass and draw, yet never cover land (one drawn level with
// the finer land beside it stood over it; a skirt stood as a wall in a gap),
// and keep their own order among themselves.
const float BACK = 0.002; // of the depth range (−1..1), the slice's

const float BS = 10.0;
VARYING_ lowp vec4 varColor;
VARYING_ highp vec3 eyeVec;
VARYING_ highp float varAltitude; // m above the sea
VARYING_ lowp vec3 varDay;        // the daylight, for the air
VARYING_ highp float varSeen;     // how far off it's seen, scene units
VARYING_ highp vec2 varGrid;      // in the tile's grid, cells from its corner

void main(void)
{
	vec4 pos = inVertexPosition;
	varAltitude = inTexCoord0.x;
	varGrid = inTexCoord1.xy;
	varDay = dayLight;
	gl_Position = mWorldViewProj * pos;
	float z = clamp(gl_Position.z / gl_Position.w, -1.0, 1.0);
	bool back = inVertexColor.a < 0.5 || farBehind > 0.5;
	z = back ? 1.0 - BACK * (1.0 - z) : -1.0 + (2.0 - 2.0 * BACK) * (z + 1.0) / 2.0;
	gl_Position.z = z * gl_Position.w;
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
