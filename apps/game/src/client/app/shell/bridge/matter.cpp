// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/app/shell/bridge/matter.h"
#include "library/matter/matter/mixture.h"
#include "src/client/app/shell/bridge/bridge.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/porting.h"
#include <cmath>

namespace shell {

using namespace matter;

const Matter &MatterVerbs::table()
{
	std::call_once(m_loaded, [this] { m_table = Matter::load(porting::path_share + DIR_DELIM "matter"); });
	return *m_table;
}

static Json::Value pair(double t, double p)
{
	Json::Value v(Json::arrayValue);
	v.append(t);
	v.append(p);
	return v;
}

// Each boundary as [kelvin, pascals] points, enough to draw it smoothly.
static Json::Value curves(const Substance &s)
{
	Json::Value out;
	Json::Value sub(Json::arrayValue), vap(Json::arrayValue), melt(Json::arrayValue);
	const int N = 40;
	for (int i = 0; i <= N; i++) {
		double t = s.triple.t * (0.5 + 0.5 * i / N);
		sub.append(pair(t, s.sublimationPressure(t)));
		double tv = s.triple.t + (s.critical.t - s.triple.t) * i / N;
		vap.append(pair(tv, s.vapourPressure(tv)));
		// the melting line up to 10 GPa, evenly in log pressure
		double p = s.triple.p * std::pow(1e10 / s.triple.p, (double)i / N);
		melt.append(pair(s.meltingPoint(p), p));
	}
	out["sublimation"] = sub;
	out["vapour"] = vap;
	out["melting"] = melt;
	return out;
}

Json::Value MatterVerbs::list()
{
	const Matter &m = table();
	Json::Value out;
	out["elements"] = m.raw()["elements"];
	Json::Value subs(Json::arrayValue);
	const Json::Value &raw = m.raw()["substances"];
	for (Json::ArrayIndex i = 0; i < raw.size(); i++) {
		Json::Value s = raw[i];
		const Substance &x = m.substances()[i];
		s["molar_mass"] = x.molar_mass;
		s["curves"] = curves(x);
		subs.append(s);
	}
	out["substances"] = subs;
	out["reactions"] = m.raw()["reactions"];
	out["rocks"] = m.raw()["rocks"];
	return out;
}

static double positive(const Json::Value &v, const char *field, double most)
{
	if (!v[field].isNumeric() || v[field].asDouble() < 0 || v[field].asDouble() > most)
		throw Refused(std::string("matter.react: '") + field + "' must be a number from 0 to " + std::to_string(most));
	return v[field].asDouble();
}

Json::Value MatterVerbs::react(const Json::Value &intent)
{
	const Matter &m = table();
	Mixture mix(m);
	if (!intent["amounts"].isObject())
		throw Refused("matter.react needs 'amounts': { substance: moles }");
	for (const std::string &id : intent["amounts"].getMemberNames()) {
		size_t i;
		try {
			i = m.index(id);
		} catch (const MatterError &e) {
			throw Refused(e.what());
		}
		mix.amounts[i] = positive(intent["amounts"], id.c_str(), 1e6);
	}
	mix.temperature = std::max(1.0, positive(intent, "temperature", 1e5));
	if (intent["spark"].asBool())
		mix.temperature = std::max(mix.temperature, 1100.0); // enough to light most fuels
	Conditions c{ std::max(1.0, positive(intent, "surroundings", 1e5)), std::max(1e-9, positive(intent, "pressure", 1e11)),
			positive(intent, "power", 1e9), positive(intent, "light", 1e9) };
	double seconds = positive(intent, "seconds", 86400);
	std::vector<double> rates = seconds > 0 ? mix.run(seconds, c) : std::vector<double>(m.reactions().size(), 0.0);

	Json::Value out, amounts(Json::objectValue), states(Json::objectValue), running(Json::objectValue);
	for (size_t i = 0; i < mix.amounts.size(); i++) {
		if (mix.amounts[i] <= 1e-12)
			continue;
		amounts[m.substances()[i].id] = mix.amounts[i];
		states[m.substances()[i].id] = STATE_NAMES[(int)mix.state(i, c.pressure)];
	}
	for (size_t j = 0; j < rates.size(); j++)
		if (rates[j] > 0)
			running[m.reactions()[j].id] = rates[j];
	out["amounts"] = amounts;
	out["temperature"] = mix.temperature;
	out["states"] = states;
	out["rates"] = running;
	return out;
}

Json::Value MatterVerbs::state(const Json::Value &intent)
{
	if (!intent["id"].isString() || !intent["t"].isNumeric() || !intent["p"].isNumeric())
		throw Refused("matter.state needs { id, t (kelvin), p (pascals) }");
	double t = intent["t"].asDouble(), p = intent["p"].asDouble();
	if (!(t > 0) || !(p > 0))
		throw Refused("temperature and pressure must be positive");
	const Substance *s;
	try {
		s = &table().substance(intent["id"].asString());
	} catch (const MatterError &e) {
		throw Refused(e.what());
	}
	Json::Value out;
	out["state"] = STATE_NAMES[(int)s->state(t, p)];
	std::optional<double> boils = s->boilingPoint(p);
	out["boils_at"] = boils ? Json::Value(*boils) : Json::Value();
	out["melts_at"] = p > s->triple.p ? Json::Value(s->meltingPoint(p)) : Json::Value();
	return out;
}

} // namespace shell
