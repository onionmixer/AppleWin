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
 * Simple Template Engine
 * Internal implementation - no external library dependencies
 *
 * Supported syntax:
 *   {{variable}}           - Variable substitution
 *   {{#array}}...{{/array}} - Array/loop block
 *   {{?condition}}...{{/condition}} - Conditional block (true)
 *   {{!condition}}...{{/condition}} - Conditional block (false/negated)
 *   {{>partial}}           - Include another template (partial)
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>

namespace debugserver {

class SimpleTemplate {
public:
    // Type aliases
    using VariableMap = std::map<std::string, std::string>;
    using ArrayData = std::vector<VariableMap>;
    using PartialProvider = std::function<std::string(const std::string&)>;

    SimpleTemplate();
    ~SimpleTemplate() = default;

    // Load template from string
    bool LoadFromString(const std::string& templateStr);

    // Load template from file
    bool LoadFromFile(const std::string& filePath);

    // Set a single variable
    void SetVariable(const std::string& name, const std::string& value);
    void SetVariable(const std::string& name, const char* value);

    // Set multiple variables at once
    void SetVariables(const VariableMap& vars);

    // Set a variable with numeric value (convenience)
    void SetVariable(const std::string& name, int value);
    void SetVariable(const std::string& name, unsigned int value);
    void SetVariable(const std::string& name, long long value);
    void SetVariable(const std::string& name, double value, int precision = 2);
    void SetVariable(const std::string& name, bool value);

    // Set array data for loop blocks
    void SetArray(const std::string& name, const ArrayData& items);

    // Add a single item to an array
    void AddArrayItem(const std::string& name, const VariableMap& item);

    // Set condition for conditional blocks
    void SetCondition(const std::string& name, bool value);

    // Set partial template provider
    void SetPartialProvider(PartialProvider provider);

    // Render the template with current variables
    std::string Render();

    // Render with additional variables (merged with existing)
    std::string Render(const VariableMap& additionalVars);

    // Static render helper (one-shot rendering)
    static std::string RenderString(const std::string& templateStr,
                                     const VariableMap& vars);

    // Get last error message
    const std::string& GetLastError() const { return m_lastError; }

    // Clear all data
    void Clear();

    // Clear only variables (keep template)
    void ClearVariables();

private:
    // Rendering implementation
    std::string RenderInternal(const std::string& templateStr,
                                const VariableMap& vars);

    // Process different block types
    std::string ProcessVariables(const std::string& text,
                                  const VariableMap& vars);
    std::string ProcessArrays(const std::string& text,
                               const VariableMap& contextVars);
    std::string ProcessConditions(const std::string& text,
                                   const VariableMap& contextVars);
    std::string ProcessPartials(const std::string& text,
                                 const VariableMap& contextVars);

    // Find matching end tag
    size_t FindEndTag(const std::string& text, size_t startPos,
                      const std::string& tagName);

    // Utility functions
    static std::string Trim(const std::string& str);

    std::string m_template;
    VariableMap m_variables;
    std::map<std::string, ArrayData> m_arrays;
    std::map<std::string, bool> m_conditions;
    PartialProvider m_partialProvider;
    std::string m_lastError;
};

} // namespace debugserver
