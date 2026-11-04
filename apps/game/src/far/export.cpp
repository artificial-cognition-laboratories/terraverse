// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "far/export.h"
#include "far/baked.h"
#include "client/renderingengine.h"
#include "client/texturesource.h"
#include "gameparams.h"
#include "exceptions.h"
#include "filesys.h"
#include "client/inputhandler.h"
#include "log.h"
#include "nodedef.h"
#include "script/lua_api/l_matter.h"
#include "server.h"
#include "settings.h"
#include "util/string.h"
#include <IFileSystem.h>
#include <IVideoDriver.h>
#include "far/landbake.h"
#include "pile/piles.h"
#include "workshop/motion.h"
#include "workshop/showcase.h"
#include <fstream>
#include <memory>

namespace far {

void exportPalette(const GameParams &params, const std::string &path)
{
	// no window: the textures are composed on the CPU, as a client composes them
	g_settings->set("video_driver", "null");
	MyEventReceiver receiver;
	RenderingEngine engine(&receiver);

	Server server(params.world_path, params.game_spec, false, UDPSocket::Create(Address(127, 0, 0, 1, 0)), false);
	server.init();
	const std::optional<Horizon> horizon = server.horizon();
	if (!horizon)
		throw BaseException("export palette: the world isn't a terra world");

	std::unique_ptr<IWritableTextureSource> tsrc(createTextureSource());
	io::IFileSystem *fs = engine.get_filesystem();
	video::IVideoDriver *driver = engine.get_video_driver();
	for (const auto &[name, file] : server.mediaPaths()) {
		if (!str_ends_with(name, ".png") && !str_ends_with(name, ".jpg") && !str_ends_with(name, ".tga"))
			continue;
		video::IImage *img = driver->createImageFromFile(file.c_str());
		if (!img)
			throw BaseException("export palette: can't read the image " + file);
		tsrc->insertSourceImage(name, img);
		img->drop();
	}

	const NodeDefManager *ndef = server.getNodeDefManager();
	const Palette palette = Palette::live(ndef, tsrc.get(), blocksIn(horizon->content));
	Json::Value json = Baked::write(horizon->content, palette,
			[&](content_t id) { return ndef->get(id).name; });

	json["scene"] = ModApiMatter::baked(ndef);
	// what a course needs of a block beyond its looks: how tyres grip it, how it gives
	for (Json::Value &b : json["scene"]) {
		content_t id;
		if (!ndef->getId(b["node"].asString(), id))
			throw BaseException("export palette: no block " + b["node"].asString());
		if (const workshop::Material *m = server.getMaterials().get(id))
			b["friction"] = m->friction;
		if (const std::optional<pile::Material> p = pile::materialOf(ndef->get(id))) {
			b["pile"].append(p->repose);
			b["pile"].append(p->yield);
			b["pile"].append(p->bearing);
			b["pile"].append(p->give);
		}
	}
	json["river"] = far::bakeRiverPatch();
	json["machines"] = workshop::bakeShowcase(&server, server.getMaterials());

	Json::StreamWriterBuilder writer;
	writer["indentation"] = "\t";
	writer["precision"] = 17; // exact: the niches are doubles
	const std::string text = Json::writeString(writer, json) + "\n";
	if (!fs::safeWriteToFile(path, text))
		throw BaseException("export palette: can't write " + path);
	actionstream << "export palette: " << palette.looks().size() << " blocks, "
			<< horizon->content.species.size() << " species to " << path << std::endl;
}

} // namespace far
