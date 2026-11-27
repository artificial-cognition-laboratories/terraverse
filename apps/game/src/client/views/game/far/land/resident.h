// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "src/shared/base/irrlichttypes_bloated.h"
#include <IIndexBuffer.h>
#include <IMeshBuffer.h>
#include <IVertexBuffer.h>
#include <S3DVertex.h>
#include <SMaterial.h>
#include <vector>

namespace far {

/*
	A far tile's mesh, as Irrlicht draws it, that can let go of its vertices
	and indices once they're on the graphics card (release()): a tile is drawn
	from its buffers there, so the copy in RAM was only kept in case it was
	repainted (a Planetarium overlay's tint). Released, it keeps its counts
	(the draw reads them) and drops its data. Irrlicht's own buffers are final
	and keep their data for good, so these are ours.

	Released, nothing may make the driver upload it again: it must never be
	marked dirty, and its owner touches it often enough that the driver never
	lets it expire (Tiles::touch). Reading a vertex of a released buffer throws.
*/
// A tile's vertex as it's built (far/mesh.h: its texture coordinates the
// height over the sea and its place in the tile's grid), and as it's kept and
// drawn: packed, 24 bytes to 44 (no normal: the shaders find a face's own).
using Vertex = video::S3DVertex2TCoords;
using PackedVertex = video::S3DVertexPacked;
// A built vertex packed: its height to whole blocks, its place in the grid to half floats.
PackedVertex packed(const Vertex &v);

class ResidentVertices final : public scene::IVertexBuffer
{
public:
	std::vector<PackedVertex> Data;

	HWBuffer::Type getBufferType() const override { return HWBuffer::Type::VERTEX; }
	const void *getData() const override { return Data.data(); } // (none once released: the draw reads the card's)
	void *getData() override { return Data.data(); }
	u32 getCount() const override { return m_released ? m_count : (u32)Data.size(); }
	video::E_VERTEX_TYPE getType() const override { return PackedVertex::getType(); }
	u32 getElementSize() const override { return sizeof(PackedVertex); }
	const core::vector3df &getPosition(u32 i) const override { return at(i).Pos; }
	core::vector3df &getPosition(u32 i) override { return at(i).Pos; }
	// a packed vertex has neither, as Irrlicht's vectors: asking throws
	const core::vector3df &getNormal(u32 i) const override;
	core::vector3df &getNormal(u32 i) override;
	const core::vector2df &getTCoords(u32 i) const override;
	core::vector2df &getTCoords(u32 i) override;
	const scene::WeightBuffer *getWeightBuffer() const override { return nullptr; }
	void useSwSkinning() override;

	// The vertices let go of: the card's copy is the only one now.
	void release();
	bool released() const { return m_released; }

private:
	const PackedVertex &at(u32 i) const;
	PackedVertex &at(u32 i);

	u32 m_count = 0;
	bool m_released = false;
};

class ResidentIndices final : public scene::IIndexBuffer
{
public:
	std::vector<u16> Data;

	HWBuffer::Type getBufferType() const override { return HWBuffer::Type::INDEX; }
	video::E_INDEX_TYPE getType() const override { return video::EIT_16BIT; }
	const void *getData() const override { return Data.data(); }
	void *getData() override { return Data.data(); }
	u32 getElementSize() const override { return sizeof(u16); }
	u32 getCount() const override { return m_released ? m_count : (u32)Data.size(); }

	void release();
	bool released() const { return m_released; }

private:
	u32 m_count = 0;
	bool m_released = false;
};

class TileBuffer final : public scene::IMeshBuffer
{
public:
	TileBuffer();
	~TileBuffer() override;
	TileBuffer(const TileBuffer &) = delete;
	TileBuffer &operator=(const TileBuffer &) = delete;

	// Shared with a quarter's buffer (Tiles::quarter): grabbed, dropped with this.
	ResidentVertices *Vertices;
	ResidentIndices *Indices;

	video::SMaterial &getMaterial() override { return m_material; }
	const video::SMaterial &getMaterial() const override { return m_material; }
	const scene::IVertexBuffer *getVertexBuffer() const override { return Vertices; }
	scene::IVertexBuffer *getVertexBuffer() override { return Vertices; }
	const scene::IIndexBuffer *getIndexBuffer() const override { return Indices; }
	scene::IIndexBuffer *getIndexBuffer() override { return Indices; }
	const core::aabbox3df &getBoundingBox() const override { return m_box; }
	void setBoundingBox(const core::aabbox3df &box) override { m_box = box; }
	void recalculateBoundingBox() override;
	void append(const void *const vertices, u32 numVertices, const u16 *const indices, u32 numIndices) override;
	void setPrimitiveType(scene::E_PRIMITIVE_TYPE type) override { m_primitive = type; }
	scene::E_PRIMITIVE_TYPE getPrimitiveType() const override { return m_primitive; }

	// Its vertices (unless shared and kept by another) and indices let go of.
	void release();
	// Let go of: on the card only.
	bool released() const { return Indices->released(); }

private:
	video::SMaterial m_material;
	core::aabbox3df m_box{ v3f() };
	scene::E_PRIMITIVE_TYPE m_primitive = scene::EPT_TRIANGLES;
};

} // namespace far
