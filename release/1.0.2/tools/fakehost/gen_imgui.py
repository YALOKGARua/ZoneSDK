import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HEADER = os.path.join(HERE, "..", "..", "extern", "reshade", "reshade_overlay.hpp")
MEMBER = re.compile(r"^\s*(.+?)\(\*(\w+)\)\((.*)\);\s*$")
TRUE = {"TreeNode", "TreeNodeV", "TreeNodeV2", "TreeNodeEx", "TreeNodeExV", "TreeNodeExV2", "CollapsingHeader", "CollapsingHeader2", "BeginChild", "BeginChild2", "BeginTable", "BeginTabBar", "BeginTabItem", "ImFont_IsGlyphInFont"}
SPECIAL = {
    "GetIO": "return Io();",
    "GetStyle": "return Style();",
    "GetWindowDrawList": "return List();",
    "GetBackgroundDrawList": "return List();",
    "GetForegroundDrawList": "return List();",
    "GetFont": "return Font();",
    "GetFontSize": "return 13.0f;",
    "GetContentRegionAvail": "return ImVec2(600.0f, 400.0f);",
    "GetCursorScreenPos": "return ImVec2(100.0f, 100.0f);",
    "GetCursorPos": "return ImVec2(8.0f, 8.0f);",
    "GetItemRectMin": "return ImVec2(100.0f, 100.0f);",
    "GetItemRectMax": "return ImVec2(200.0f, 122.0f);",
    "GetItemRectSize": "return ImVec2(100.0f, 22.0f);",
    "GetWindowSize": "return ImVec2(640.0f, 480.0f);",
    "GetWindowPos": "return ImVec2(40.0f, 40.0f);",
    "GetWindowWidth": "return 640.0f;",
    "GetWindowHeight": "return 480.0f;",
    "GetColorU32": "return 0xFFFFFFFFu;",
    "GetColorU322": "return 0xFFFFFFFFu;",
    "GetColorU323": "return col;",
    "GetTime": "return Seconds();",
    "GetFrameCount": "return Frames();",
    "GetVersion": "return \"1.92.5\";",
    "GetKeyName": "return \"\";",
    "GetClipboardText": "return \"\";",
    "GetMainViewport": "return View();",
    "MemAlloc": "return std::malloc(size ? size : 1);",
    "MemFree": "std::free(ptr);",
    "CalcTextSize": "size_t n = 0; for (const char *w = text; w && (text_end == nullptr || w < text_end) && *w; ++w) if ((static_cast<unsigned char>(*w) & 0xC0) != 0x80) ++n; return ImVec2(static_cast<float>(n) * 7.0f, 13.0f);",
}


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "imgui.inc")
    with open(HEADER, "r", encoding="utf-8") as handle:
        text = handle.read()
    start = text.index("struct imgui_function_table_19250")
    body = text[text.index("{", start) + 1:text.index("};", start)]
    members = []
    for line in body.splitlines():
        m = MEMBER.match(line)
        if m:
            members.append((m.group(1).strip(), m.group(2), m.group(3).strip()))
    lines = ["#pragma warning(push)", "#pragma warning(disable : 4100)"]
    for index, (ret, name, params) in enumerate(members):
        if name in SPECIAL:
            body = SPECIAL[name]
        elif name in TRUE:
            body = "return true;"
        elif ret == "void":
            body = ""
        elif ret.endswith("&"):
            kind = ret[:-1].strip()
            if kind.startswith("const "):
                kind = kind[6:].strip()
            body = "return Store<%s>();" % kind
        else:
            body = "return {};"
        lines.append("inline %s Stub_%s(%s) { Hit(%d, \"%s\"); %s }" % (ret, name, params, index, name, body))
    lines.append("inline void Fill(imgui_function_table_19250 &t)")
    lines.append("{")
    for ret, name, params in members:
        lines.append("    t.%s = &Stub_%s;" % (name, name))
    lines.append("}")
    lines.append("inline constexpr int kImGuiFunctions = %d;" % len(members))
    lines.append("#pragma warning(pop)")
    with open(out, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    print("%d ImGui functions -> %s" % (len(members), os.path.abspath(out)))


if __name__ == "__main__":
    main()
