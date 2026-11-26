// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/planetarium/space/picture.h"
#include "src/shared/platform/system/filesys.h"
#include "src/shared/platform/system/porting.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/system/log.h"
#include <chrono>
#include <fstream>

namespace planetarium::picture {

namespace {

constexpr unsigned VERSION = 3; // bump with a change to milkyway:: (the picture's shipped and kept)

// where the galaxy lies in the scene: chosen so the band crosses the main menu's picture (planetarium/shot.h)
const milkyway::Way POLE{ -0.6440f, -0.0970f, -0.7589f }, CENTRE{ -0.7616f, -0.0130f, 0.6480f };

std::string name(Size size);
std::shared_ptr<const milkyway::Picture> read(const std::string &path, Size size);
milkyway::Picture baked(Size size);
void write(const std::string &path, const milkyway::Picture &picture);

} // namespace

std::shared_ptr<const milkyway::Picture> find(Size size)
{
	if (auto shipped = read(porting::path_share + DIR_DELIM + "client" + DIR_DELIM + name(size), size))
		return shipped;
	return read(porting::path_cache + DIR_DELIM + name(size), size);
}

std::shared_ptr<const milkyway::Picture> paint(Size size)
{
	if (auto was = find(size))
		return was;
	auto out = std::make_shared<const milkyway::Picture>(baked(size));
	fs::CreateAllDirs(porting::path_cache);
	const std::string path = porting::path_cache + DIR_DELIM + name(size);
	try {
		write(path, *out);
	} catch (const BaseException &e) {
		warningstream << e.what() << std::endl; // it's only baked again next time
	}
	return out;
}

void ship(const std::string &dir)
{
	const std::string path = dir + DIR_DELIM + name(FULL);
	if (read(path, FULL))
		return;
	fs::CreateAllDirs(dir);
	for (const fs::DirListNode &old : fs::GetDirListing(dir))
		if (!old.dir && old.name.rfind("milkyway_", 0) == 0)
			fs::DeleteSingleFileOrEmptyDirectory(dir + DIR_DELIM + old.name);
	write(path, baked(FULL));
}

namespace {

std::string name(Size size)
{
	return "milkyway_" + std::to_string(VERSION) + "_" + std::to_string(size.width) + ".bin";
}

// the picture this size at `path`, if it's there whole (any other length: an interrupted write)
std::shared_ptr<const milkyway::Picture> read(const std::string &path, Size size)
{
	auto out = std::make_shared<milkyway::Picture>();
	out->width = size.width;
	out->height = size.height;
	out->rgb.resize((size_t)size.width * size.height * 3);
	std::ifstream in(path, std::ios::binary);
	if (in && in.read(reinterpret_cast<char *>(out->rgb.data()), out->rgb.size()) && in.peek() == EOF)
		return out;
	return nullptr;
}

milkyway::Picture baked(Size size)
{
	const auto from = std::chrono::steady_clock::now();
	milkyway::Opts opts;
	opts.width = size.width;
	opts.height = size.height;
	opts.exposure = 0.45f;
	opts.pole = POLE;
	opts.centre = CENTRE;
	milkyway::Picture out = milkyway::bake(opts);
	infostream << "galaxy: baked " << size.width << " wide in "
			<< std::chrono::duration<double>(std::chrono::steady_clock::now() - from).count() << " s" << std::endl;
	return out;
}

void write(const std::string &path, const milkyway::Picture &picture)
{
	const std::string bytes(reinterpret_cast<const char *>(picture.rgb.data()), picture.rgb.size());
	if (!fs::safeWriteToFile(path, bytes))
		throw BaseException("galaxy: couldn't write the picture to " + path);
}

} // namespace

} // namespace planetarium::picture
