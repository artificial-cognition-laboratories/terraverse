// The liquid layer's fine water (client/liquids.cpp), drawn from the grid's
// heights: one mesh, a tile's columns (each a top and four sides), drawn
// once per tile with water. Each vertex finds its column's top (and its
// neighbour's) in the height texture; a face not there (dry, or a side with
// nothing lower beside it) is put behind the eye and never drawn. So
// moving water is a texture upload, never a mesh rebuilt.
//
// Vertex: position x, z the corner in columns within the tile, y 0 (a
// side's foot) or 1 (its head); texture x the face (0 top, 1..4 the sides
// +x −x +z −z, 5 a level tile's one top), y the column in the tile (x + 16 z).
// Two grids share it: a column's a sixteenth of a node (fine) or a quarter (coarse).
uniform sampler2D texture0; // per column: r its top, g its ground (sixteenths over the grid's base; −1e9: none), b its light, a its foam (client/liquidlook.h)
uniform highp vec4 waterGrid;   // scene x, y, z of the grid's corner at its base; columns a side
uniform highp vec4 waterTile;   // the tile's first column x, z; its tops' mean; its light
uniform highp vec4 waterColour; // the liquid's; columns a node

VARYING_ lowp vec4 varColor;
VARYING_ highp vec3 eyeVec;

const float BS = 10.0;
const float NONE = -1.0e8;
const float SHEEN = 0.22;                         // tops catch the sky (clear liquids are still seen)
const vec3 SKY = vec3(190.0, 215.0, 235.0) / 255.0;
const float ALPHA = 175.0 / 255.0;
const vec3 FOAM = vec3(0.92, 0.96, 1.0);

void main(void)
{
	int face = int(inTexCoord0.x + 0.5);
	int c = int(inTexCoord0.y + 0.5);
	ivec2 col = ivec2(int(waterTile.x) + c - (c / 16) * 16, int(waterTile.y) + c / 16);
	vec4 here = texelFetch(texture0, col, 0);
	float h = here.r;
	bool shown = h > NONE;
	float y = h;
	if (face == 5) {
		y = waterTile.z; // a level tile: its one height
		shown = true;
	} else if (face > 0 && shown) {
		ivec2 step = face == 1 ? ivec2(1, 0) : face == 2 ? ivec2(-1, 0) : face == 3 ? ivec2(0, 1) : ivec2(0, -1);
		ivec2 n = col + step;
		int size = int(waterGrid.w);
		if (n.x < 0 || n.y < 0 || n.x >= size || n.y >= size) {
			shown = false; // the grid's edge: the flat water beyond meets it
		} else {
			vec2 there = texelFetch(texture0, n, 0).rg;
			float low = there.r > NONE ? there.r : there.g;
			shown = there.g > NONE && low < h;
			y = mix(low, h, inVertexPosition.y);
		}
	}
	if (!shown) {
		gl_Position = vec4(0.0, 0.0, -2.0, 1.0); // past the near plane: clipped
		varColor = vec4(0.0);
		eyeVec = vec3(0.0);
		return;
	}
	float res = waterColour.a;
	vec4 p = vec4(waterGrid.x + (waterTile.x + inVertexPosition.x) / res * BS,
		waterGrid.y + y / 16.0 * BS,
		waterGrid.z + (waterTile.y + inVertexPosition.z) / res * BS, 1.0);
	gl_Position = mWorldViewProj * p;
	eyeVec = -(mWorldView * p).xyz;

	float shade = waterTile.w;
	if (face == 0 || face == 5) {
		// deep reads deep (darker, less clear); higher than the rest a little lighter (ripples are seen)
		float deep = clamp((h - here.g) / 16.0, 0.0, 1.0);
		float by = (1.0 + 0.3 * (h - waterTile.z) / 16.0) * (1.0 - 0.35 * deep);
		if (face == 0)
			by *= here.b;
		vec3 lit = mix(mix(waterColour.rgb, SKY, SHEEN), FOAM, face == 0 ? here.a : 0.0);
		varColor = vec4(clamp(lit * shade * by, 0.0, 1.0), ALPHA + 60.0 / 255.0 * deep);
	} else {
		varColor = vec4(clamp(waterColour.rgb * shade * 0.85, 0.0, 1.0), ALPHA);
	}
}
