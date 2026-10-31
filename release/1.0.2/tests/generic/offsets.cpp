#include <zsdk/present.h>
#include <zsdk/test/addon.h>
#include <zsdk/test/check.h>

#include <cstdio>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

int main()
{
    using check::That;
    const addon::Tree t = addon::Load();
    const std::regex bare("constexpr\\s+(?:uintptr_t|intptr_t|int32_t|uint32_t|int64_t|uint64_t|int|size_t|ptrdiff_t|uint16_t|uint8_t|DWORD|ULONGLONG)\\s+(k\\w*(?:At|Rva|Stride|Size|Off|Offset|Shift))\\s*=\\s*(?:0x[0-9A-Fa-f]+|\\d+)");
    const std::regex inlined("\\b(?:Read|Copy|Poke|PokeType|Grab|Stamp|Write)\\s*\\((?:[^,;()]|\\([^()]*\\))*?[+-]\\s*(0x[0-9A-Fa-f]+|\\d+)u?\\s*[,)]");
    const std::regex hex("[+-]\\s*0x[0-9A-Fa-f]+u?\\b");
    const std::regex named("Field::Unproven\\(\\s*\"([^\"]+)\"");
    std::map<std::string, std::string> listed;
    std::istringstream lines(scan::Body(t.root / "tests" / "unproven-fields.txt"));
    std::string line;
    while (std::getline(lines, line))
    {
        const size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        listed[scan::Trim(line.substr(0, colon))] = scan::Trim(line.substr(colon + 1));
    }
    std::set<std::string> used;
    int typed = 0, rip = 0, raw = 0;
    std::vector<std::filesystem::path> files = t.present;
    files.insert(files.end(), t.overlay.begin(), t.overlay.end());
    for (const auto &file : files)
    {
        const std::string body = scan::Body(file);
        const std::string code = scan::Lex(body).code;
        const std::string rel = addon::Rel(t, file);
        for (std::sregex_iterator it(code.begin(), code.end(), bare), end; it != end; ++it)
            That(false, "a raw layout constant outside zsdk::guard::Field: " + (*it)[1].str() + " in " + rel);
        for (std::sregex_iterator it(code.begin(), code.end(), inlined), end; it != end; ++it)
            That(false, "a literal offset inside a memory access: " + (*it)[0].str() + " in " + rel);
        for (std::sregex_iterator it(code.begin(), code.end(), hex), end; it != end; ++it)
            That(false, "hex literal arithmetic, an offset belongs in a Field: " + (*it)[0].str() + " in " + rel);
        for (std::sregex_iterator it(body.begin(), body.end(), named), end; it != end; ++it) used.insert((*it)[1].str());
        typed += static_cast<int>(scan::Count(code, "Field k"));
        rip += static_cast<int>(scan::Count(code, "Field::Rip("));
        raw += static_cast<int>(scan::Count(code, ".Raw()"));
    }
    That(true, "no raw layout constant or literal offset outside a Field");
    for (const std::string &name : used) That(listed.count(name) == 1 && !listed.at(name).empty(), "the unproven field " + name + " is listed with a reason in tests/unproven-fields.txt");
    for (const auto &[name, reason] : listed) That(used.count(name) == 1, "the listed unproven field " + name + " still exists");
    std::printf("offsets: %d typed fields, %d RIP proofs, %d unproven and listed, %d raw uses behind the add-on's own gates\n", typed, rip, static_cast<int>(used.size()), raw);
    return check::Finish("zsdk-offsets");
}
