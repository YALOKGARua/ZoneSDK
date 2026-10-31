import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
HEADERS = [os.path.join(HERE, "..", "..", "extern", "reshade", name) for name in ("reshade_api_device.hpp", "reshade_api.hpp")]
WANTED = ("api_object", "device_object", "effect_runtime")
DECL = re.compile(r"^\s*virtual\s+(.+?)\b(\w+)\s*\((.*)\)\s*(const)?\s*=\s*0\s*;\s*$")
SPECIAL = {
    "is_key_down": "return keys.Down(keycode);",
    "is_key_pressed": "return keys.Pressed(keycode);",
    "is_key_released": "return false;",
    "is_mouse_button_down": "return false;",
    "is_mouse_button_pressed": "return false;",
    "is_mouse_button_released": "return false;",
    "get_mouse_cursor_position": "if (out_x) *out_x = 0; if (out_y) *out_y = 0; if (out_wheel_delta) *out_wheel_delta = 0;",
    "get_device": "return nullptr;",
}


def blocks(text):
    for name in WANTED:
        match = re.search(r"struct\s+__declspec\(novtable\)\s+" + name + r"\b[^{;]*\{", text)
        if not match:
            continue
        depth = 0
        start = match.end() - 1
        for i in range(start, len(text)):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    yield name, text[start:i]
                    break


def strip_defaults(params):
    out = []
    depth = 0
    piece = ""
    for c in params:
        if c in "(<":
            depth += 1
        elif c in ")>":
            depth -= 1
        if c == "," and depth == 0:
            out.append(piece)
            piece = ""
        else:
            piece += c
    if piece.strip():
        out.append(piece)
    cleaned = []
    for p in out:
        cleaned.append(p.split("=")[0].strip())
    return ", ".join(cleaned)


def split(params):
    out = []
    depth = 0
    piece = ""
    for c in params:
        if c in "(<":
            depth += 1
        elif c in ")>":
            depth -= 1
        if c == "," and depth == 0:
            out.append(piece)
            piece = ""
        else:
            piece += c
    if piece.strip():
        out.append(piece)
    return out


def names(params):
    out = []
    for p in split(params):
        p = p.strip()
        if not p:
            continue
        pointer = re.search(r"\(\s*\*\s*(\w+)\s*\)", p)
        if pointer:
            out.append(pointer.group(1))
            continue
        m = re.search(r"(\w+)\s*$", re.sub(r"\[[^\]]*\]", "", p))
        if m:
            out.append(m.group(1))
    return out


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "runtime.inc")
    seen = []
    for header in HEADERS:
        with open(header, "r", encoding="utf-8") as handle:
            text = handle.read()
        for name, body in blocks(text):
            for line in body.splitlines():
                m = DECL.match(line)
                if m:
                    seen.append((m.group(1).strip(), m.group(2), strip_defaults(m.group(3)), bool(m.group(4))))
    lines = []
    for index, (ret, name, params, const) in enumerate(seen):
        tail = " const" if const else ""
        used = names(params)
        if name in SPECIAL:
            body = SPECIAL[name]
            silence = "".join("(void)%s; " % n for n in used if n not in body)
        else:
            silence = "".join("(void)%s; " % n for n in used)
            body = "Trap(%d, \"%s\");" % (index, name)
            if ret != "void":
                body += " return {};"
        lines.append("    %s %s(%s)%s override { %s%s }" % (ret, name, params, tail, silence, body))
    with open(out, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    print("%d virtual functions -> %s" % (len(seen), os.path.abspath(out)))


if __name__ == "__main__":
    main()
