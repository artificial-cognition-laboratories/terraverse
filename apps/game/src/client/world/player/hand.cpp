// Terra Firma
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "src/client/world/player/hand.h"

#include "src/shared/base/exceptions.h"
#include <algorithm>
#include <iterator>
#include <utility>

namespace {

constexpr int SIZES[] = { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64 };

} // namespace

const Hand::Action Hand::ACTIONS[size_t(Verb::Count)] = {
	{ "focus", false },
	{ "mirror", false },
	{ "undo", false },
	{ "start", false },
	{ "save", true },
	{ "place", true },
	{ "hollow", false },
	{ "level", false },
	{ "reset", false },
	{ "detach", false },
	{ "delete", false },
	{ "relink", false },
	{ "socket", false },
	{ "unsocket", false },
	{ "copy", false },
	{ "paste", false },
	{ "name", false },
	{ "take", true },
	{ "forget", true },
	{ "round", false },
};

const Hand::BrushKind Hand::BRUSHES[size_t(Brush::Count)] = {
	{ "add", sculpt::Act::Add, false, false, true },
	{ "cut", sculpt::Act::Cut, false, false, true },
	{ "paint", sculpt::Act::Paint, false, false, true },
	{ "part", sculpt::Act::Add, true, false, false },
	{ "pull", sculpt::Act::Add, false, true, false },
};

void Hand::setPartJoint(piece::Joint joint)
{
	if (joint >= piece::Joint::Count)
		throw BaseException("Hand::setPartJoint: no such joint");
	m_part_joint = joint;
}

void Hand::pickBrush(const std::string &id)
{
	for (size_t i = 0; i < size_t(Brush::Count); i++)
		if (id == BRUSHES[i].id && BRUSHES[i].picked) {
			m_brush = Brush(i);
			return;
		}
	throw BaseException("Hand::pickBrush: no brush \"" + id + "\" on the rail");
}

void Hand::nextBrush()
{
	size_t i = size_t(m_brush);
	do
		i = (i + 1) % size_t(Brush::Count);
	while (!BRUSHES[i].picked);
	m_brush = Brush(i);
}

void Hand::relink(u64 other, std::optional<piece::Joint> joint)
{
	if ((joint && *joint >= piece::Joint::Count) || other == 0)
		throw BaseException("Hand::relink: no such joint");
	m_asked.push_back({ Verb::Relink, "", other, joint ? *joint : piece::Joint::Fixed, !joint });
}

void Hand::socket(piece::Joint joint)
{
	if (joint >= piece::Joint::Count)
		throw BaseException("Hand::socket: no such joint");
	m_asked.push_back({ Verb::Socket, "", 0, joint, false });
}

void Hand::unsocket(u64 id)
{
	if (id == 0)
		throw BaseException("Hand::unsocket: no socket");
	m_asked.push_back({ Verb::Unsocket, "", id, piece::Joint::Fixed, false });
}

void Hand::ask(const std::string &id, const std::string &name)
{
	for (size_t i = 0; i < size_t(Verb::Count); i++) {
		if (id != ACTIONS[i].id)
			continue;
		if (Verb(i) == Verb::Relink || Verb(i) == Verb::Socket || Verb(i) == Verb::Unsocket)
			throw BaseException(std::string("Hand::ask: ") + ACTIONS[i].id + " takes what it's of: its own call");
		if (ACTIONS[i].named == name.empty())
			throw BaseException("Hand::ask: \"" + id + (ACTIONS[i].named ? "\" takes a name" : "\" takes no name"));
		m_asked.push_back({ Verb(i), name });
		return;
	}
	throw BaseException("Hand::ask: no action \"" + id + "\"");
}

std::vector<Hand::Ask> Hand::take()
{
	return std::exchange(m_asked, {});
}

void Hand::pick(const std::string &id)
{
	for (size_t i = 0; i < size_t(sculpt::Tool::Count); i++)
		if (id == sculpt::KINDS[i].id) {
			m_tool = sculpt::Tool(i);
			return;
		}
	throw BaseException("Hand::pick: no tool \"" + id + "\"");
}

void Hand::resize(int steps)
{
	const int *at = std::lower_bound(std::begin(SIZES), std::end(SIZES), m_size);
	const int i = std::clamp(int(at - std::begin(SIZES)) + steps, 0, int(std::size(SIZES)) - 1);
	m_size = SIZES[i];
}

std::string Hand::describe() const
{
	const int steps = shape::STEPS;
	const std::string size = m_size % steps == 0
			? std::to_string(m_size / steps) + " block" + (m_size > steps ? "s" : "")
			: std::to_string(m_size) + "/16";
	return std::string(BRUSHES[size_t(m_brush)].id) + " · " + sculpt::KINDS[size_t(m_tool)].name + " · " + size
			+ (m_mirror ? " · mirrored" : "");
}
