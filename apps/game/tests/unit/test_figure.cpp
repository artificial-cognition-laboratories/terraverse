// Terraverse
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "tests/unit/test.h"

#include "library/physics/pose.h"
#include "library/physics/skeleton.h"
#include "src/client/views/figure/figure_mesh.h"
#include <cmath>

using namespace physics;

/*
	The figure as the wiki's scenes and the inventory draw it
	(views/figure/figure_mesh): a closed box for every bone, standing on the
	origin a person tall, each face wound to look out of its box and lit from
	above, and moved whole by where it's placed.
*/
class TestFigure : public TestBase {
public:
	TestFigure() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestFigure"; }

	void runTests(IGameDef *gamedef);

	void testABoxForEveryBone();
	void testStandsAPersonTall();
	void testFacesLookOut();
	void testLitFromAbove();
	void testPlacedWhole();
};

static TestFigure g_test_instance;

void TestFigure::runTests(IGameDef *gamedef)
{
	TEST(testABoxForEveryBone);
	TEST(testStandsAPersonTall);
	TEST(testFacesLookOut);
	TEST(testLitFromAbove);
	TEST(testPlacedWhole);
}

namespace {

constexpr size_t FACES = 6, CORNERS = 4, BOX_VERTICES = FACES * CORNERS, BOX_INDICES = FACES * 6;

Bones standing();
size_t boxes(const Bones &b);
v3f middle(const figure_mesh::Mesh &mesh, size_t box);

} // namespace

// Six faces of two triangles for each of the skeleton's boxes, nothing else
void TestFigure::testABoxForEveryBone()
{
	const Bones b = standing();
	const figure_mesh::Mesh mesh = figure_mesh::build(b, core::IdentityMatrix);
	UASSERT(boxes(b) > 10);
	UASSERTEQ(size_t, mesh.vertices.size(), boxes(b) * BOX_VERTICES);
	UASSERTEQ(size_t, mesh.indices.size(), boxes(b) * BOX_INDICES);
	for (u16 i : mesh.indices)
		UASSERT(i < mesh.vertices.size());
}

// Its feet on the origin, its head a person's height over them, about its
// middle side to side
void TestFigure::testStandsAPersonTall()
{
	const figure_mesh::Mesh mesh = figure_mesh::build(standing(), core::IdentityMatrix);
	v3f lo(1e9f), hi(-1e9f);
	for (const video::S3DVertex &v : mesh.vertices) {
		lo = v3f(std::min(lo.X, v.Pos.X), std::min(lo.Y, v.Pos.Y), std::min(lo.Z, v.Pos.Z));
		hi = v3f(std::max(hi.X, v.Pos.X), std::max(hi.Y, v.Pos.Y), std::max(hi.Z, v.Pos.Z));
	}
	UASSERT(std::abs(lo.Y) < 0.1f);
	UASSERT(hi.Y > 1.6f && hi.Y < 2.1f);
	UASSERT(std::abs(lo.X + hi.X) < 0.2f && hi.X - lo.X < 1.2f);
}

// Every triangle is wound clockwise seen from outside its box (Irrlicht's
// left-handed front face), so back-face culling hides only the insides
void TestFigure::testFacesLookOut()
{
	const figure_mesh::Mesh mesh = figure_mesh::build(standing(), core::IdentityMatrix);
	int out = 0, in = 0;
	for (size_t t = 0; t < mesh.indices.size(); t += 3) {
		const v3f a = mesh.vertices[mesh.indices[t]].Pos, b = mesh.vertices[mesh.indices[t + 1]].Pos,
				c = mesh.vertices[mesh.indices[t + 2]].Pos;
		const v3f from = (a + b + c) / 3 - middle(mesh, t / BOX_INDICES);
		((b - a).crossProduct(c - a).dotProduct(from) > 0 ? out : in)++;
	}
	UASSERT(out > 0 && in == 0);
}

// A box's top is lighter than its bottom, in every bone
void TestFigure::testLitFromAbove()
{
	const figure_mesh::Mesh mesh = figure_mesh::build(standing(), core::IdentityMatrix);
	for (size_t box = 0; box < mesh.vertices.size() / BOX_VERTICES; box++) {
		const video::SColor top = mesh.vertices[box * BOX_VERTICES].Color, bottom = mesh.vertices[box * BOX_VERTICES + CORNERS].Color;
		UASSERT(top.getRed() + top.getGreen() + top.getBlue() > bottom.getRed() + bottom.getGreen() + bottom.getBlue());
	}
}

// `root` moves all of it: the same figure, every corner shifted alike
void TestFigure::testPlacedWhole()
{
	const Bones b = standing();
	core::matrix4 root;
	root.setTranslation(v3f(3, -2, 5));
	const figure_mesh::Mesh here = figure_mesh::build(b, core::IdentityMatrix), there = figure_mesh::build(b, root);
	UASSERTEQ(size_t, there.vertices.size(), here.vertices.size());
	for (size_t i = 0; i < here.vertices.size(); i++)
		UASSERT(there.vertices[i].Pos.getDistanceFrom(here.vertices[i].Pos + v3f(3, -2, 5)) < 1e-4f);
}

namespace {

// Standing still on flat ground, settled
Bones standing()
{
	Posing posing;
	Contacts contacts;
	Bones b;
	for (int i = 0; i < 120; i++)
		b = posing.pose(1 / 60.f, v3f(0), 0, false, Stance::STAND, 0, std::nullopt, 0, contacts,
				[](v3f) { return 0.f; }, [](v3f, float) { return v3f(0); });
	return b;
}

size_t boxes(const Bones &b)
{
	size_t n = 0;
	eachBoneBox(b, [&](const core::matrix4 &, v3f, v3f, BoneColour) { n++; });
	return n;
}

v3f middle(const figure_mesh::Mesh &mesh, size_t box)
{
	v3f sum(0);
	for (size_t k = 0; k < BOX_VERTICES; k++)
		sum += mesh.vertices[box * BOX_VERTICES + k].Pos;
	return sum / (float)BOX_VERTICES;
}

} // namespace
