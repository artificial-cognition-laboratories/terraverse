// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/views/game/far/land/resident.h"
#include "src/shared/base/exceptions.h"
#include "src/shared/util/math/half.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace far {

PackedVertex packed(const Vertex &v)
{
	PackedVertex out;
	out.Pos = v.Pos;
	out.Color = v.Color;
	// whole blocks: the haze and the contours read it, a block's half is nothing to them
	const float h = std::round(v.TCoords.X);
	out.TCoords[0] = (s16)std::clamp(h, -32768.f, 32767.f);
	out.TCoords[1] = 0;
	out.TCoords2[0] = toHalf(v.TCoords2.X);
	out.TCoords2[1] = toHalf(v.TCoords2.Y);
	return out;
}

void ResidentVertices::useSwSkinning()
{
	throw BaseException("far: a tile's mesh has no skin to skin");
}

void ResidentVertices::release()
{
	if (m_released)
		return;
	m_count = (u32)Data.size();
	std::vector<PackedVertex>().swap(Data); // (clear() keeps the capacity: the memory with it)
	m_released = true;
}

const core::vector3df &ResidentVertices::getNormal(u32) const
{
	throw BaseException("far: a packed tile vertex has no normal (the shaders find a face's own)");
}

core::vector3df &ResidentVertices::getNormal(u32)
{
	throw BaseException("far: a packed tile vertex has no normal (the shaders find a face's own)");
}

const core::vector2df &ResidentVertices::getTCoords(u32) const
{
	throw BaseException("far: a packed tile vertex's texture coordinates aren't floats");
}

core::vector2df &ResidentVertices::getTCoords(u32)
{
	throw BaseException("far: a packed tile vertex's texture coordinates aren't floats");
}

const PackedVertex &ResidentVertices::at(u32 i) const
{
	if (m_released)
		throw BaseException("far: a tile's vertex read after its mesh went to the card alone");
	return Data.at(i);
}

PackedVertex &ResidentVertices::at(u32 i)
{
	if (m_released)
		throw BaseException("far: a tile's vertex read after its mesh went to the card alone");
	return Data.at(i);
}

void ResidentIndices::release()
{
	if (m_released)
		return;
	m_count = (u32)Data.size();
	std::vector<u16>().swap(Data);
	m_released = true;
}

TileBuffer::TileBuffer() : Vertices(new ResidentVertices()), Indices(new ResidentIndices())
{
}

TileBuffer::~TileBuffer()
{
	Vertices->drop();
	Indices->drop();
}

void TileBuffer::recalculateBoundingBox()
{
	if (Vertices->released())
		throw BaseException("far: a tile's bounds asked after its mesh went to the card alone");
	if (Vertices->Data.empty()) {
		m_box.reset(0, 0, 0);
		return;
	}
	m_box.reset(Vertices->Data.front().Pos);
	for (const PackedVertex &v : Vertices->Data)
		m_box.addInternalPoint(v.Pos);
}

void TileBuffer::append(const void *const, u32, const u16 *const, u32)
{
	throw BaseException("far: a tile's mesh is made whole by its worker, never appended to");
}

void TileBuffer::release()
{
	Vertices->release();
	Indices->release();
}


} // namespace far
