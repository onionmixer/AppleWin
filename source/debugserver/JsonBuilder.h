/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2024, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * JSON Builder
 * Simple JSON generator - no external library dependencies
 * Only generates JSON (no parsing needed for debug server)
 */

#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <cstdio>

namespace debugserver {

class JsonBuilder {
public:
    JsonBuilder();
    ~JsonBuilder() = default;

    // Object operations
    JsonBuilder& BeginObject();
    JsonBuilder& EndObject();

    // Array operations
    JsonBuilder& BeginArray();
    JsonBuilder& EndArray();

    // Key for object (must be followed by a value)
    JsonBuilder& Key(const std::string& key);

    // Value types
    JsonBuilder& Value(const std::string& val);
    JsonBuilder& Value(const char* val);
    JsonBuilder& Value(int val);
    JsonBuilder& Value(unsigned int val);
    JsonBuilder& Value(long val);
    JsonBuilder& Value(unsigned long val);
    JsonBuilder& Value(long long val);
    JsonBuilder& Value(unsigned long long val);
    JsonBuilder& Value(double val, int precision = 6);
    JsonBuilder& Value(float val, int precision = 6);
    JsonBuilder& Value(bool val);
    JsonBuilder& Null();

    // Raw JSON value (for pre-formatted JSON)
    JsonBuilder& RawValue(const std::string& jsonValue);

    // Convenience methods: Key + Value in one call
    JsonBuilder& Add(const std::string& key, const std::string& val);
    JsonBuilder& Add(const std::string& key, const char* val);
    JsonBuilder& Add(const std::string& key, int val);
    JsonBuilder& Add(const std::string& key, unsigned int val);
    JsonBuilder& Add(const std::string& key, long val);
    JsonBuilder& Add(const std::string& key, unsigned long val);
    JsonBuilder& Add(const std::string& key, long long val);
    JsonBuilder& Add(const std::string& key, unsigned long long val);
    JsonBuilder& Add(const std::string& key, double val, int precision = 6);
    JsonBuilder& Add(const std::string& key, float val, int precision = 6);
    JsonBuilder& Add(const std::string& key, bool val);
    JsonBuilder& AddNull(const std::string& key);

    // Hex value helpers (useful for debug info)
    JsonBuilder& AddHex8(const std::string& key, uint8_t val);
    JsonBuilder& AddHex16(const std::string& key, uint16_t val);
    JsonBuilder& AddHex32(const std::string& key, uint32_t val);

    // Get the result
    std::string ToString() const;
    std::string ToPrettyString(int indent = 2) const;

    // Reset for reuse
    void Clear();

    // Check if empty
    bool IsEmpty() const;

private:
    void AddCommaIfNeeded();
    void WriteIndent(std::ostringstream& out, int depth, int indentSize) const;
    std::string FormatPretty(const std::string& json, int indentSize) const;

    static std::string EscapeString(const std::string& str);
    static std::string ToHexString(uint32_t val, int width);

    std::ostringstream m_stream;
    std::vector<char> m_stack;      // '{' or '[' for nesting tracking
    std::vector<bool> m_needComma;  // Whether next element needs preceding comma
};

// Inline implementations for performance-critical methods

inline JsonBuilder::JsonBuilder() {
    Clear();
}

inline void JsonBuilder::Clear() {
    m_stream.str("");
    m_stream.clear();
    m_stack.clear();
    m_needComma.clear();
}

inline bool JsonBuilder::IsEmpty() const {
    return m_stream.str().empty();
}

inline std::string JsonBuilder::ToString() const {
    return m_stream.str();
}

inline void JsonBuilder::AddCommaIfNeeded() {
    if (!m_needComma.empty() && m_needComma.back()) {
        m_stream << ',';
        m_needComma.back() = false;
    }
}

inline JsonBuilder& JsonBuilder::BeginObject() {
    AddCommaIfNeeded();
    m_stream << '{';
    m_stack.push_back('{');
    m_needComma.push_back(false);
    return *this;
}

inline JsonBuilder& JsonBuilder::EndObject() {
    m_stream << '}';
    if (!m_stack.empty()) m_stack.pop_back();
    if (!m_needComma.empty()) m_needComma.pop_back();
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::BeginArray() {
    AddCommaIfNeeded();
    m_stream << '[';
    m_stack.push_back('[');
    m_needComma.push_back(false);
    return *this;
}

inline JsonBuilder& JsonBuilder::EndArray() {
    m_stream << ']';
    if (!m_stack.empty()) m_stack.pop_back();
    if (!m_needComma.empty()) m_needComma.pop_back();
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Key(const std::string& key) {
    AddCommaIfNeeded();
    m_stream << '"' << EscapeString(key) << "\":";
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(const std::string& val) {
    AddCommaIfNeeded();
    m_stream << '"' << EscapeString(val) << '"';
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(const char* val) {
    if (val == nullptr) {
        return Null();
    }
    return Value(std::string(val));
}

inline JsonBuilder& JsonBuilder::Value(int val) {
    AddCommaIfNeeded();
    m_stream << val;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(unsigned int val) {
    AddCommaIfNeeded();
    m_stream << val;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(long val) {
    AddCommaIfNeeded();
    m_stream << val;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(unsigned long val) {
    AddCommaIfNeeded();
    m_stream << val;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(long long val) {
    AddCommaIfNeeded();
    m_stream << val;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(unsigned long long val) {
    AddCommaIfNeeded();
    m_stream << val;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(double val, int precision) {
    AddCommaIfNeeded();
    m_stream << std::fixed << std::setprecision(precision) << val;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(float val, int precision) {
    return Value(static_cast<double>(val), precision);
}

inline JsonBuilder& JsonBuilder::Value(bool val) {
    AddCommaIfNeeded();
    m_stream << (val ? "true" : "false");
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Null() {
    AddCommaIfNeeded();
    m_stream << "null";
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::RawValue(const std::string& jsonValue) {
    AddCommaIfNeeded();
    m_stream << jsonValue;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

// Convenience Add methods
inline JsonBuilder& JsonBuilder::Add(const std::string& key, const std::string& val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, const char* val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, int val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, unsigned int val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, long val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, unsigned long val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, long long val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, unsigned long long val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, double val, int precision) {
    return Key(key).Value(val, precision);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, float val, int precision) {
    return Key(key).Value(val, precision);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, bool val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::AddNull(const std::string& key) {
    return Key(key).Null();
}

} // namespace debugserver
