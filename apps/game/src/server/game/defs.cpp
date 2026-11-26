// Terraverse: blocks and items as data (sprint 119). See defs.h.
#include "src/server/game/defs.h"

#include "src/shared/base/config.h"
#if CHECK_CLIENT_BUILD()
#include "src/client/render/map/mesh/node_visuals.h" // ~NodeVisuals, as ContentFeatures moves
#endif

#include "src/shared/base/constants.h"
#include "src/shared/map/light/light.h"
#include "src/shared/game/items/tool.h"
#include "src/shared/util/text/string.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace defs {

namespace {

// A definition's fields, each read at most once; whatever's left unread when it's done
// is a field the engine doesn't know.
class Fields
{
public:
	Fields(const std::string &owner, const Json::Value &def) : m_owner(owner), m_def(def)
	{
		if (!def.isObject())
			throw std::invalid_argument(owner + ": a definition is an object");
	}

	[[noreturn]] void fail(const std::string &field, const std::string &what) const
	{
		throw std::invalid_argument(m_owner + ": " + field + " " + what);
	}

	const std::string &owner() const { return m_owner; }

	// The field, or null when it's not given.
	const Json::Value &operator[](const char *field)
	{
		m_read.insert(field);
		return m_def[field];
	}
	bool has(const char *field) const { return m_def.isMember(field); }

	std::string string(const char *field, const std::string &fallback = "")
	{
		const Json::Value &v = (*this)[field];
		if (v.isNull())
			return fallback;
		if (!v.isString())
			fail(field, "is text");
		return v.asString();
	}
	bool boolean(const char *field, bool fallback)
	{
		const Json::Value &v = (*this)[field];
		if (v.isNull())
			return fallback;
		if (!v.isBool())
			fail(field, "is true or false");
		return v.asBool();
	}
	double number(const char *field, double fallback)
	{
		const Json::Value &v = (*this)[field];
		if (v.isNull())
			return fallback;
		if (!v.isNumeric())
			fail(field, "is a number");
		return v.asDouble();
	}
	int integer(const char *field, int fallback)
	{
		const Json::Value &v = (*this)[field];
		if (v.isNull())
			return fallback;
		if (!v.isIntegral())
			fail(field, "is a whole number");
		return v.asInt();
	}

	// Every field given was one read.
	void done() const
	{
		for (const std::string &key : m_def.getMemberNames())
			if (!m_read.count(key))
				throw std::invalid_argument(m_owner + ": no field called " + key);
	}

private:
	std::string m_owner;
	const Json::Value &m_def;
	std::set<std::string> m_read;
};

template <typename E>
struct Named
{
	const char *name;
	E value;
};

template <typename E, size_t N>
E named(Fields &f, const char *field, const Named<E> (&table)[N], E fallback)
{
	const std::string s = f.string(field);
	if (s.empty())
		return fallback;
	for (const Named<E> &n : table)
		if (s == n.name)
			return n.value;
	f.fail(field, "has no kind called " + s);
}

const Named<NodeDrawType> DRAWTYPES[] = {
	{ "normal", NDT_NORMAL }, { "airlike", NDT_AIRLIKE }, { "liquid", NDT_LIQUID },
	{ "flowingliquid", NDT_FLOWINGLIQUID }, { "glasslike", NDT_GLASSLIKE },
	{ "allfaces", NDT_ALLFACES }, { "allfaces_optional", NDT_ALLFACES_OPTIONAL },
	{ "torchlike", NDT_TORCHLIKE }, { "signlike", NDT_SIGNLIKE }, { "plantlike", NDT_PLANTLIKE },
	{ "fencelike", NDT_FENCELIKE }, { "raillike", NDT_RAILLIKE }, { "nodebox", NDT_NODEBOX },
	{ "glasslike_framed", NDT_GLASSLIKE_FRAMED }, { "firelike", NDT_FIRELIKE },
	{ "glasslike_framed_optional", NDT_GLASSLIKE_FRAMED_OPTIONAL }, { "mesh", NDT_MESH },
	{ "plantlike_rooted", NDT_PLANTLIKE_ROOTED },
};
const Named<ContentParamType> PARAMTYPES[] = { { "none", CPT_NONE }, { "light", CPT_LIGHT } };
const Named<ContentParamType2> PARAMTYPES2[] = {
	{ "none", CPT2_NONE }, { "full", CPT2_FULL }, { "flowingliquid", CPT2_FLOWINGLIQUID },
	{ "facedir", CPT2_FACEDIR }, { "wallmounted", CPT2_WALLMOUNTED }, { "leveled", CPT2_LEVELED },
	{ "degrotate", CPT2_DEGROTATE }, { "meshoptions", CPT2_MESHOPTIONS }, { "color", CPT2_COLOR },
	{ "colorfacedir", CPT2_COLORED_FACEDIR }, { "colorwallmounted", CPT2_COLORED_WALLMOUNTED },
	{ "glasslikeliquidlevel", CPT2_GLASSLIKE_LIQUID_LEVEL },
	{ "colordegrotate", CPT2_COLORED_DEGROTATE }, { "4dir", CPT2_4DIR }, { "color4dir", CPT2_COLORED_4DIR },
};
const Named<LiquidType> LIQUIDTYPES[] = {
	{ "none", LIQUID_NONE }, { "flowing", LIQUID_FLOWING }, { "source", LIQUID_SOURCE },
};
const Named<NodeBoxType> BOXTYPES[] = {
	{ "regular", NODEBOX_REGULAR }, { "fixed", NODEBOX_FIXED }, { "wallmounted", NODEBOX_WALLMOUNTED },
	{ "leveled", NODEBOX_LEVELED },
};
const Named<AlphaMode> ALPHAS[] = {
	{ "blend", ALPHAMODE_BLEND }, { "clip", ALPHAMODE_CLIP }, { "opaque", ALPHAMODE_OPAQUE },
};
const Named<ItemType> ITEMTYPES[] = { { "none", ITEM_NONE }, { "craft", ITEM_CRAFT }, { "tool", ITEM_TOOL } };

// "#rrggbb", a colour's name, or { a, r, g, b } (a defaults to 255).
video::SColor colour(Fields &f, const char *field, const Json::Value &v)
{
	video::SColor c;
	if (v.isString()) {
		if (!parseColorString(v.asString(), c, true))
			f.fail(field, "isn't a colour: " + v.asString());
		return c;
	}
	if (!v.isObject())
		f.fail(field, "is a colour: \"#rrggbb\" or { a, r, g, b }");
	Fields parts(f.owner() + ": " + field, v);
	const auto part = [&](const char *name, double fallback) {
		return (u32)std::clamp(parts.number(name, fallback), 0.0, 255.0);
	};
	c.set(part("a", 255), part("r", 0), part("g", 0), part("b", 0));
	parts.done();
	return c;
}

ItemGroupList groups(Fields &f)
{
	ItemGroupList out;
	const Json::Value &v = f["groups"];
	if (v.isNull())
		return out;
	if (!v.isObject())
		f.fail("groups", "are { group: rating }");
	for (const std::string &g : v.getMemberNames()) {
		if (!v[g].isIntegral())
			f.fail("groups", g + " is a whole number");
		if (const int rating = v[g].asInt(); rating != 0) // 0 is not in the group
			out[g] = rating;
	}
	return out;
}

TileAnimationParams animation(const std::string &owner, const Json::Value &v)
{
	TileAnimationParams a;
	a.type = TAT_NONE;
	if (v.isNull())
		return a;
	Fields f(owner + ": animation", v);
	const std::string type = f.string("type");
	if (type == "vertical_frames") {
		a.type = TAT_VERTICAL_FRAMES;
		a.vertical_frames.aspect_w = f.integer("aspect_w", 16);
		a.vertical_frames.aspect_h = f.integer("aspect_h", 16);
		a.vertical_frames.length = f.number("length", 1);
	} else if (type == "sheet_2d") {
		a.type = TAT_SHEET_2D;
		a.sheet_2d.frames_w = f.integer("frames_w", 1);
		a.sheet_2d.frames_h = f.integer("frames_h", 1);
		a.sheet_2d.frame_length = f.number("frame_length", 1);
	} else {
		f.fail("type", "is vertical_frames or sheet_2d");
	}
	f.done();
	return a;
}

// A tile: its texture, or { name, backface_culling, color, animation, … }. Whether it
// tiles and culls by default depends on how the block's drawn, as the engine's did.
TileDef tile(const std::string &owner, const Json::Value &v, NodeDrawType drawtype, bool special)
{
	bool tiling = true, culling = true;
	switch (drawtype) {
	case NDT_PLANTLIKE:
	case NDT_FIRELIKE:
		tiling = false;
		[[fallthrough]];
	case NDT_MESH:
	case NDT_LIQUID:
		culling = false;
		break;
	case NDT_PLANTLIKE_ROOTED:
		tiling = !special;
		culling = !special;
		break;
	default:
		break;
	}
	TileDef t;
	t.tileable_horizontal = t.tileable_vertical = tiling;
	t.backface_culling = culling;
	if (v.isString()) {
		t.name = v.asString();
		return t;
	}
	Fields f(owner + ": a tile", v);
	t.name = f.string("name");
	if (t.name.empty())
		f.fail("name", "is required");
	t.backface_culling = f.boolean("backface_culling", culling);
	t.tileable_horizontal = f.boolean("tileable_horizontal", tiling);
	t.tileable_vertical = f.boolean("tileable_vertical", tiling);
	if (const std::string align = f.string("align_style"); !align.empty())
		t.align_style = align == "user" ? ALIGN_STYLE_USER_DEFINED : align == "world" ? ALIGN_STYLE_WORLD : ALIGN_STYLE_NODE;
	t.scale = f.integer("scale", 0);
	if (const Json::Value &c = f["color"]; !c.isNull()) {
		t.color = colour(f, "color", c);
		t.has_color = true;
	}
	t.animation = animation(f.owner(), f["animation"]);
	f.done();
	return t;
}

// Up to six tiles (top, bottom, right, left, back, front); the last given fills the rest.
void tiles(Fields &f, const char *field, TileDef (&out)[6], NodeDrawType drawtype)
{
	const Json::Value &v = f[field];
	if (v.isNull())
		return;
	if (!v.isArray() || v.empty() || v.size() > 6)
		f.fail(field, "are a list of one to six tiles");
	for (Json::ArrayIndex i = 0; i < 6; i++)
		out[i] = tile(f.owner(), v[std::min(i, v.size() - 1)], drawtype, false);
}

// A box, in nodes from the block's centre: [x1, y1, z1, x2, y2, z2].
aabb3f box(Fields &f, const char *field, const Json::Value &v)
{
	if (!v.isArray() || v.size() != 6)
		f.fail(field, "is a box: [x1, y1, z1, x2, y2, z2]");
	float n[6];
	for (Json::ArrayIndex i = 0; i < 6; i++) {
		if (!v[i].isNumeric())
			f.fail(field, "is a box of numbers");
		n[i] = v[i].asFloat() * BS;
	}
	aabb3f b(n[0], n[1], n[2], n[3], n[4], n[5]);
	b.repair();
	return b;
}

NodeBox nodebox(Fields &owner, const char *field)
{
	NodeBox nb;
	const Json::Value &v = owner[field];
	if (v.isNull())
		return nb;
	Fields f(owner.owner() + ": " + field, v);
	nb.type = named(f, "type", BOXTYPES, NODEBOX_REGULAR);
	// one box, or a list of them
	if (const Json::Value &fixed = f["fixed"]; !fixed.isNull()) {
		if (fixed.isArray() && !fixed.empty() && fixed[0].isArray())
			for (const Json::Value &b : fixed)
				nb.fixed.push_back(box(f, "fixed", b));
		else
			nb.fixed.push_back(box(f, "fixed", fixed));
	}
	if (const Json::Value &b = f["wall_top"]; !b.isNull())
		nb.wall_top = box(f, "wall_top", b);
	if (const Json::Value &b = f["wall_bottom"]; !b.isNull())
		nb.wall_bottom = box(f, "wall_bottom", b);
	if (const Json::Value &b = f["wall_side"]; !b.isNull())
		nb.wall_side = box(f, "wall_side", b);
	f.done();
	return nb;
}

// How long the hand or a tool takes on each group, by its rating: { group: { times:
// { rating: seconds } or [ seconds for rating 1, 2, … ], uses, maxlevel } }.
ToolCapabilities toolCapabilities(Fields &owner)
{
	Fields f(owner.owner() + ": tool_capabilities", owner["tool_capabilities"]);
	ToolCapabilities caps;
	caps.full_punch_interval = f.number("full_punch_interval", caps.full_punch_interval);
	caps.max_drop_level = f.integer("max_drop_level", caps.max_drop_level);
	caps.punch_attack_uses = f.integer("punch_attack_uses", caps.punch_attack_uses);
	if (const Json::Value &gc = f["groupcaps"]; !gc.isNull()) {
		if (!gc.isObject())
			f.fail("groupcaps", "are { group: { times, uses, maxlevel } }");
		for (const std::string &group : gc.getMemberNames()) {
			Fields g(f.owner() + ": " + group, gc[group]);
			ToolGroupCap cap;
			cap.uses = g.integer("uses", cap.uses);
			cap.maxlevel = g.integer("maxlevel", cap.maxlevel);
			const Json::Value &times = g["times"];
			if (times.isArray()) {
				for (Json::ArrayIndex i = 0; i < times.size(); i++)
					if (times[i].isNumeric())
						cap.times.emplace_back(int(i) + 1, times[i].asFloat());
					else
						g.fail("times", "are seconds");
			} else if (times.isObject()) {
				for (const std::string &rating : times.getMemberNames()) {
					if (!times[rating].isNumeric())
						g.fail("times", "are seconds");
					cap.times.emplace_back(stoi(rating), times[rating].asFloat());
				}
			} else if (!times.isNull()) {
				g.fail("times", "are { rating: seconds } or a list");
			}
			g.done();
			caps.groupcaps[group] = cap;
		}
	}
	if (const Json::Value &dg = f["damage_groups"]; !dg.isNull()) {
		if (!dg.isObject())
			f.fail("damage_groups", "are { group: damage }");
		for (const std::string &group : dg.getMemberNames())
			caps.damageGroups[group] = dg[group].asInt();
	}
	f.done();
	return caps;
}

// What a block's made of (`_matter`: a substance's id, or { matter, share, state, form,
// vessel, loose }), a family of compositions (`_family`) and its part in a machine
// (`_machine`): see matter/block.h.
matter::State state(Fields &f)
{
	const std::string s = f.string("state", "solid");
	if (s != "solid" && s != "liquid")
		f.fail("state", "is solid or liquid");
	return s == "solid" ? matter::State::Solid : matter::State::Liquid;
}

void matterFields(Fields &f, ContentFeatures &n)
{
	if (const Json::Value &m = f["_matter"]; m.isString()) {
		n.matter = MatterBlock{ m.asString() };
	} else if (!m.isNull()) {
		Fields b(f.owner() + ": _matter", m);
		MatterBlock out;
		out.matter = b.string("matter");
		if (out.matter.empty())
			b.fail("matter", "is required");
		out.share = b.number("share", 1);
		out.state = state(b);
		out.form = b.boolean("form", false);
		out.vessel = b.boolean("vessel", false);
		out.loose = b.boolean("loose", false);
		b.done();
		n.matter = out;
	}
	if (const Json::Value &m = f["_family"]; !m.isNull()) {
		Fields b(f.owner() + ": _family", m);
		MatterFamily fam;
		fam.share = b.number("share", 1);
		fam.state = state(b);
		fam.form = b.boolean("form", false);
		const Json::Value &points = b["points"];
		if (!points.isArray() || points.empty())
			b.fail("points", "are a list of { substance: share }");
		for (const Json::Value &p : points) {
			if (!p.isObject())
				b.fail("points", "are { substance: share }");
			std::vector<std::pair<std::string, double>> parts;
			for (const std::string &s : p.getMemberNames()) // (sorted: the same every time)
				parts.push_back({ s, p[s].asDouble() });
			fam.points.push_back(std::move(parts));
		}
		if (const Json::Value &holds = b["holds"]; !holds.isNull()) {
			if (!holds.isArray() || holds.size() != points.size())
				b.fail("holds", "are a number for each point");
			for (const Json::Value &h : holds)
				fam.holds.push_back(h.asDouble());
		}
		b.done();
		n.family = std::move(fam);
	}
	n.matter_generated = f.boolean("_matter_generated", false);
	if (const Json::Value &m = f["_machine"]; !m.isNull()) {
		Fields b(f.owner() + ": _machine", m);
		MatterMachine out;
		const auto opt = [&](const char *field, std::optional<float> &v) {
			if (b.has(field))
				v = (float)b.number(field, 0);
		};
		opt("mass", out.mass);
		opt("friction", out.friction);
		opt("tension", out.tension);
		opt("compression", out.compression);
		opt("toughness", out.toughness);
		opt("power", out.power);
		opt("rpm", out.rpm);
		out.role = b.string("role");
		b.done();
		n.machine = out;
	}
}

// What an item and a block share: how it's named, shown and grouped.
void itemFields(Fields &f, ItemDefinition &d)
{
	d.description = f.string("description");
	d.short_description = f.string("short_description");
	d.inventory_image = f.string("inventory_image");
	d.wield_image = f.string("wield_image");
	d.groups = groups(f);
	// (a stack holds 99, as the engine's: TEMP, until what's carried is held by its mass)
}

} // namespace

Block block(const std::string &name, const Json::Value &def)
{
	Fields f(name, def);
	Block b;
	ItemDefinition &d = b.item;
	ContentFeatures &n = b.node;

	d.type = ITEM_NODE;
	d.name = n.name = name;
	itemFields(f, d);
	n.groups = d.groups;
	// a block placed is predicted by the one placing it, unless it says otherwise
	d.node_placement_prediction = f.string("node_placement_prediction", name);

	n.drawtype = named(f, "drawtype", DRAWTYPES, NDT_NORMAL);
	n.visual_scale = f.number("visual_scale", 1);
	n.mesh = f.string("mesh");
	tiles(f, "tiles", n.tiledef, n.drawtype);
	tiles(f, "overlay_tiles", n.tiledef_overlay, n.drawtype);
	if (const Json::Value &special = f["special_tiles"]; !special.isNull()) {
		if (!special.isArray() || special.size() > CF_SPECIAL_COUNT)
			f.fail("special_tiles", "are a list of up to six tiles");
		for (Json::ArrayIndex i = 0; i < special.size(); i++)
			n.tiledef_special[i] = tile(name, special[i], n.drawtype, true);
	}
	n.setDefaultAlphaMode();
	n.alpha = named(f, "use_texture_alpha", ALPHAS, n.alpha);

	if (const Json::Value &c = f["color"]; !c.isNull())
		d.color = n.color = colour(f, "color", c);
	d.palette_image = n.palette_name = f.string("palette");
	if (const Json::Value &c = f["post_effect_color"]; !c.isNull())
		n.post_effect_color = colour(f, "post_effect_color", c);

	n.param_type = named(f, "paramtype", PARAMTYPES, CPT_NONE);
	n.param_type_2 = named(f, "paramtype2", PARAMTYPES2, CPT2_NONE);
	n.liquid_type = named(f, "liquidtype", LIQUIDTYPES, LIQUID_NONE);
	if (n.liquid_type == LIQUID_FLOWING)
		n.param_type_2 = CPT2_FLOWINGLIQUID; // a flowing liquid keeps its level in param2
	const bool coloured = n.param_type_2 == CPT2_COLOR || n.param_type_2 == CPT2_COLORED_FACEDIR ||
			n.param_type_2 == CPT2_COLORED_WALLMOUNTED || n.param_type_2 == CPT2_COLORED_DEGROTATE ||
			n.param_type_2 == CPT2_COLORED_4DIR;
	if (!n.palette_name.empty() && !coloured)
		f.fail("palette", "needs a colour paramtype2 to pick from it");

	n.is_ground_content = f.boolean("is_ground_content", true);
	n.light_propagates = n.param_type == CPT_LIGHT;
	n.sunlight_propagates = f.boolean("sunlight_propagates", false);
	n.walkable = f.boolean("walkable", true);
	n.pointable = f.boolean("pointable", true) ? PointabilityType::POINTABLE : PointabilityType::POINTABLE_NOT;
	n.diggable = f.boolean("diggable", true);
	n.climbable = f.boolean("climbable", false);
	n.buildable_to = f.boolean("buildable_to", false);
	n.floodable = f.boolean("floodable", false);
	n.rightclickable = false; // nothing's done by right-clicking a block yet
	n.liquid_alternative_flowing = f.string("liquid_alternative_flowing");
	n.liquid_alternative_source = f.string("liquid_alternative_source");
	n.liquid_viscosity = f.integer("liquid_viscosity", 0);
	n.move_resistance = f.integer("move_resistance", n.liquid_viscosity);
	n.liquid_range = f.integer("liquid_range", n.liquid_range);
	n.liquid_renewable = f.boolean("liquid_renewable", true);
	n.liquid_move_physics = f.boolean("liquid_move_physics", n.liquid_type != LIQUID_NONE);
	n.leveled = f.integer("leveled", 0);
	n.leveled_max = f.integer("leveled_max", n.leveled_max);
	n.drowning = f.integer("drowning", 0);
	n.light_source = f.integer("light_source", 0);
	if (n.light_source > LIGHT_MAX)
		f.fail("light_source", "is at most " + std::to_string(LIGHT_MAX));
	n.damage_per_second = f.integer("damage_per_second", 0);
	n.waving = f.integer("waving", 0);

	n.node_box = nodebox(f, "node_box");
	n.selection_box = nodebox(f, "selection_box");
	n.collision_box = nodebox(f, "collision_box");
	// a shaped block is picked out by its shape unless it says otherwise
	if (n.drawtype == NDT_NODEBOX && !f.has("selection_box"))
		n.selection_box = n.node_box;

	matterFields(f, n);
	if (f.has("drop"))
		n.drop = f.string("drop");
	n.rotates = f.boolean("_rotates", false);
	if (n.rotates && n.param_type_2 != CPT2_FACEDIR && n.param_type_2 != CPT2_COLORED_FACEDIR)
		f.fail("_rotates", "needs paramtype2 facedir");
	f.done();
	return b;
}

ItemDefinition item(const std::string &name, const Json::Value &def)
{
	Fields f(name.empty() ? "the hand" : name, def);
	ItemDefinition d;
	d.name = name;
	d.type = named(f, "type", ITEMTYPES, ITEM_CRAFT);
	itemFields(f, d);
	if (d.type == ITEM_TOOL)
		d.stack_max = 1;
	if (f.has("tool_capabilities"))
		d.tool_capabilities = new ToolCapabilities(toolCapabilities(f));
	else if (name.empty())
		f.fail("tool_capabilities", "are the hand's: what it digs, and how fast");
	f.done();
	return d;
}

} // namespace defs
