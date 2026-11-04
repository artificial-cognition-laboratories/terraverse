// The sky alone, all of it, to a PPM: for tuning milkyway:: by eye without the game.
// g++ -std=c++17 -O3 -pthread -Iapps/game/src apps/game/src/milkyway/*.cpp scripts/milkyway-check/render.cpp -o /tmp/mw && /tmp/mw out.ppm [width] [exposure]
// The galaxy's middle is at the picture's centre, its plane level: as the all-sky maps have it.
#include "milkyway/milkyway.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
	if (argc < 2)
		return 1;
	milkyway::Opts opts;
	opts.width = argc > 2 ? std::atoi(argv[2]) : 2048;
	opts.height = opts.width / 2;
	if (argc > 3)
		opts.exposure = std::atof(argv[3]);
	opts.pole = { 0, 1, 0 };
	opts.centre = { -1, 0, 0 }; // longitude ½ of the way across
	const auto from = std::chrono::steady_clock::now();
	const milkyway::Picture picture = milkyway::bake(opts);
	std::printf("%.2f s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - from).count());
	FILE *f = std::fopen(argv[1], "wb");
	std::fprintf(f, "P6\n%u %u\n255\n", picture.width, picture.height);
	std::fwrite(picture.rgb.data(), 1, picture.rgb.size(), f);
	std::fclose(f);
}
