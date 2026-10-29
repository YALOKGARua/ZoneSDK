#include <zsdk/present.h>
#include <zsdk/test/addon.h>
#include <zsdk/test/check.h>

#include <regex>
#include <sstream>
#include <string>

int main()
{
    using check::That;
    const addon::Tree t = addon::Load();
    That(std::filesystem::exists(t.root / "tests" / "zsdk.cfg"), "tests/zsdk.cfg exists");
    That(t.cfg.count("name") && t.cfg.at("name") == scan::Env("ZSDK_ADDON_NAME"), "zsdk.cfg names the add-on that is built");
    const std::string rule = t.cfg.count("comments") ? t.cfg.at("comments") : std::string();
    That(rule.empty() || rule == "forbid" || rule == "allow", "zsdk.cfg comments= is forbid or allow: " + rule);
    const bool forbid = rule == "forbid";
    std::printf("style: comments are %s (zsdk.cfg comments=%s)\n", forbid ? "forbidden" : "allowed", forbid ? "forbid" : rule.empty() ? "<unset>, forbid turns the check on" : rule.c_str());
    std::vector<std::filesystem::path> files = t.present;
    files.insert(files.end(), t.overlay.begin(), t.overlay.end());
    files.push_back(t.entry);
    files.insert(files.end(), t.tests.begin(), t.tests.end());
    for (const auto &file : scan::Files(t.root / "src", { ".rc" }, false)) files.push_back(file);
    for (const auto &file : files)
    {
        const scan::Lexed lexed = scan::Lex(scan::Body(file));
        if (forbid) That(lexed.comments == 0, "no comments in " + addon::Rel(t, file) + (lexed.comments ? " (line " + std::to_string(lexed.firstComment) + ")" : std::string()));
        That(lexed.closed, "every literal is closed in " + addon::Rel(t, file));
    }
    const std::string make = scan::Body(t.root / "build" / "make.ps1");
    That(!make.empty(), "build/make.ps1 exists");
    std::istringstream lines(make);
    std::string line;
    int number = 0;
    while (std::getline(lines, line))
    {
        ++number;
        const size_t at = line.find_first_not_of(" \t");
        if (at == std::string::npos) continue;
        if (forbid) That(line[at] != '#' && line.find("<#") == std::string::npos, "no comments in make.ps1 line " + std::to_string(number));
        bool ascii = true;
        for (const char c : line) ascii = ascii && static_cast<unsigned char>(c) < 0x80;
        That(ascii, "make.ps1 is pure ASCII, line " + std::to_string(number));
    }
    That(!std::regex_search(make, std::regex("(^|[^\\\\A-Za-z0-9_])[A-Z]" ":" "[\\\\/]+[A-Za-z_]")), "make.ps1 has no absolute drive path");
    for (const char *word : { "Steam" "Library", "Win64", "-Deploy", "Deploy", "GameDir", "Copy-Item" })
        That(make.find(word) == std::string::npos, std::string("build/make.ps1 deploys nothing: no ") + word);
    That(make.find("zsdk.psm1") != std::string::npos && make.find("Invoke-ZsdkBuild") != std::string::npos && make.find("zsdk.lock") != std::string::npos,
         "make.ps1 builds through the pinned ZoneSDK module");
    const std::vector<std::string> banned = scan::List(t.cfg.count("banned") ? t.cfg.at("banned") : std::string());
    for (const auto &file : files)
    {
        if (file.parent_path().filename() == "tests") continue;
        const scan::Lexed lexed = scan::Lex(scan::Body(file));
        for (const std::string &name : banned)
        {
            bool seen = lexed.words.count(name) != 0;
            for (const std::string &literal : lexed.literals) seen = seen || literal.find(name) != std::string::npos;
            That(!seen, "forbidden call " + name + " is absent from " + addon::Rel(t, file));
        }
    }
    for (const auto &file : files)
    {
        if (file.parent_path().filename() == "tests") continue;
        for (const scan::Tick &raw : scan::Ticks(scan::Lex(scan::Body(file)).code))
            check::Warn("style: a raw tick difference '" + raw.text + "' in " + addon::Rel(t, file) + " line " + std::to_string(raw.line) + ", zsdk::Age(now, at) gives 0 when the stamp is later than now");
    }
    return check::Finish("zsdk-style");
}
