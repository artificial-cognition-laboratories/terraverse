// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/render/map/mesh/near_buffer.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/util/math/half.h"
#include <algorithm>
#include <cmath>

namespace {

bool exact(const video::S3DVertex &v);
video::S3DVertexNear packed(const video::S3DVertex &v);
u32 snorm10(float x);

} // namespace

void NearVertices::useSwSkinning()
{
	throw BaseException("map: a block's mesh has no skin to skin");
}

NearVertices::NearVertices(std::vector<video::S3DVertex> vertices, bool packable)
{
	if (packable && std::all_of(vertices.begin(), vertices.end(), exact)) {
		m_near.reserve(vertices.size());
		for (const video::S3DVertex &v : vertices)
			m_near.push_back(packed(v));
		m_packed = true;
		return;
	}
	m_standard = std::move(vertices);
}

const void *NearVertices::getData() const
{
	return m_packed ? (const void *)m_near.data() : (const void *)m_standard.data();
}

void *NearVertices::getData()
{
	return m_packed ? (void *)m_near.data() : (void *)m_standard.data();
}

u32 NearVertices::getCount() const
{
	if (m_released)
		return m_count;
	return (u32)(m_packed ? m_near.size() : m_standard.size());
}

const core::vector3df &NearVertices::getPosition(u32 i) const
{
	live();
	return m_packed ? m_near.at(i).Pos : m_standard.at(i).Pos;
}

core::vector3df &NearVertices::getPosition(u32 i)
{
	live();
	return m_packed ? m_near.at(i).Pos : m_standard.at(i).Pos;
}

void NearVertices::release()
{
	if (m_released)
		return;
	m_count = getCount();
	std::vector<video::S3DVertex>().swap(m_standard); // (clear() keeps the capacity: the memory with it)
	std::vector<video::S3DVertexNear>().swap(m_near);
	m_released = true;
}

size_t NearVertices::ram() const
{
	return m_standard.size() * sizeof(video::S3DVertex) + m_near.size() * sizeof(video::S3DVertexNear);
}

void NearVertices::live() const
{
	if (m_released)
		throw BaseException("map: a block's vertex read after its mesh went to the card alone");
}

const video::S3DVertex &NearVertices::standard(u32 i) const
{
	live();
	if (m_packed)
		throw BaseException("map: a packed block vertex's normal and texture coordinates aren't Irrlicht's vectors");
	return m_standard.at(i);
}

video::S3DVertex &NearVertices::standard(u32 i)
{
	live();
	if (m_packed)
		throw BaseException("map: a packed block vertex's normal and texture coordinates aren't Irrlicht's vectors");
	return m_standard.at(i);
}

void NearIndices::release()
{
	if (m_released)
		return;
	m_count = (u32)Data.size();
	std::vector<u16>().swap(Data);
	m_released = true;
}

NearBuffer::NearBuffer(std::vector<video::S3DVertex> vertices, std::vector<u16> indices, Pack pack) :
	m_vertices(nullptr), m_indices(new NearIndices())
{
	// its bounds while they're floats
	if (!vertices.empty()) {
		m_box.reset(vertices.front().Pos);
		for (const video::S3DVertex &v : vertices)
			m_box.addInternalPoint(v.Pos);
	}
	m_vertices = new NearVertices(std::move(vertices), pack == Pack::IfExact);
	m_indices->Data = std::move(indices);
}

NearBuffer::~NearBuffer()
{
	m_vertices->drop();
	m_indices->drop();
}

void NearBuffer::recalculateBoundingBox()
{
	const u32 n = m_vertices->getCount();
	if (n == 0) {
		m_box.reset(0, 0, 0);
		return;
	}
	m_box.reset(m_vertices->getPosition(0)); // (throws once released)
	for (u32 i = 1; i < n; i++)
		m_box.addInternalPoint(m_vertices->getPosition(i));
}

void NearBuffer::append(const void *const, u32, const u16 *const, u32)
{
	throw BaseException("map: a block's opaque mesh is made whole, never appended to");
}

void NearBuffer::release()
{
	if (!held())
		return;
	m_vertices->release();
	m_indices->release();
}

size_t NearBuffer::ram() const
{
	return m_vertices->ram() + m_indices->Data.size() * sizeof(u16);
}

size_t NearBuffer::bytes() const
{
	return (size_t)m_vertices->getCount() * m_vertices->getElementSize() + (size_t)m_indices->getCount() * sizeof(u16);
}

namespace {

// Packing it changes nothing the shader reads: its normal of length one or
// none (a small plant's is longer: its height), its texture coordinates where
// a half is within an eighth of a 16-pixel texture's pixel (to 16 tiles).
bool exact(const video::S3DVertex &v)
{
	const float length = v.Normal.getLength();
	if (length > 1e-6f && std::abs(length - 1.f) > 1e-3f)
		return false;
	return std::abs(v.TCoords.X) <= 16.f && std::abs(v.TCoords.Y) <= 16.f;
}

video::S3DVertexNear packed(const video::S3DVertex &v)
{
	video::S3DVertexNear out;
	out.Pos = v.Pos;
	out.Normal = snorm10(v.Normal.X) | (snorm10(v.Normal.Y) << 10) | (snorm10(v.Normal.Z) << 20);
	out.Color = v.Color;
	out.TCoords[0] = toHalf(v.TCoords.X);
	out.TCoords[1] = toHalf(v.TCoords.Y);
	out.Aux = v.Aux;
	out.Pad = 0;
	return out;
}

// -1..1 as a signed normalized 10-bit field (two's complement)
u32 snorm10(float x)
{
	const s32 q = (s32)std::lround(std::clamp(x, -1.f, 1.f) * 511.f);
	return (u32)q & 0x3ff;
}

} // namespace
