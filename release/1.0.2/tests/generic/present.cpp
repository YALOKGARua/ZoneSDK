#include <zsdk/present.h>
#include <zsdk/test/addon.h>
#include <zsdk/test/check.h>

#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace
{
    using check::That;

    std::set<std::string> Defined(const std::vector<std::filesystem::path> &files)
    {
        std::set<std::string> out;
        for (const auto &file : files)
            for (const scan::Function &f : scan::Functions(scan::Lex(scan::Body(file)).code)) out.insert(f.name);
        return out;
    }

    std::string Last(const std::string &name)
    {
        const size_t cut = name.rfind("::");
        return cut == std::string::npos ? name : name.substr(cut + 2);
    }
}

int main()
{
    const addon::Tree t = addon::Load();
    That(!t.present.empty() && !t.entry.empty(), "the add-on tree has present units and src/entry.cpp");
    const char *banned[] = { "ImGui::", "ImDrawList", "imgui.h", "reshade.hpp", "zsdk/overlay.h", "zsdk/paint.h", "paint::", "tab::", "overlay::", "IM_COL32" };
    for (const auto &file : t.present)
    {
        const std::string code = scan::Lex(scan::Body(file)).code;
        for (const char *word : banned) That(code.find(word) == std::string::npos, std::string("no ") + word + " in the present unit " + addon::Rel(t, file));
    }
    std::vector<std::filesystem::path> overlayFiles = t.overlay;
    for (const auto &file : scan::Files(t.sdk / "src" / "overlay", { ".cpp" }, false)) overlayFiles.push_back(file);
    overlayFiles.push_back(t.sdk / "src" / "entry" / "shell.cpp");
    std::vector<std::filesystem::path> presentFiles = t.present;
    for (const auto &file : scan::Files(t.sdk / "src" / "present", { ".cpp" }, false)) presentFiles.push_back(file);
    const std::set<std::string> overlayNames = Defined(overlayFiles);
    const std::set<std::string> presentNames = Defined(presentFiles);
    std::set<std::string> only;
    for (const std::string &name : overlayNames)
        if (!presentNames.count(name)) only.insert(name);
    That(only.count("Page") || only.count("Draw") || only.count("Label"), "the overlay-only call set is not empty, " + std::to_string(only.size()));
    const std::set<std::string> allowed = addon::Allowed(t, "present.allow");
    for (const auto &file : presentFiles)
    {
        const std::set<std::string> calls = scan::Calls(scan::Lex(scan::Body(file)).code);
        for (const std::string &name : calls)
            if (only.count(name) && !allowed.count(name)) That(false, "the present unit " + scan::Text(file.filename()) + " calls the overlay-only function " + name);
    }
    That(true, "present units call no overlay-only function by name");
    const std::filesystem::path symbols = scan::EnvPath("ZSDK_SYMBOLS");
    That(!symbols.empty() && std::filesystem::exists(symbols), "the build wrote the object symbol table: " + scan::Text(symbols));
    const std::map<std::string, std::string> table = scan::Config(symbols);
    That(table.count("present_undefined") && std::atoi(table.at("present_undefined").c_str()) > 0, "the present objects reference external symbols");
    That(table.count("overlay_only") && std::atoi(table.at("overlay_only").c_str()) > 0, "the overlay objects define symbols that no present object defines");
    std::istringstream rows(scan::Body(symbols));
    std::string row;
    int violations = 0;
    while (std::getline(rows, row))
        if (row.rfind("violation=", 0) == 0)
        {
            ++violations;
            That(false, "a present object uses a symbol defined only in overlay objects: " + row.substr(10));
        }
    That(violations == 0, "no present object references an overlay-only symbol (mangled names, " + std::to_string(violations) + " found)");
    const std::set<std::string> ownPresent = Defined(t.present);
    const std::set<std::string> ownOverlay = Defined(t.overlay);
    const std::set<std::string> ownEntry = Defined({ t.entry });
    const auto where = [&](const std::string &name) {
        if (ownEntry.count(name)) return std::string(", which is defined in src/entry.cpp; entry.cpp only wires hooks, so move it into a present unit");
        if (ownOverlay.count(name) && ownPresent.count(name)) return std::string(", which is defined both in a present unit and in src/overlay; give the two functions different names");
        if (ownOverlay.count(name)) return std::string(", which is defined in src/overlay; only overlay and page are overlay functions");
        if (!ownPresent.count(name)) return std::string(", which no unit of the add-on defines under that name");
        return std::string();
    };
    const std::string units = " (present units are src/*.cpp and their subfolders, except src/entry.cpp and src/overlay)";
    const std::string entry = scan::Lex(scan::Body(t.entry)).code;
    That(entry.find("zsdk::shell::Main(") != std::string::npos && entry.find("ZSDK_ADDON(") != std::string::npos, "entry.cpp registers through zsdk::shell::Main");
    for (const char *word : { "ImGui::", "imgui.h", "ImDrawList", "reshade.hpp" }) That(entry.find(word) == std::string::npos, std::string("no ") + word + " in entry.cpp");
    const std::regex lambda("\\.\\s*(\\w+)\\s*=\\s*\\[");
    for (std::sregex_iterator it(entry.begin(), entry.end(), lambda), end; it != end; ++it) That(false, "the hook " + (*it)[1].str() + " is wired to a lambda; hooks are named functions");
    const std::regex any("\\b[A-Za-z_]\\w*\\.(\\w+)\\s*=\\s*([^;]+);");
    const std::set<std::string> presentHooks = { "boot", "refresh", "prime", "advance", "step", "hot", "publish", "turn", "order", "orderName", "quit", "census", "complete", "frame", "opened", "stop", "words", "tone", "pipelineInit", "pipelineBind", "pipelineDestroy", "deviceDestroy", "report" };
    const std::set<std::string> overlayHooks = { "overlay", "page" };
    int present = 0, pages = 0;
    for (std::sregex_iterator it(entry.begin(), entry.end(), any), end; it != end; ++it)
    {
        const std::string hook = (*it)[1].str();
        std::string target = scan::Trim((*it)[2].str());
        if (!target.empty() && target[0] == '&') target = scan::Trim(target.substr(1));
        const bool named = std::regex_match(target, std::regex("[A-Za-z_][A-Za-z_0-9:]*"));
        That(named, "the hook " + hook + " is wired to a named function: " + target);
        if (!named) continue;
        const std::string name = Last(target);
        if (overlayHooks.count(hook))
        {
            ++pages;
            That(ownOverlay.count(name) == 1 && ownPresent.count(name) == 0, "the hook " + hook + " is an overlay function defined in src/overlay: " + name + (ownEntry.count(name) ? ", which is defined in src/entry.cpp" : ownPresent.count(name) ? ", which a present unit defines too" : ownOverlay.count(name) ? "" : ", which no overlay unit defines"));
        }
        else
        {
            That(presentHooks.count(hook) == 1, "the hook " + hook + " is a known ZoneSDK hook");
            if (hook == "words" || hook == "tone")
            {
                That(ownOverlay.count(name) + ownPresent.count(name) == 1, "the toast hook " + hook + " is one add-on function: " + name);
                continue;
            }
            ++present;
            That(ownPresent.count(name) == 1 && ownOverlay.count(name) == 0, "the hook " + hook + " is a function of a present unit" + units + ": " + name + where(name));
        }
    }
    That(present >= 2, "entry.cpp wires the present hooks, " + std::to_string(present));
    That(pages >= 1, "entry.cpp wires the tab page");
    return check::Finish("zsdk-present");
}
