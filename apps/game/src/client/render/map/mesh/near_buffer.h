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

/*
	An opaque buffer of a block's mesh, as Irrlicht draws it, that can let go
	of its vertices and indices once they're on the graphics card (release()):
	it's drawn from there, and nothing reads it after (animation changes only
	its material; on a planet nothing merges it). Released, it keeps its counts
	(the draw reads them) and drops its data, so it must never be marked dirty,
	and its owner touches it often enough that the driver never lets it expire
	(NearMeshes). Reading a vertex of a released one throws. Irrlicht's own
	buffers are final and keep their data for good, so these are ours, as the
	far view's are (far/land/resident.h).

	Kept packed (video::S3DVertexNear, 28 bytes to 40) when packing changes
	nothing the shader reads: every normal of length one or none (a small
	plant's longer one is its height), texture coordinates a half holds closely.
*/
class NearVertices final : public scene::IVertexBuffer
{
public:
	// Kept packed if packing changes nothing the shader reads (`packable`: the
	// material doesn't read a normal's length either).
	NearVertices(std::vector<video::S3DVertex> vertices, bool packable);

	HWBuffer::Type getBufferType() const override { return HWBuffer::Type::VERTEX; }
	const void *getData() const override;
	void *getData() override;
	u32 getCount() const override;
	video::E_VERTEX_TYPE getType() const override { return m_packed ? video::EVT_NEAR : video::EVT_STANDARD; }
	u32 getElementSize() const override { return m_packed ? sizeof(video::S3DVertexNear) : sizeof(video::S3DVertex); }
	const core::vector3df &getPosition(u32 i) const override;
	core::vector3df &getPosition(u32 i) override;
	// a packed vertex has neither as Irrlicht's vectors: asking throws
	const core::vector3df &getNormal(u32 i) const override { return standard(i).Normal; }
	core::vector3df &getNormal(u32 i) override { return standard(i).Normal; }
	const core::vector2df &getTCoords(u32 i) const override { return standard(i).TCoords; }
	core::vector2df &getTCoords(u32 i) override { return standard(i).TCoords; }
	const scene::WeightBuffer *getWeightBuffer() const override { return nullptr; }
	void useSwSkinning() override;

	void release(); // the card's copy the only one now
	bool released() const { return m_released; }
	size_t ram() const; // what it holds in RAM still

private:
	const video::S3DVertex &standard(u32 i) const;
	video::S3DVertex &standard(u32 i);
	void live() const; // throws once released

	std::vector<video::S3DVertex> m_standard; // one of these two holds it
	std::vector<video::S3DVertexNear> m_near;
	bool m_packed = false;
	u32 m_count = 0;
	bool m_released = false;
};

class NearIndices final : public scene::IIndexBuffer
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

class NearBuffer final : public scene::IMeshBuffer
{
public:
	// How it may be kept.
	enum class Pack { Never, IfExact };
	// Its vertices and indices, its bounds found from them; packed if `pack`
	// allows and nothing would change (NearVertices).
	NearBuffer(std::vector<video::S3DVertex> vertices, std::vector<u16> indices, Pack pack);
	~NearBuffer() override;
	NearBuffer(const NearBuffer &) = delete;
	NearBuffer &operator=(const NearBuffer &) = delete;

	video::SMaterial Material;

	video::SMaterial &getMaterial() override { return Material; }
	const video::SMaterial &getMaterial() const override { return Material; }
	const scene::IVertexBuffer *getVertexBuffer() const override { return m_vertices; }
	scene::IVertexBuffer *getVertexBuffer() override { return m_vertices; }
	const scene::IIndexBuffer *getIndexBuffer() const override { return m_indices; }
	scene::IIndexBuffer *getIndexBuffer() override { return m_indices; }
	const core::aabbox3df &getBoundingBox() const override { return m_box; }
	void setBoundingBox(const core::aabbox3df &box) override { m_box = box; }
	void recalculateBoundingBox() override;
	void append(const void *const vertices, u32 numVertices, const u16 *const indices, u32 numIndices) override;
	void setPrimitiveType(scene::E_PRIMITIVE_TYPE type) override { m_primitive = type; }
	scene::E_PRIMITIVE_TYPE getPrimitiveType() const override { return m_primitive; }

	// Let go of in RAM, if the driver holds both (a small one it draws from RAM: kept).
	void release();
	bool released() const { return m_indices->released(); }
	// On the card still (the driver lets a buffer go unused for long).
	bool held() const { return m_vertices->Link && m_indices->Link; }
	size_t ram() const; // what it holds in RAM still
	size_t bytes() const; // its whole mesh's
	bool packed() const { return m_vertices->getType() == video::EVT_NEAR; }

private:
	NearVertices *m_vertices;
	NearIndices *m_indices;
	core::aabbox3df m_box{ v3f() };
	scene::E_PRIMITIVE_TYPE m_primitive = scene::EPT_TRIANGLES;
};
