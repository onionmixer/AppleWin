/*
 * Test program for JsonBuilder and SimpleTemplate
 * Build:
 *   g++ -std=c++17 -o test_template test_template.cpp JsonBuilder.cpp SimpleTemplate.cpp -lpthread
 */

#include "JsonBuilder.h"
#include "SimpleTemplate.h"
#include <iostream>

void testJsonBuilder() {
    std::cout << "=== JsonBuilder Test ===" << std::endl;

    debugserver::JsonBuilder json;

    json.BeginObject()
        .Add("status", "ok")
        .Add("server", "AppleWin Debug Server")
        .Add("version", 1)
        .Add("enabled", true)
        .AddHex16("pc", 0xC600)
        .AddHex8("a", 0xFF)
        .Key("registers").BeginObject()
            .AddHex8("A", 0x00)
            .AddHex8("X", 0x01)
            .AddHex8("Y", 0x02)
            .AddHex16("PC", 0xC600)
            .AddHex8("SP", 0xFF)
            .AddHex8("P", 0x30)
        .EndObject()
        .Key("breakpoints").BeginArray()
            .BeginObject()
                .Add("type", "pc")
                .AddHex16("address", 0xC600)
            .EndObject()
            .BeginObject()
                .Add("type", "memory")
                .AddHex16("address", 0x0300)
                .Add("mode", "rw")
            .EndObject()
        .EndArray()
    .EndObject();

    std::cout << "Compact:" << std::endl;
    std::cout << json.ToString() << std::endl;
    std::cout << std::endl;

    std::cout << "Pretty:" << std::endl;
    std::cout << json.ToPrettyString() << std::endl;
    std::cout << std::endl;
}

void testSimpleTemplate() {
    std::cout << "=== SimpleTemplate Test ===" << std::endl;

    // Test variable substitution
    {
        std::cout << "-- Variable substitution --" << std::endl;
        debugserver::SimpleTemplate tpl;
        tpl.LoadFromString("Hello {{name}}! Your score is {{score}}.");
        tpl.SetVariable("name", "User");
        tpl.SetVariable("score", 100);
        std::cout << tpl.Render() << std::endl;
    }

    // Test array loop
    {
        std::cout << "-- Array loop --" << std::endl;
        debugserver::SimpleTemplate tpl;
        tpl.LoadFromString(R"(
Registers:
{{#registers}}  {{name}}: {{value}}
{{/registers}})");

        debugserver::SimpleTemplate::ArrayData regs;
        regs.push_back({{"name", "A"}, {"value", "$00"}});
        regs.push_back({{"name", "X"}, {"value", "$01"}});
        regs.push_back({{"name", "Y"}, {"value", "$02"}});
        regs.push_back({{"name", "PC"}, {"value", "$C600"}});
        tpl.SetArray("registers", regs);

        std::cout << tpl.Render() << std::endl;
    }

    // Test condition (true)
    {
        std::cout << "-- Condition (true) --" << std::endl;
        debugserver::SimpleTemplate tpl;
        tpl.LoadFromString("{{?running}}Emulator is running{{/running}}{{!running}}Emulator is stopped{{/running}}");
        tpl.SetCondition("running", true);
        std::cout << tpl.Render() << std::endl;
    }

    // Test condition (false)
    {
        std::cout << "-- Condition (false) --" << std::endl;
        debugserver::SimpleTemplate tpl;
        tpl.LoadFromString("{{?running}}Emulator is running{{/running}}{{!running}}Emulator is stopped{{/running}}");
        tpl.SetCondition("running", false);
        std::cout << tpl.Render() << std::endl;
    }

    // Test combined
    {
        std::cout << "-- Combined test (HTML-like) --" << std::endl;
        debugserver::SimpleTemplate tpl;
        tpl.LoadFromString(R"(<!DOCTYPE html>
<html>
<head><title>{{title}}</title></head>
<body>
<h1>{{title}}</h1>
{{?hasBreakpoints}}
<h2>Breakpoints</h2>
<table>
{{#breakpoints}}
<tr><td>{{_index1}}</td><td>{{type}}</td><td>{{address}}</td></tr>
{{/breakpoints}}
</table>
{{/hasBreakpoints}}
{{!hasBreakpoints}}
<p>No breakpoints set.</p>
{{/hasBreakpoints}}
</body>
</html>)");

        tpl.SetVariable("title", "CPU Debug Info");
        tpl.SetCondition("hasBreakpoints", true);

        debugserver::SimpleTemplate::ArrayData bps;
        bps.push_back({{"type", "PC"}, {"address", "$C600"}});
        bps.push_back({{"type", "Memory"}, {"address", "$0300"}});
        bps.push_back({{"type", "Register"}, {"address", "A=00"}});
        tpl.SetArray("breakpoints", bps);

        std::cout << tpl.Render() << std::endl;
    }

    // Test RenderString static helper
    {
        std::cout << "-- Static RenderString --" << std::endl;
        std::string result = debugserver::SimpleTemplate::RenderString(
            "PC: {{pc}}, A: {{a}}, X: {{x}}, Y: {{y}}",
            {{"pc", "$C600"}, {"a", "$00"}, {"x", "$01"}, {"y", "$02"}}
        );
        std::cout << result << std::endl;
    }
}

int main() {
    testJsonBuilder();
    std::cout << std::endl;
    testSimpleTemplate();
    std::cout << std::endl;
    std::cout << "All tests completed!" << std::endl;
    return 0;
}
