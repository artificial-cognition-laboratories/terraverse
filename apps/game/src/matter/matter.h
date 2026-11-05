// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <json/json.h>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/*
	Matter: the building blocks. Elements (identity only: the periodic
	table) and substances (water, iron, methane, …) with the properties that
	decide how they behave: where they melt, boil and sublime at any
	pressure, their latent heats and densities. The engine knows what a
	triple point is; which substances exist, and their numbers, are data
	(matter/*.json at the repo root), so a game can add its own.

	Runtime physics, not world generation: doubles are fine here (terragen
	keeps its own fixed point). See AGENTS.md "Engine primitives".
*/
namespace matter {

// A data file that can't be read or doesn't make sense.
struct MatterError : std::runtime_error
{
	using std::runtime_error::runtime_error;
};

enum class State : uint8_t { Solid, Liquid, Gas, Supercritical };
extern const char *const STATE_NAMES[4];

struct Element
{
	int number;
	std::string symbol, name, category;
	double mass; // standard atomic weight, g/mol
};

struct Point
{
	double t; // kelvin
	double p; // pascals
};

struct Substance
{
	std::string id, name, formula;
	std::map<std::string, int> composition; // element symbol → atoms
	double molar_mass;                       // g/mol, from the composition
	Point triple, critical;
	std::optional<double> boil;              // at 1 atm, where it can be liquid there
	std::optional<double> sublime;           // at 1 atm, where it can't (then the triple point is above 1 atm)
	double fusion, vaporization;             // kJ/mol
	double melt_slope;                       // K per MPa along the melting line (negative: water)
	double heat_capacity;                    // J/(kg·K)
	double density_solid, density_liquid;    // kg/m³
	double conductivity[3];                  // W/(m·K): solid, liquid, gas
	bool approximate;                        // some anchors are estimates
	// Sprint 79, from thermo.json (scripts/matter-data): its enthalpy of
	// formation, kJ/mol, in its standard state (as it is at 298.15 K and
	// 1 atm); and, for the fluids CoolProp models, its saturation curve
	// (vapour pressure and liquid density, by temperature, rising)
	double formation = 0;
	State standard = State::Solid;
	struct Saturation
	{
		std::vector<double> t, p, liquid;
	};
	std::optional<Saturation> saturation;
	// How it looks (sprint 79): its colour (0xRRGGBB) and how strongly a
	// little of it colours what it's in (1: as much as its mass; cobalt
	// oxide's tens: a pinch turns glass blue)
	uint32_t colour = 0;
	double tint = 1;
	// What it dissolves in (sprint 79): a gas or solid held in that liquid,
	// up to `most` moles of it per mole of the solvent (hydrogen chloride
	// in water is hydrochloric acid; salt in water, brine), counted as
	// liquid while it's held. More than that stays as it would be.
	struct Dissolves
	{
		size_t solvent;
		double most;
		double heat; // kJ/mol given off going in (taken: negative), its heat of solution
	};
	std::optional<Dissolves> dissolves;

	State state(double t, double p) const;
	double vapourPressure(double t) const;   // liquid–gas, triple..critical
	double sublimationPressure(double t) const; // solid–gas, below the triple point
	double meltingPoint(double p) const;     // solid–liquid, above the triple point's pressure
	// Where it boils at this pressure, if it can be liquid there.
	std::optional<double> boilingPoint(double p) const;
	// kg/m³ in a state at t and p (a gas or supercritical fluid as an ideal gas).
	double density(State s, double t, double p) const;
	// W/(m·K) in a state (a supercritical fluid conducts like its gas).
	double conducts(State s) const;
};

constexpr double ATMOSPHERE = 101325.0;
constexpr double GAS_CONSTANT = 8.314462618; // J/(mol·K)

// What drives a reaction besides heat.
enum class Energy : uint8_t { None, Electricity, Light };

/*
	A reaction: reactants become products, in whole proportions that keep
	every element's atoms (checked on load). `heat` is released per reaction
	as written (negative: it takes heat). A heat-driven reaction runs at
	factor·e^(−activation/RT) per second of its limiting reactant; one driven
	by electricity or light runs as fast as that power pays for its heat.
	It needs every requirement met (a substance present in a state).
*/
struct Reaction
{
	struct Term
	{
		size_t substance; // index into Matter::substances()
		double count;
	};
	struct Requirement
	{
		size_t substance;
		State state;
	};

	std::string id, name;
	std::vector<Term> reactants, products;
	// kJ per reaction as written, everything in its standard state: from
	// the formation enthalpies (sprint 79), never given by hand
	double heat;
	double activation; // kJ/mol
	double factor;     // per second
	Energy energy;
	uint32_t flame = 0; // what colour it burns (0xrrggbb), 0: it has no flame
	std::vector<Requirement> needs;
};

/*
	A rock: a named mixture of minerals, by mass. Its properties come from
	what's in it — the first of the mixtures the world is made of.
*/
struct RockKind
{
	struct Part
	{
		size_t substance;
		double share; // of its mass
	};
	std::string id, name, origin; // origin: igneous, metamorphic, sedimentary, sediment, impact
	std::vector<Part> minerals;
};

/*
	The tables, loaded from a directory (elements.json, substances.json,
	reactions.json, rocks.json, thermo.json). Every substance's composition must name known elements,
	its anchors must be in order (triple below critical, the boiling point
	between), and every reaction must balance.
*/
class Matter
{
public:
	static Matter load(const std::string &dir); // throws MatterError

	const std::vector<Element> &elements() const { return m_elements; }
	const std::vector<Substance> &substances() const { return m_substances; }
	const std::vector<Reaction> &reactions() const { return m_reactions; }
	const std::vector<RockKind> &rocks() const { return m_rocks; }
	size_t index(const std::string &substance_id) const; // throws MatterError
	const Substance &substance(const std::string &id) const; // throws MatterError
	const Element &element(const std::string &symbol) const; // throws MatterError

	// The raw JSON of each, for whoever shows them (colours, structures, notes).
	const Json::Value &raw() const { return m_raw; }

private:
	std::vector<Element> m_elements;
	std::vector<Substance> m_substances;
	std::vector<Reaction> m_reactions;
	std::vector<RockKind> m_rocks;
	Json::Value m_raw;
};

} // namespace matter
