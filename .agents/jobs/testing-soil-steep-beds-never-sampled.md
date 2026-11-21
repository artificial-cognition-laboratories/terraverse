# SoilByWater never checks steep beds against gentle ones

`terragen.soil.slow` (`apps/game/library/world/terragen/tests/soil.cpp`, `TEST_CASE("SoilByWater")`)
claims three things; the third, "stones in steep beds > in gentle beds", is behind
`if (steep > 5 && gentle > 5)`, and on TF24.dGVycmE at Earth size it samples **0 steep beds**
(of 2253 bed samples), so it never runs. Found by the testing owner, 2026-11-20.

Reproduce:

    bash apps/game/scripts/build.sh --target terragen_tests -j
    TEST_NOTES=1 .output/tests/terragen_tests "--source-file=*soil.cpp" "--test-suite=slow"
    # soil by water: ... stones in steep beds 0 (0), gentle 0.0976 (2217)

Either the windows round the 100 biggest rivers never reach a bed steeper than 3/1000 (pick
headwater rivers too, or lower the threshold to what big rivers actually have), or no river
slope is above it at all, which would itself be a bug. Then make the check unconditional.

## Agent notes (2026-11-20, jobs bot)

Blocked for tonight: `tests/soil.cpp` and the terragen grid sources it samples (drainage, geology,
grid, history) are modified and uncommitted in the developer's tree (sprint 120). Changing the test
now would mix into that work. Pick up once those are committed.
