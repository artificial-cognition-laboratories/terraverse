// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "system.h"
#include "bridge.h"
#include "terragen/body.h"
#include "terragen/code.h"
#include "terragen/system.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace shell {

using namespace terragen;

static fix number(const Json::Value &v, const char *field, const std::string &where)
{
	if (!v[field].isNumeric())
		throw Refused(where + " needs a number '" + field + "'");
	double d = v[field].asDouble();
	if (d < -30000 || d > 30000)
		throw Refused(where + ": '" + field + "' is out of range");
	return (fix)(d * ONE);
}

static double real(fix v)
{
	return (double)v / ONE;
}

static Json::Value cardsJson(const std::vector<BodyCard> &cards, const std::vector<std::string> &ids)
{
	Json::Value out(Json::arrayValue);
	for (size_t i = 0; i < cards.size(); i++) {
		const BodyCard &c = cards[i];
		Json::Value j;
		j["id"] = ids[i];
		j["kind"] = KIND_NAMES[(int)c.kind];
		j["giant"] = c.giant;
		j["metal"] = real(c.metal);
		j["rock"] = real(c.rock);
		j["ice"] = real(c.ice);
		j["radius"] = real(c.radius);
		j["gravity"] = real(c.gravity);
		j["escape"] = real(c.escape);
		j["sunlight"] = real(c.sunlight);
		j["temperature"] = real(c.temperature);
		j["equilibrium"] = real(c.equilibrium);
		j["pressure"] = c.pressure < 0 ? Json::Value() : Json::Value(real(c.pressure));
		Json::Value gases(Json::objectValue);
		for (int g = 0; g < (int)Gas::Count; g++)
			if (c.gases[g] > 0)
				gases[GAS_NAMES[g]] = real(c.gases[g]);
		j["gases"] = gases;
		j["field"] = real(c.field);
		j["heat"] = real(c.heat);
		j["tidal"] = real(c.tidal);
		j["ocean"] = c.ocean;
		j["plates"] = c.plates;
		j["volcanism"] = c.volcanism;
		j["locked"] = c.locked;
		j["day"] = real(c.day);
		Json::Value layers(Json::arrayValue);
		for (const Layer &l : c.layers) {
			Json::Value e;
			e["name"] = l.name;
			e["outer"] = real(l.outer);
			e["molten"] = l.molten;
			layers.append(e);
		}
		j["layers"] = layers;
		Json::Value history(Json::arrayValue);
		for (const Moment &m : c.history) {
			Json::Value e;
			e["age"] = real(m.age);
			e["text"] = EVENT_TEXT[(int)m.event];
			history.append(e);
		}
		j["history"] = history;
		out.append(j);
	}
	return out;
}

Json::Value systemPreview(const Json::Value &intent)
{
	StarCauses star{ -1, number(intent, "starMass", "the system"), number(intent, "metallicity", "the system"),
			number(intent, "age", "the system"), number(intent, "activity", "the system") };
	for (int i = 0; i < STAR_COUNT; i++)
		if (intent["star"].isString() && intent["star"].asString() == STARS[i].id)
			star.type = i;
	if (star.type < 0)
		throw Refused("the system's star must be a star class");
	const Json::Value &second = intent["companion"];
	if (!second.isObject() || !second["kind"].isString())
		throw Refused("the system needs a 'companion'");
	const std::string pairing = second["kind"].asString();
	if (pairing != "none") {
		if (pairing != "close" && pairing != "wide")
			throw Refused("a companion is none, close or wide");
		star.companion.kind = pairing == "close" ? Pairing::Close : Pairing::Wide;
		star.companion.mass = number(second, "mass", "the companion");
		star.companion.separation = number(second, "separation", "the companion");
		star.companion.type = -1;
		for (int i = 0; i < STAR_COUNT; i++)
			if (second["class"].isString() && second["class"].asString() == STARS[i].id)
				star.companion.type = i;
		if (star.companion.type < 0)
			throw Refused("the companion's star must be a star class");
	}
	if (!intent["bodies"].isArray())
		throw Refused("the system needs a 'bodies' list");

	const auto spinOf = [](const Json::Value &b, const std::string &where) {
		for (int i = 0; i < (int)Spin::Count; i++)
			if (b["spin"].isString() && b["spin"].asString() == SPIN_NAMES[i])
				return (Spin)i;
		throw Refused(where + " needs a 'spin' this game has");
	};
	std::vector<BodyCauses> causes;
	std::vector<std::string> ids;
	std::unordered_map<std::string, int32_t> index;
	for (const Json::Value &b : intent["bodies"]) {
		if (!b["id"].isString())
			throw Refused("every body needs an id");
		std::string id = b["id"].asString(), where = "body " + id;
		int32_t parent = -1;
		if (!b["parent"].isNull()) {
			auto found = index.find(b["parent"].asString());
			if (found == index.end())
				throw Refused(where + ": its parent must come before it");
			parent = found->second;
		}
		if (!b["life"].isBool())
			throw Refused(where + " needs 'life' true or false");
		causes.push_back({ parent, number(b, "orbit", where), number(b, "eccentricity", where),
				number(b, "mass", where), number(b, "formed", where), number(b, "volatiles", where),
				number(b, "day", where), b["life"].asBool(), spinOf(b, where) });
		index[id] = (int32_t)ids.size();
		ids.push_back(id);
	}

	std::vector<BodyCard> cards;
	try {
		cards = bodies(star, causes);
	} catch (const std::invalid_argument &e) {
		throw Refused(e.what());
	}

	return cardsJson(cards, ids);
}

Json::Value systemJson(const SystemSpec &spec)
{
	Json::Value out;
	out["star"] = STARS[spec.star.type].id;
	out["starMass"] = real(spec.star.mass);
	out["metallicity"] = real(spec.star.metallicity);
	out["age"] = real(spec.star.age);
	out["activity"] = real(spec.star.activity);
	const Companion &second = spec.star.companion;
	Json::Value companion;
	companion["kind"] = second.kind == Pairing::None ? "none" : second.kind == Pairing::Close ? "close" : "wide";
	companion["class"] = STARS[second.type].id;
	companion["mass"] = second.kind == Pairing::None ? real(STARS[second.type].mass) : real(second.mass);
	companion["separation"] = real(second.separation);
	out["companion"] = companion;
	Json::Value bodies(Json::arrayValue);
	for (const SystemBody &b : spec.bodies) {
		const BodyCauses &c = b.causes;
		Json::Value j;
		j["id"] = b.id;
		j["name"] = b.name;
		j["parent"] = c.parent < 0 ? Json::Value() : Json::Value(spec.bodies.at((size_t)c.parent).id);
		j["color"] = b.colour;
		j["phase"] = b.phase;
		j["playable"] = b.playable;
		j["orbit"] = real(c.orbit);
		j["eccentricity"] = real(c.eccentricity);
		j["inclination"] = b.inclination;
		j["mass"] = std::log10(real(c.mass)); // the page's slider is logarithmic
		j["formed"] = real(c.formed);
		j["volatiles"] = real(c.volatiles);
		j["day"] = real(c.day);
		j["tilt"] = b.tilt;
		j["life"] = c.life;
		j["spin"] = SPIN_NAMES[(int)c.spin];
		j["land"] = Json::Value();
		if (b.land) {
			terragen::WorldCode w;
			w.setLand(*b.land);
			Json::Value l;
			l["continents"] = w.continents;
			for (const auto &[name, v] : { std::pair{ "land", w.land }, { "coast", w.coast }, { "mountains", w.mountains }, { "roughness", w.roughness },
					{ "temperature", w.temperature }, { "humidity", w.humidity }, { "rivers", w.rivers }, { "caves", w.caves }, { "ores", w.ores } })
				l[name] = real(v);
			j["land"] = l;
		}
		bodies.append(j);
	}
	out["bodies"] = bodies;
	return out;
}

Json::Value worldSystem(const Json::Value &intent)
{
	if (!intent["code"].isString())
		throw Refused("system.world takes { code }");
	const SystemSpec spec = [&] {
		try {
			return SystemSpec::forWorld(WorldCode::decode(intent["code"].asString()));
		} catch (const CodeError &e) {
			throw Refused(std::string("system.world: ") + e.what());
		}
	}();
	const System system = [&] {
		try {
			return System(spec);
		} catch (const std::invalid_argument &e) {
			throw Refused(std::string("system.world: ") + e.what());
		}
	}();
	Json::Value out = systemJson(spec);
	std::vector<std::string> ids;
	for (const SystemBody &b : spec.bodies)
		ids.push_back(b.id);
	out["cards"] = cardsJson(system.cards(), ids);
	return out;
}

} // namespace shell
