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

#include "SimpleTemplate.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace debugserver {

SimpleTemplate::SimpleTemplate() {
    Clear();
}

bool SimpleTemplate::LoadFromString(const std::string& templateStr) {
    m_template = templateStr;
    m_lastError.clear();
    return true;
}

bool SimpleTemplate::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        m_lastError = "Failed to open file: " + filePath;
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    m_template = ss.str();
    m_lastError.clear();
    return true;
}

void SimpleTemplate::SetVariable(const std::string& name, const std::string& value) {
    m_variables[name] = value;
}

void SimpleTemplate::SetVariable(const std::string& name, const char* value) {
    m_variables[name] = value ? value : "";
}

void SimpleTemplate::SetVariables(const VariableMap& vars) {
    for (const auto& kv : vars) {
        m_variables[kv.first] = kv.second;
    }
}

void SimpleTemplate::SetVariable(const std::string& name, int value) {
    m_variables[name] = std::to_string(value);
}

void SimpleTemplate::SetVariable(const std::string& name, unsigned int value) {
    m_variables[name] = std::to_string(value);
}

void SimpleTemplate::SetVariable(const std::string& name, long long value) {
    m_variables[name] = std::to_string(value);
}

void SimpleTemplate::SetVariable(const std::string& name, double value, int precision) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    m_variables[name] = ss.str();
}

void SimpleTemplate::SetVariable(const std::string& name, bool value) {
    m_variables[name] = value ? "true" : "false";
    // Also set as condition
    m_conditions[name] = value;
}

void SimpleTemplate::SetArray(const std::string& name, const ArrayData& items) {
    m_arrays[name] = items;
}

void SimpleTemplate::AddArrayItem(const std::string& name, const VariableMap& item) {
    m_arrays[name].push_back(item);
}

void SimpleTemplate::SetCondition(const std::string& name, bool value) {
    m_conditions[name] = value;
}

void SimpleTemplate::SetPartialProvider(PartialProvider provider) {
    m_partialProvider = std::move(provider);
}

std::string SimpleTemplate::Render() {
    return RenderInternal(m_template, m_variables);
}

std::string SimpleTemplate::Render(const VariableMap& additionalVars) {
    VariableMap mergedVars = m_variables;
    for (const auto& kv : additionalVars) {
        mergedVars[kv.first] = kv.second;
    }
    return RenderInternal(m_template, mergedVars);
}

std::string SimpleTemplate::RenderString(const std::string& templateStr,
                                          const VariableMap& vars) {
    SimpleTemplate tpl;
    tpl.LoadFromString(templateStr);
    tpl.SetVariables(vars);
    return tpl.Render();
}

void SimpleTemplate::Clear() {
    m_template.clear();
    m_variables.clear();
    m_arrays.clear();
    m_conditions.clear();
    m_partialProvider = nullptr;
    m_lastError.clear();
}

void SimpleTemplate::ClearVariables() {
    m_variables.clear();
    m_arrays.clear();
    m_conditions.clear();
}

std::string SimpleTemplate::RenderInternal(const std::string& templateStr,
                                            const VariableMap& vars) {
    std::string result = templateStr;

    // Process in order: partials, arrays, conditions, then variables
    result = ProcessPartials(result, vars);
    result = ProcessArrays(result, vars);
    result = ProcessConditions(result, vars);
    result = ProcessVariables(result, vars);

    return result;
}

std::string SimpleTemplate::ProcessVariables(const std::string& text,
                                              const VariableMap& vars) {
    std::string result;
    result.reserve(text.size());

    size_t pos = 0;
    while (pos < text.size()) {
        size_t start = text.find("{{", pos);
        if (start == std::string::npos) {
            result += text.substr(pos);
            break;
        }

        // Add text before the tag
        result += text.substr(pos, start - pos);

        size_t end = text.find("}}", start);
        if (end == std::string::npos) {
            // Unclosed tag, add rest of text
            result += text.substr(start);
            break;
        }

        std::string tag = text.substr(start + 2, end - start - 2);
        tag = Trim(tag);

        // Skip block tags (handled elsewhere)
        if (!tag.empty() && (tag[0] == '#' || tag[0] == '/' ||
                             tag[0] == '?' || tag[0] == '!' || tag[0] == '>')) {
            result += text.substr(start, end + 2 - start);
            pos = end + 2;
            continue;
        }

        // Variable substitution
        auto it = vars.find(tag);
        if (it != vars.end()) {
            result += it->second;
        }
        // If variable not found, output nothing (remove the tag)

        pos = end + 2;
    }

    return result;
}

std::string SimpleTemplate::ProcessArrays(const std::string& text,
                                           const VariableMap& contextVars) {
    std::string result;
    result.reserve(text.size());

    size_t pos = 0;
    while (pos < text.size()) {
        // Find {{#arrayName}}
        size_t start = text.find("{{#", pos);
        if (start == std::string::npos) {
            result += text.substr(pos);
            break;
        }

        // Add text before the tag
        result += text.substr(pos, start - pos);

        size_t tagEnd = text.find("}}", start);
        if (tagEnd == std::string::npos) {
            result += text.substr(start);
            break;
        }

        std::string arrayName = text.substr(start + 3, tagEnd - start - 3);
        arrayName = Trim(arrayName);

        // Find the matching end tag {{/arrayName}}
        size_t blockStart = tagEnd + 2;
        size_t blockEnd = FindEndTag(text, blockStart, arrayName);

        if (blockEnd == std::string::npos) {
            m_lastError = "Unclosed array block: " + arrayName;
            result += text.substr(start);
            break;
        }

        std::string blockContent = text.substr(blockStart, blockEnd - blockStart);

        // Process the array
        auto it = m_arrays.find(arrayName);
        if (it != m_arrays.end() && !it->second.empty()) {
            const ArrayData& items = it->second;
            for (size_t i = 0; i < items.size(); ++i) {
                // Merge context vars with item vars
                VariableMap itemVars = contextVars;
                for (const auto& kv : items[i]) {
                    itemVars[kv.first] = kv.second;
                }
                // Add special loop variables
                itemVars["_index"] = std::to_string(i);
                itemVars["_index1"] = std::to_string(i + 1);
                itemVars["_first"] = (i == 0) ? "true" : "";
                itemVars["_last"] = (i == items.size() - 1) ? "true" : "";

                // Render the block content with item vars
                result += RenderInternal(blockContent, itemVars);
            }
        }
        // If array not found or empty, output nothing

        // Skip past the end tag
        size_t endTagPos = text.find("}}", blockEnd);
        pos = (endTagPos != std::string::npos) ? endTagPos + 2 : text.size();
    }

    return result;
}

std::string SimpleTemplate::ProcessConditions(const std::string& text,
                                               const VariableMap& contextVars) {
    std::string result = text;

    // Process positive conditions {{?condition}}...{{/condition}}
    size_t pos = 0;
    while (pos < result.size()) {
        size_t start = result.find("{{?", pos);
        if (start == std::string::npos) break;

        size_t tagEnd = result.find("}}", start);
        if (tagEnd == std::string::npos) break;

        std::string condName = result.substr(start + 3, tagEnd - start - 3);
        condName = Trim(condName);

        size_t blockStart = tagEnd + 2;
        size_t blockEnd = FindEndTag(result, blockStart, condName);

        if (blockEnd == std::string::npos) {
            m_lastError = "Unclosed condition block: " + condName;
            pos = tagEnd + 2;
            continue;
        }

        std::string blockContent = result.substr(blockStart, blockEnd - blockStart);

        // Check if condition is true
        bool condValue = false;
        auto it = m_conditions.find(condName);
        if (it != m_conditions.end()) {
            condValue = it->second;
        } else {
            // Check if variable exists and is non-empty
            auto varIt = contextVars.find(condName);
            if (varIt != contextVars.end()) {
                condValue = !varIt->second.empty() &&
                            varIt->second != "false" &&
                            varIt->second != "0";
            }
        }

        // Find end of the closing tag
        size_t endTagEnd = result.find("}}", blockEnd);
        size_t fullBlockEnd = (endTagEnd != std::string::npos) ? endTagEnd + 2 : result.size();

        if (condValue) {
            // Replace block with rendered content
            std::string rendered = RenderInternal(blockContent, contextVars);
            result = result.substr(0, start) + rendered + result.substr(fullBlockEnd);
            pos = start + rendered.size();
        } else {
            // Remove the block
            result = result.substr(0, start) + result.substr(fullBlockEnd);
            pos = start;
        }
    }

    // Process negative conditions {{!condition}}...{{/condition}}
    pos = 0;
    while (pos < result.size()) {
        size_t start = result.find("{{!", pos);
        if (start == std::string::npos) break;

        size_t tagEnd = result.find("}}", start);
        if (tagEnd == std::string::npos) break;

        std::string condName = result.substr(start + 3, tagEnd - start - 3);
        condName = Trim(condName);

        size_t blockStart = tagEnd + 2;
        size_t blockEnd = FindEndTag(result, blockStart, condName);

        if (blockEnd == std::string::npos) {
            m_lastError = "Unclosed negated condition block: " + condName;
            pos = tagEnd + 2;
            continue;
        }

        std::string blockContent = result.substr(blockStart, blockEnd - blockStart);

        // Check if condition is false (negated)
        bool condValue = false;
        auto it = m_conditions.find(condName);
        if (it != m_conditions.end()) {
            condValue = it->second;
        } else {
            auto varIt = contextVars.find(condName);
            if (varIt != contextVars.end()) {
                condValue = !varIt->second.empty() &&
                            varIt->second != "false" &&
                            varIt->second != "0";
            }
        }

        size_t endTagEnd = result.find("}}", blockEnd);
        size_t fullBlockEnd = (endTagEnd != std::string::npos) ? endTagEnd + 2 : result.size();

        if (!condValue) {  // Note: negated
            std::string rendered = RenderInternal(blockContent, contextVars);
            result = result.substr(0, start) + rendered + result.substr(fullBlockEnd);
            pos = start + rendered.size();
        } else {
            result = result.substr(0, start) + result.substr(fullBlockEnd);
            pos = start;
        }
    }

    return result;
}

std::string SimpleTemplate::ProcessPartials(const std::string& text,
                                             const VariableMap& contextVars) {
    if (!m_partialProvider) {
        return text;
    }

    std::string result;
    result.reserve(text.size());

    size_t pos = 0;
    while (pos < text.size()) {
        size_t start = text.find("{{>", pos);
        if (start == std::string::npos) {
            result += text.substr(pos);
            break;
        }

        result += text.substr(pos, start - pos);

        size_t end = text.find("}}", start);
        if (end == std::string::npos) {
            result += text.substr(start);
            break;
        }

        std::string partialName = text.substr(start + 3, end - start - 3);
        partialName = Trim(partialName);

        // Get the partial content
        std::string partialContent = m_partialProvider(partialName);
        if (!partialContent.empty()) {
            // Render the partial with current context
            result += RenderInternal(partialContent, contextVars);
        }

        pos = end + 2;
    }

    return result;
}

size_t SimpleTemplate::FindEndTag(const std::string& text, size_t startPos,
                                   const std::string& tagName) {
    std::string endTag = "{{/" + tagName + "}}";
    std::string openTag1 = "{{#" + tagName + "}}";
    std::string openTag2 = "{{?" + tagName + "}}";
    std::string openTag3 = "{{!" + tagName + "}}";

    int depth = 1;
    size_t pos = startPos;

    while (pos < text.size() && depth > 0) {
        size_t nextEnd = text.find(endTag, pos);
        size_t nextOpen1 = text.find(openTag1, pos);
        size_t nextOpen2 = text.find(openTag2, pos);
        size_t nextOpen3 = text.find(openTag3, pos);

        // Find the minimum of all positions
        size_t nextOpen = std::min({nextOpen1, nextOpen2, nextOpen3});

        if (nextEnd == std::string::npos) {
            return std::string::npos;
        }

        if (nextOpen != std::string::npos && nextOpen < nextEnd) {
            // Found nested open tag
            depth++;
            // Move past the opening tag
            size_t tagEnd = text.find("}}", nextOpen);
            pos = (tagEnd != std::string::npos) ? tagEnd + 2 : nextOpen + 4;
        } else {
            // Found end tag
            depth--;
            if (depth == 0) {
                return nextEnd;
            }
            pos = nextEnd + endTag.size();
        }
    }

    return std::string::npos;
}

std::string SimpleTemplate::Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

} // namespace debugserver
