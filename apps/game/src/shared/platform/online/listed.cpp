// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/shared/platform/online/listed.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/platform/online/ref.h"
#include "src/shared/platform/system/filesys.h"
#include <ctime>
#include <json/json.h>
#include <random>
#ifdef _WIN32
#include <process.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace online {

namespace {

std::string randomSecret();
long ownPid();
// The pid a record at `file` names, or -1 when there's none to read.
long recordedPid(const std::string &file);

} // namespace

std::unique_ptr<Listed> Listed::open(Opts opts)
{
	if (!fs::CreateAllDirs(opts.dir))
		throw BaseException("online: couldn't make " + opts.dir);
#ifndef _WIN32
	// the secrets in it admit whoever reads them: the user's alone (as terra's running/)
	chmod(opts.dir.c_str(), 0700);
#endif
	std::string secret = randomSecret();
	Json::Value record(Json::objectValue);
	record["ref"] = ref::print(ref::Address{ "localhost", opts.port });
	record["title"] = opts.title;
	// a world's own mods aren't read here; terra lists none for a saved world without a server either
	record["mods"] = Json::Value(Json::arrayValue);
	record["port"] = opts.port;
	record["pid"] = (Json::Int64)ownPid();
	record["secret"] = secret;
	record["owner"] = opts.owner;
	record["server"] = opts.world;
	record["started"] = (Json::Int64)std::time(nullptr);
	record["by"] = "game";
	const std::string file = opts.dir + DIR_DELIM + std::to_string(opts.port) + ".json";
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "  ";
	if (!fs::safeWriteToFile(file, Json::writeString(writer, record) + "\n"))
		throw BaseException("online: couldn't write " + file);
#ifndef _WIN32
	chmod(file.c_str(), 0600);
#endif
	return std::unique_ptr<Listed>(new Listed(file, std::move(secret), opts.port));
}

Listed::Listed(std::string file, std::string secret, uint16_t port) :
	m_file(std::move(file)), m_port(port), m_secret(std::move(secret))
{
}

Listed::~Listed()
{
	// only ours: a later run on this port may have replaced it
	if (recordedPid(m_file) == ownPid())
		fs::DeleteSingleFileOrEmptyDirectory(m_file);
}

namespace {

std::string randomSecret()
{
	std::random_device random;
	std::string out;
	for (int i = 0; i < 64; i++)
		out += "0123456789abcdef"[random() & 15];
	return out;
}

long ownPid()
{
#ifdef _WIN32
	return _getpid();
#else
	return getpid();
#endif
}

long recordedPid(const std::string &file)
{
	std::string text;
	if (!fs::ReadFile(file, text))
		return -1;
	Json::Value v;
	std::string errors;
	Json::CharReaderBuilder builder;
	std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	if (!reader->parse(text.data(), text.data() + text.size(), &v, &errors) || !v["pid"].isIntegral())
		return -1;
	return (long)v["pid"].asInt64();
}

} // namespace

} // namespace online
