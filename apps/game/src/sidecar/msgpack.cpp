#include "msgpack.h"

#include <cstring>

namespace sidecar::mp {

Value Value::boolean(bool b) { Value v; v.m_kind = Kind::Bool; v.m_bool = b; return v; }
Value Value::integer(s64 i) { Value v; v.m_kind = Kind::Int; v.m_int = i; return v; }
Value Value::number(double d) { Value v; v.m_kind = Kind::Float; v.m_float = d; return v; }
Value Value::string(std::string s) { Value v; v.m_kind = Kind::String; v.m_string = std::move(s); return v; }
Value Value::array(std::vector<Value> items) { Value v; v.m_kind = Kind::Array; v.m_items = std::move(items); return v; }
Value Value::map(std::vector<std::string> keys, std::vector<Value> values)
{
	if (keys.size() != values.size())
		throw Error("a map needs a value for every key");
	Value v;
	v.m_kind = Kind::Map;
	v.m_keys = std::move(keys);
	v.m_items = std::move(values);
	return v;
}

bool Value::asBool() const
{
	if (m_kind != Kind::Bool) throw Error("expected a boolean");
	return m_bool;
}

s64 Value::asInt() const
{
	if (m_kind != Kind::Int) throw Error("expected an integer");
	return m_int;
}

double Value::asNumber() const
{
	if (m_kind == Kind::Int) return (double)m_int;
	if (m_kind != Kind::Float) throw Error("expected a number");
	return m_float;
}

const std::string &Value::asString() const
{
	if (m_kind != Kind::String) throw Error("expected a string");
	return m_string;
}

const std::vector<Value> &Value::asArray() const
{
	if (m_kind != Kind::Array) throw Error("expected an array");
	return m_items;
}

const Value *Value::get(std::string_view key) const
{
	if (m_kind != Kind::Map) throw Error("expected a map");
	for (size_t i = 0; i < m_keys.size(); i++)
		if (m_keys[i] == key)
			return &m_items[i];
	return nullptr;
}

const Value &Value::at(std::string_view key) const
{
	const Value *v = get(key);
	if (!v) throw Error("expected a map with \"" + std::string(key) + "\"");
	return *v;
}

namespace {

struct Reader {
	const u8 *p;
	const u8 *end;

	void need(size_t n) const
	{
		if ((size_t)(end - p) < n) throw Error("a value runs past the end of its frame");
	}
	u64 be(int bytes)
	{
		need(bytes);
		u64 v = 0;
		for (int i = 0; i < bytes; i++)
			v = (v << 8) | *p++;
		return v;
	}
	std::string bytes(size_t n)
	{
		need(n);
		std::string s((const char *)p, n);
		p += n;
		return s;
	}

	Value array(size_t n, int depth)
	{
		std::vector<Value> items;
		items.reserve(std::min<size_t>(n, end - p)); // every value is at least a byte
		for (size_t i = 0; i < n; i++)
			items.push_back(value(depth + 1));
		return Value::array(std::move(items));
	}
	Value map(size_t n, int depth)
	{
		std::vector<std::string> keys;
		std::vector<Value> values;
		for (size_t i = 0; i < n; i++) {
			Value key = value(depth + 1);
			if (key.kind() != Value::Kind::String) throw Error("a map's keys must be strings");
			keys.push_back(key.asString());
			values.push_back(value(depth + 1));
		}
		return Value::map(std::move(keys), std::move(values));
	}

	Value value(int depth)
	{
		if (depth > 64) throw Error("values nest deeper than 64");
		need(1);
		const u8 b = *p++;
		if (b <= 0x7f) return Value::integer(b);
		if (b >= 0xe0) return Value::integer((s8)b);
		if ((b & 0xe0) == 0xa0) return Value::string(bytes(b & 0x1f));
		if ((b & 0xf0) == 0x90) return array(b & 0x0f, depth);
		if ((b & 0xf0) == 0x80) return map(b & 0x0f, depth);
		switch (b) {
		case 0xc0: return Value::nil();
		case 0xc2: return Value::boolean(false);
		case 0xc3: return Value::boolean(true);
		case 0xc4: case 0xd9: return Value::string(bytes(be(1)));
		case 0xc5: case 0xda: return Value::string(bytes(be(2)));
		case 0xc6: case 0xdb: return Value::string(bytes(be(4)));
		case 0xca: {
			u32 bits = (u32)be(4);
			float f;
			std::memcpy(&f, &bits, 4);
			return Value::number(f);
		}
		case 0xcb: {
			u64 bits = be(8);
			double d;
			std::memcpy(&d, &bits, 8);
			return Value::number(d);
		}
		case 0xcc: return Value::integer((s64)be(1));
		case 0xcd: return Value::integer((s64)be(2));
		case 0xce: return Value::integer((s64)be(4));
		case 0xcf: {
			u64 v = be(8);
			if (v > (u64)INT64_MAX) throw Error("an integer past 2^63");
			return Value::integer((s64)v);
		}
		case 0xd0: return Value::integer((s8)be(1));
		case 0xd1: return Value::integer((s16)be(2));
		case 0xd2: return Value::integer((s32)be(4));
		case 0xd3: return Value::integer((s64)be(8));
		case 0xdc: return array(be(2), depth);
		case 0xdd: return array(be(4), depth);
		case 0xde: return map(be(2), depth);
		case 0xdf: return map(be(4), depth);
		default: throw Error("a MessagePack type the sidecar never sends (0x" +
				std::string(1, "0123456789abcdef"[b >> 4]) + "0123456789abcdef"[b & 15] + ")");
		}
	}
};

} // namespace

Value decode(const u8 *data, size_t size)
{
	Reader r{ data, data + size };
	Value v = r.value(0);
	if (r.p != r.end) throw Error("a frame holds more than one value");
	return v;
}

void Writer::be(u64 v, int bytes)
{
	for (int i = bytes - 1; i >= 0; i--)
		m_out.push_back((char)((v >> (i * 8)) & 0xff));
}

void Writer::nil() { m_out.push_back((char)0xc0); }
void Writer::boolean(bool b) { m_out.push_back((char)(b ? 0xc3 : 0xc2)); }

void Writer::integer(s64 i)
{
	if (i >= 0 && i <= 0x7f) {
		m_out.push_back((char)i);
	} else if (i < 0 && i >= -32) {
		m_out.push_back((char)(s8)i);
	} else {
		m_out.push_back((char)0xd3);
		be((u64)i, 8);
	}
}

void Writer::number(double d)
{
	u64 bits;
	std::memcpy(&bits, &d, 8);
	m_out.push_back((char)0xcb);
	be(bits, 8);
}

void Writer::string(std::string_view s)
{
	const size_t n = s.size();
	if (n < 32) {
		m_out.push_back((char)(0xa0 | n));
	} else if (n < 0x100) {
		m_out.push_back((char)0xd9);
		be(n, 1);
	} else if (n < 0x10000) {
		m_out.push_back((char)0xda);
		be(n, 2);
	} else {
		m_out.push_back((char)0xdb);
		be(n, 4);
	}
	m_out.append(s);
}

void Writer::array(u32 n)
{
	if (n < 16) {
		m_out.push_back((char)(0x90 | n));
	} else if (n < 0x10000) {
		m_out.push_back((char)0xdc);
		be(n, 2);
	} else {
		m_out.push_back((char)0xdd);
		be(n, 4);
	}
}

void Writer::map(u32 n)
{
	if (n < 16) {
		m_out.push_back((char)(0x80 | n));
	} else if (n < 0x10000) {
		m_out.push_back((char)0xde);
		be(n, 2);
	} else {
		m_out.push_back((char)0xdf);
		be(n, 4);
	}
}

void Writer::value(const Value &v)
{
	switch (v.kind()) {
	case Value::Kind::Nil: nil(); break;
	case Value::Kind::Bool: boolean(v.asBool()); break;
	case Value::Kind::Int: integer(v.asInt()); break;
	case Value::Kind::Float: number(v.asNumber()); break;
	case Value::Kind::String: string(v.asString()); break;
	case Value::Kind::Array:
		array((u32)v.asArray().size());
		for (const Value &item : v.asArray())
			value(item);
		break;
	case Value::Kind::Map:
		throw Error("writing a decoded map isn't needed yet");
	}
}

} // namespace sidecar::mp
