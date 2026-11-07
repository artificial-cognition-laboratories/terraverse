-- How a form is made. `needs`: material properties the worked material must
-- have, and the reason shown when it hasn't. `uses`: an affordance something
-- else in reach must give (a striker, a blade); `harder`: that tool must be
-- harder than what it works. `hand`: it can be done without a station, at
-- `slower` times its seconds. `at`: only at a station of this kind.
return {
	knap = { name = "Knap", seconds = 4, hand = true, slower = 1.5,
		needs = { fractures = "only stone that breaks clean, like flint, knaps" }, uses = "strike" },
	carve = { name = "Carve", seconds = 6, hand = true, slower = 2,
		needs = { fibrous = "only something with a grain carves" }, uses = "cut", harder = true },
	twist = { name = "Twist", seconds = 3, hand = true, slower = 1,
		needs = { flexible = "only fibre twists into cord" } },
	haft = { name = "Haft", seconds = 5, hand = true, slower = 2 },
	lash = { name = "Lash", seconds = 8, hand = true, slower = 1 },
	forge = { name = "Forge", seconds = 6, at = "forge",
		needs = { ductile = "only metal that bends hot forges" } },
}
