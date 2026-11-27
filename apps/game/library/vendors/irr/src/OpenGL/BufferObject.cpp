// Copyright (C) 2024 sfan5
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "BufferObject.h"

#include <cassert>
#include <mt_opengl.h>

namespace video
{

void OGLBufferObject::upload(const void *data, size_t size, size_t offset,
		GLenum usage, bool mustShrink)
{
	bool newBuffer = false;
	assert(!(mustShrink && offset > 0)); // forbidden usage
	// Terraverse: immutable storage for what's static (BufferObject.h); written again, made anew
	const bool immutable = usage == GL_STATIC_DRAW && GL.BufferStorage && offset == 0;
	if (m_name && m_immutable) {
		assert(offset == 0); // (an immutable buffer can't be written in part)
		destroy();
	}
	if (!m_name) {
		GL.GenBuffers(1, &m_name);
		if (!m_name)
			return;
		newBuffer = true;
	} else if (size > m_size || mustShrink) {
		newBuffer = size != m_size;
	}

	GL.BindBuffer(m_target, m_name);

	if (newBuffer && immutable) {
		GL.BufferStorage(m_target, size, data, 0);
		m_size = size;
		m_immutable = true;
	} else if (newBuffer) {
		assert(offset == 0);
		GL.BufferData(m_target, size, data, usage);
		m_size = size;
		m_immutable = false;
	} else {
		GL.BufferSubData(m_target, offset, size, data);
	}

	GL.BindBuffer(m_target, 0);
}

void OGLBufferObject::destroy()
{
	if (m_name)
		GL.DeleteBuffers(1, &m_name);
	m_name = 0;
	m_size = 0;
	m_immutable = false;
}

}
