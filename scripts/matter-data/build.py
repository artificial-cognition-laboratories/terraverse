#!/usr/bin/env python3
"""
Bake the matter tables' thermodynamics (sprint 79): assets/matter/thermo.json.

Offline only; the game reads what this writes. For each substance in
substances.json:
  formation   standard enthalpy of formation at 298.15 K, kJ/mol, in the
              state it's in there (NASA 7-term polynomials, as Cantera ships
              them; else the literature table below, each with its source)
  saturation  for the fluids CoolProp models: its vapour pressure and the
              two densities from the triple point to near the critical
              point, so boiling follows the real curve, not two anchors

Reaction heats are computed from `formation` when the tables load
(src/matter), so a new reaction is as consistent as the data under it.

Needs CoolProp and Cantera: python3 -m venv v && v/bin/pip install CoolProp cantera
Run: v/bin/python scripts/matter-data/build.py
"""
import json, math, os, sys

import cantera as ct
import CoolProp.CoolProp as CP

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
MATTER = os.path.join(ROOT, "assets", "matter")

# our id -> the NASA species in its 298 K state (Cantera's nasa_gas/nasa_condensed)
NASA = {
    "water": "H2O(L)", "carbon_dioxide": "CO2", "methane": "CH4", "ammonia": "NH3",
    "sulfur_dioxide": "SO2",
    "sulfuric_acid": "H2SO4(L)",     "salt": "NaCL(s)",
    "silica": "SiO2(Lqz)", "carbon_monoxide": "CO", "hematite": "Fe2O3(s)", "copper_oxide": "CuO(s)",
    "calcite": "CaCO3(caL)", "lime": "CaO(s)", "magnetite": "Fe3O4(s)", "pyrite": "FeS2(s)",
    "soda_ash": "Na2CO3(I)", "slaked_lime": "CaO2H2(s)",
    "hydrogen_chloride": "HCL",
    "sodium_hydroxide": "NaOH(a)", "potassium_hydroxide": "KOH(a)", "alumina": "AL2O3(a)",
    "aluminium_chloride": "ALCL3(s)", "iron_chloride": "FeCL2(s)",     "propane": "C3H8", "nitrogen_dioxide": "NO2",
    "potassium_carbonate": "K2CO3(s)", "potassium_sulfide": "K2S(1)",
    "sodium_sulfate": "Na2SO4(V)", "hydrogen_peroxide": "H2O2(L)",     "galena": "PbS(s)", "lead_oxide": "PbO(rd)",
        "magnesia": "MgO(s)", "olivine": "Mg2SiO4(s)", "pyroxene": "MgSiO3(I)",
}

# what NASA's tables don't have, kJ/mol at 298 K (and where it's from)
LITERATURE = {
    "zinc_oxide": (-350.5, "NIST WebBook, ZnO (cr)"),
    "zinc_chloride": (-415.1, "NIST WebBook, ZnCl2 (cr)"),
    "cassiterite": (-577.6, "Robie & Hemingway 1995, SnO2"),
    "cobalt_oxide": (-237.9, "NIST WebBook, CoO (cr)"),
    "manganese_oxide": (-520.0, "NIST WebBook, MnO2 (cr)"),
    "nitric_acid": (-174.1, "NIST WebBook, HNO3 (l)"),
    "potassium_nitrate": (-494.6, "NIST WebBook, KNO3 (cr)"),
    "glucose": (-1273.3, "NIST WebBook, D-glucose (cr)"),
    "wood": (-963.0, "cellulose per C6H10O5 unit (Kroll et al.)"),
    "rubber": (-35.0, "polyisoprene per C5H8 unit, approx."),
    "feldspar": (-3968.0, "Robie & Hemingway 1995, microcline"),
    "plagioclase": (-3935.0, "Robie & Hemingway 1995, low albite"),
    "mica": (-5976.0, "Robie & Hemingway 1995, muscovite"),
    "kaolinite": (-4120.0, "Robie & Hemingway 1995"),
    "metakaolin": (-3211.0, "approx., kaolinite less its water"),
    "malachite": (-1051.0, "Robie & Hemingway 1995"),
    "chalcopyrite": (-190.0, "Robie & Hemingway 1995"),
    "ilmenite": (-1232.0, "Robie & Hemingway 1995, FeTiO3"),
    "apatite": (-6872.0, "Robie & Hemingway 1995, fluorapatite"),
    "glass": (-1561.0, "NIST, Na2SiO3 (cr)"),
    "ethanol": (-277.6, "NIST WebBook, ethanol (l)"),
    "octane": (-250.1, "NIST WebBook, n-octane (l)"),
    # alloys: as their metals, give or take a little mixing heat
    "bronze": (-5.0, "alloy, approx."), "cast_iron": (-20.0, "alloy, approx."),
    "steel": (-10.0, "alloy, approx."), "pewter": (-2.0, "alloy, approx."),
}

# our id -> CoolProp's fluid
COOLPROP = {
    "water": "Water", "carbon_dioxide": "CarbonDioxide", "methane": "Methane", "ammonia": "Ammonia",
    "nitrogen": "Nitrogen", "oxygen": "Oxygen", "hydrogen": "Hydrogen", "sulfur_dioxide": "SulfurDioxide",
    "carbon_monoxide": "CarbonMonoxide", "propane": "Propane", "ethanol": "Ethanol",
    "octane": "n-Octane", "hydrogen_chloride": "HydrogenChloride",
}
POINTS = 24


def nasa_species():
    out = {}
    for f in ("nasa_gas.yaml", "nasa_condensed.yaml"):
        for s in ct.Species.list_from_file(f):
            out[s.name] = s
    return out


def formation(sid, species, kind):
    # an element as it is at 298 K and 1 atm (iron, O2, Cl2) is where
    # formation enthalpies are measured from
    if kind == "element":
        return 0.0, "an element in its standard state"
    if sid in NASA:
        s = species.get(NASA[sid])
        if s is None:
            sys.exit(f"{sid}: no NASA species {NASA[sid]}")
        return round(s.thermo.h(298.15) / 1e6, 2), f"NASA polynomials ({NASA[sid]})"
    if sid in LITERATURE:
        return LITERATURE[sid]
    sys.exit(f"{sid}: no formation enthalpy: add it to NASA or LITERATURE")


def saturation(fluid):
    # from just above the triple point to just short of the critical point,
    # spaced evenly in 1/T (vapour pressure is near straight in it)
    tt, tc = CP.PropsSI("Ttriple", fluid), CP.PropsSI("Tcrit", fluid)
    lo, hi = max(tt, CP.PropsSI("Tmin", fluid)) + 0.01, tc * 0.995
    ts = [1 / (1 / lo + (1 / hi - 1 / lo) * i / (POINTS - 1)) for i in range(POINTS)]
    rows = {"t": [], "p": [], "liquid": [], "gas": []}
    for t in ts:
        rows["t"].append(round(t, 3))
        rows["p"].append(float(f"{CP.PropsSI('P', 'T', t, 'Q', 0, fluid):.6g}"))
        rows["liquid"].append(round(CP.PropsSI("D", "T", t, "Q", 0, fluid), 3))
        rows["gas"].append(float(f"{CP.PropsSI('D', 'T', t, 'Q', 1, fluid):.6g}"))
    return rows


def main():
    substances = json.load(open(os.path.join(MATTER, "substances.json")))["substances"]
    species = nasa_species()
    out = {}
    for s in substances:
        sid = s["id"]
        h, source = formation(sid, species, s["kind"])
        entry = {"formation": h, "source": source}
        if sid in COOLPROP:
            entry["saturation"] = saturation(COOLPROP[sid])
            entry["saturation_source"] = f"CoolProp {CP.get_global_param_string('version')} ({COOLPROP[sid]})"
        out[sid] = entry
    with open(os.path.join(MATTER, "thermo.json"), "w") as f:
        json.dump({"note": "Written by scripts/matter-data/build.py: don't edit by hand.", "substances": out}, f, indent=1)
        f.write("\n")
    print(f"thermo.json: {len(out)} substances, {sum('saturation' in e for e in out.values())} with saturation curves")


if __name__ == "__main__":
    main()
