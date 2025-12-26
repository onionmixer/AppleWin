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

#include "JsonBuilder.h"

namespace debugserver {

std::string JsonBuilder::EscapeString(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 16);

    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control characters: encode as \u00XX
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }

    return result;
}

std::string JsonBuilder::ToHexString(uint32_t val, int width) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%0*X", width, val);
    return buf;
}

JsonBuilder& JsonBuilder::AddHex8(const std::string& key, uint8_t val) {
    return Add(key, "$" + ToHexString(val, 2));
}

JsonBuilder& JsonBuilder::AddHex16(const std::string& key, uint16_t val) {
    return Add(key, "$" + ToHexString(val, 4));
}

JsonBuilder& JsonBuilder::AddHex32(const std::string& key, uint32_t val) {
    return Add(key, "$" + ToHexString(val, 8));
}

std::string JsonBuilder::ToPrettyString(int indent) const {
    return FormatPretty(ToString(), indent);
}

std::string JsonBuilder::FormatPretty(const std::string& json, int indentSize) const {
    std::ostringstream out;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    char prevChar = 0;

    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];

        if (escaped) {
            out << c;
            escaped = false;
            prevChar = c;
            continue;
        }

        if (c == '\\' && inString) {
            out << c;
            escaped = true;
            prevChar = c;
            continue;
        }

        if (c == '"') {
            out << c;
            inString = !inString;
            prevChar = c;
            continue;
        }

        if (inString) {
            out << c;
            prevChar = c;
            continue;
        }

        // Outside of string
        switch (c) {
            case '{':
            case '[':
                out << c;
                depth++;
                // Check if next non-whitespace is closing bracket
                {
                    size_t next = i + 1;
                    while (next < json.size() && (json[next] == ' ' || json[next] == '\t')) {
                        next++;
                    }
                    if (next < json.size() && (json[next] == '}' || json[next] == ']')) {
                        // Empty object/array, don't add newline
                    } else {
                        out << '\n';
                        WriteIndent(out, depth, indentSize);
                    }
                }
                break;

            case '}':
            case ']':
                depth--;
                if (prevChar != '{' && prevChar != '[') {
                    out << '\n';
                    WriteIndent(out, depth, indentSize);
                }
                out << c;
                break;

            case ',':
                out << c;
                out << '\n';
                WriteIndent(out, depth, indentSize);
                break;

            case ':':
                out << c << ' ';
                break;

            case ' ':
            case '\t':
            case '\n':
            case '\r':
                // Skip whitespace outside strings
                break;

            default:
                out << c;
                break;
        }

        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            prevChar = c;
        }
    }

    return out.str();
}

void JsonBuilder::WriteIndent(std::ostringstream& out, int depth, int indentSize) const {
    for (int i = 0; i < depth * indentSize; ++i) {
        out << ' ';
    }
}

} // namespace debugserver
