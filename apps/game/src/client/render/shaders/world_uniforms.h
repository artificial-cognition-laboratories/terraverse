// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/client/render/shaders/shader.h"
#include <memory>
#include <string>
#include <vector>

class Client;
class Sky;
class GameGlobalShaderUniformSetter;

// What every world shader reads each frame: light, fog, the camera, the sun and moon (from the sky,
// once there is one). Game's, and a body's eye's (sprint 126).
class GameGlobalShaderUniformSetterFactory : public IShaderUniformSetterFactory
{
public:
	// `crack_length`: the frames of the crack texture (crack_anylength.png), as the client counted them.
	GameGlobalShaderUniformSetterFactory(Client *client, int crack_length);
	void setSky(Sky *sky);
	IShaderUniformSetter *create(const std::string &name) override;

private:
	Sky *m_sky = nullptr;
	Client *m_client;
	int m_crack_length;
	std::vector<GameGlobalShaderUniformSetter *> created_nosky;
};

// The node shaders' compile-time constants: material types, waving.
std::unique_ptr<IShaderConstantSetter> createNodeShaderConstantSetter();
