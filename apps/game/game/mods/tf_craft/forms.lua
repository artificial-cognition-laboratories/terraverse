-- Hand-sized forms, in the order they're offered. A form is found
-- (`materials`: what it comes in) or made (`made`: the ways to make it, each
-- by a process from other forms; it takes the first input's material, so a
-- form comes in every material one of its ways can work). `gives`: what
-- holding it lets you do, for processes that use it. `name`: %s is the
-- material, where a form comes in more than one. `station`: it's placed, and
-- is a station of that kind. `lies`: it's found lying on the ground, this box
-- of a node, and can be put down again. `digs`: what it breaks, as Luanti's
-- group times (s, for levels 1-3) at edge 1 (flint), quicker in a better
-- edge. `litres`: a found form's size, so its mass; a made one weighs what
-- went into it.
local HEAD = function(count)
	return { { by = "knap", from = { { "stone", 1 } }, count = count }, { by = "forge", from = { { "ingot", 1 } }, count = count } }
end
local HAFTED = function(head)
	return { { by = "haft", from = { { head, 1 }, { "handle", 1 }, { "cord", 1 } } } }
end
return {
	{ id = "stone", name = "%s", materials = { "flint", "stone" }, gives = { "strike" }, stack = 16, litres = 0.3,
		lies = { -0.16, -0.5, -0.12, 0.14, -0.36, 0.13 } },
	{ id = "stick", name = "Stick", materials = { "wood" }, stack = 32, litres = 0.25,
		lies = { -0.42, -0.5, -0.03, 0.4, -0.44, 0.03 } },
	{ id = "fibre", name = "Plant fibre", materials = { "fibre" }, stack = 64, litres = 0.05 },
	-- TEMP(2026-11-06): found, in creative, until ore's smelted
	{ id = "ingot", name = "%s ingot", materials = { "copper", "bronze", "iron" }, stack = 16, litres = 0.12 },
	{ id = "flake", name = "%s flake", gives = { "cut" }, stack = 16,
		made = { { by = "knap", from = { { "stone", 1 } }, count = 2 } } },
	{ id = "axe_head", name = "%s axe head", stack = 4, made = HEAD() },
	{ id = "pick_head", name = "%s pick head", stack = 4, made = HEAD() },
	{ id = "shovel_head", name = "%s shovel blade", stack = 4, made = HEAD() },
	{ id = "blade", name = "%s blade", gives = { "cut" }, stack = 4, made = HEAD() },
	{ id = "handle", name = "Handle", stack = 8,
		made = { { by = "carve", from = { { "stick", 1 } } } } },
	{ id = "cord", name = "Cord", stack = 32,
		made = { { by = "twist", from = { { "fibre", 3 } } } } },
	{ id = "axe", name = "%s axe", gives = { "cut", "chop" }, stack = 1, uses = 120,
		digs = { choppy = { 3, 1.6, 0.8 }, snappy = { 1, 0.5, 0.3 } }, made = HAFTED("axe_head") },
	{ id = "pick", name = "%s pick", gives = { "strike" }, stack = 1, uses = 120,
		digs = { cracky = { 4, 2, 1 } }, made = HAFTED("pick_head") },
	{ id = "shovel", name = "%s shovel", stack = 1, uses = 150,
		digs = { crumbly = { 1.2, 0.6, 0.3 } }, made = HAFTED("shovel_head") },
	{ id = "sword", name = "%s sword", gives = { "cut" }, stack = 1, uses = 100, hits = 5,
		digs = { snappy = { 0.8, 0.4, 0.2 } }, made = HAFTED("blade") },
	{ id = "bench", name = "Workbench", station = { name = "Workbench", kind = "bench", quicker = 0.5 }, stack = 1,
		made = { { by = "lash", from = { { "stick", 8 }, { "cord", 2 } } } } },
}
