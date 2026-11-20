// Prints every soil point terragen defines (index sand silt clay organic), for
// the palette script: the grid lives in one place, src/terragen/soil.cpp.
#include "library/world/terragen/src/world.h"
#include <cstdio>
using namespace terragen;
int main()
{
	for (int32_t i = 0; i < SOIL_POINTS; i++) {
		Soil s = soilPoint((uint8_t)i);
		std::printf("%d %f %f %f %f\n", i, s.sand / 65536.0, s.silt / 65536.0, s.clay / 65536.0, s.organic / 65536.0);
	}
}
