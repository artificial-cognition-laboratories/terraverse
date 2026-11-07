-- What things are made of, as making sees them: its matter, and the properties
-- processes work on (never its name). `hardness` is Mohs: a blade cuts only
-- what's softer than itself. `fractures`: breaks clean along a curve, so it
-- knaps. `fibrous`: has a grain to carve with. `flexible`: twists into cord.
-- `density`: kg/m³ of a piece of it, TEMP(2026-11-06) until a material's
-- matter can be asked (granite is a rock, not a substance). `tint` multiplies a form's greyscale icon; `tile` is how it looks lying about.
return {
	flint = { name = "Flint", matter = "silica", density = 2600, edge = 1, tint = "#5a554e", tile = "tf_matter_substance.png^[multiply:#4a4640", hardness = 7, fractures = true },
	stone = { name = "Stone", matter = "granite", density = 2700, edge = 0.6, tint = "#9a9892", tile = "tf_goodvibes_stone.png", hardness = 6 },
	wood = { name = "Wood", matter = "wood", density = 650, tint = "#b48a52", tile = "tf_goodvibes_oak_log.png", hardness = 2, fibrous = true },
	copper = { name = "Copper", matter = "copper", density = 8960, edge = 1.3, tint = "#c8774a", hardness = 3, ductile = true },
	bronze = { name = "Bronze", matter = "copper", density = 8800, edge = 1.7, tint = "#b8903e", hardness = 3.5, ductile = true },
	iron = { name = "Iron", matter = "iron", density = 7870, edge = 2.2, tint = "#8e9297", hardness = 4.5, ductile = true },
	fibre = { name = "Plant fibre", matter = "wood", density = 400, tint = "#a6b55e", hardness = 1, fibrous = true, flexible = true },
}
