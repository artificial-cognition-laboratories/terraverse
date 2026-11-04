// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "release.h"
#include "config.h"
#include "exceptions.h"
#include "filesys.h"
#include "log.h"
#include "settings.h"
#include "util/base64.h"
#include <json/json.h>
#include <memory>
#include <sstream>
#if USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#endif

namespace release {

const char *const UPDATE_KEY = R"(-----BEGIN PUBLIC KEY-----
MCowBQYDK2VwAyEATYuT3HsECn8T2zFIjLdBWF8O7kNFmAchF2nZLnY3EK4=
-----END PUBLIC KEY-----
)";

std::optional<std::vector<int>> parse(const std::string &version)
{
	std::string text = version;
	if (!text.empty() && text[0] == 'v')
		text.erase(0, 1);
	std::vector<int> out;
	std::stringstream parts(text);
	std::string part;
	while (std::getline(parts, part, '.')) {
		if (part.empty() || part.size() > 6 || part.find_first_not_of("0123456789") != std::string::npos)
			return std::nullopt;
		out.push_back(std::stoi(part));
	}
	if (out.empty())
		return std::nullopt;
	return out;
}

bool verify(const std::string &bytes, const std::string &signature, const std::string &key)
{
#if USE_OPENSSL
	if (!base64_is_valid(signature))
		return false;
	const std::string raw = base64_decode(signature);
	std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new_mem_buf(key.data(), (int)key.size()), BIO_free);
	if (!bio)
		return false;
	std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(
			PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
	std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
	if (!pkey || !ctx || EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, pkey.get()) != 1)
		return false;
	return EVP_DigestVerify(ctx.get(), (const unsigned char *)raw.data(), raw.size(),
			(const unsigned char *)bytes.data(), bytes.size()) == 1;
#else
	throw BaseException("release: signature checks need a build with OpenSSL");
#endif
}

std::optional<std::string> latest(const std::string &manifest, const std::string &signature, const std::string &key)
{
	if (!verify(manifest, signature, key))
		return std::nullopt;
	Json::Value root;
	std::string errors;
	std::istringstream in(manifest);
	if (!Json::parseFromStream(Json::CharReaderBuilder(), in, &root, &errors)
			|| !root.isObject() || !root["version"].isString() || !parse(root["version"].asString()))
		return std::nullopt;
	return root["version"].asString();
}

void openWorld(const std::string &world, const std::string &build, const std::string &backups)
{
	const auto ours = parse(build);
	if (!ours)
		return;
	const std::string conf = world + DIR_DELIM "world.mt";
	Settings settings;
	if (!settings.readConfigFile(conf.c_str()))
		throw BaseException("release: could not read " + conf);

	const std::string saved = settings.exists("tf_saved_by") ? settings.get("tf_saved_by") : "";
	if (saved == build)
		return;
	const auto theirs = parse(saved);
	if (theirs && *theirs > *ours)
		throw BaseException("This world was last saved by Terraverse " + saved + ", which is newer than this "
			"version (" + build + "). Run terraverse update to open it.");

	// Nothing to lose in a world that has never been played.
	if (fs::PathExists(world + DIR_DELIM "map.sqlite") || fs::PathExists(world + DIR_DELIM "env_meta.txt")) {
		const std::string target = backups + DIR_DELIM + fs::GetFilenameFromPath(world.c_str()) + "-"
			+ (saved.empty() ? "before-" + build : saved);
		if (!fs::PathExists(target)) {
			if (!fs::CreateAllDirs(backups) || !fs::CopyDir(world, target))
				throw BaseException("Could not back this world up to " + target + " before opening it in "
					"a new version, so it was left as it is.");
			actionstream << "release: backed up " << world << " to " << target << std::endl;
		}
	}
	settings.set("tf_saved_by", build);
	if (!settings.updateConfigFile(conf.c_str()))
		throw BaseException("release: could not save " + conf);
}

} // namespace release
