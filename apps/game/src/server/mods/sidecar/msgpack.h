// MessagePack, as much of it as the sidecar's frames use: nil, booleans, integers,
// floats, strings, arrays and maps with string keys. No extensions.
#pragma once

#include "src/shared/core/irrlichttypes.h"
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sidecar::mp {

struct Error : std::runtime_error {
	using std::runtime_error::runtime_error;
};

class Value {
public:
	enum class Kind : u8 { Nil, Bool, Int, Float, String, Array, Map };

	static Value nil() { return Value(); }
	static Value boolean(bool b);
	static Value integer(s64 i);
	static Value number(double d);
	static Value string(std::string s);
	static Value array(std::vector<Value> items);
	static Value map(std::vector<std::string> keys, std::vector<Value> values);

	Kind kind() const { return m_kind; }
	bool isNil() const { return m_kind == Kind::Nil; }

	// Each throws Error naming what it expected, so a malformed frame says what was wrong.
	bool asBool() const;
	s64 asInt() const;
	double asNumber() const;        // an integer or a float
	const std::string &asString() const;
	const std::vector<Value> &asArray() const;
	// A map's value by key, or nullptr.
	const Value *get(std::string_view key) const;
	const Value &at(std::string_view key) const;  // throws if missing

private:
	Kind m_kind = Kind::Nil;
	bool m_bool = false;
	s64 m_int = 0;
	double m_float = 0;
	std::string m_string;
	std::vector<Value> m_items;       // an array's items, or a map's values
	std::vector<std::string> m_keys;  // a map's keys, beside its values
};

// One whole value from bytes. Throws Error if they're not exactly one value.
Value decode(const u8 *data, size_t size);

class Writer {
public:
	void nil();
	void boolean(bool b);
	void integer(s64 i);
	void number(double d);
	void string(std::string_view s);
	void array(u32 n);   // then n values
	void map(u32 n);     // then n key, value pairs
	void value(const Value &v);
	const std::string &bytes() const { return m_out; }

private:
	void be(u64 v, int bytes);
	std::string m_out;
};

} // namespace sidecar::mp
