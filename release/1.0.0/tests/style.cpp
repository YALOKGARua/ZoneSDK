#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include <regex>
#include <sstream>
#include <string>

int main()
{
    using check::That;
    const std::filesystem::path sdk = scan::SdkRoot();
    const scan::Lexed a = scan::Lex("int a = 1; const char *b = \"http://x\"; char c = '/'; auto d = R\"(/* no */)\"; int e = 1'000;");
    That(a.comments == 0 && a.closed && a.literals.size() == 2, "comment markers inside literals are not comments");
    const scan::Lexed b = scan::Lex("int a = 1; // note\nint c = 2; /* block */");
    That(b.comments == 2 && b.firstComment == 1, "line and block comments are found");
    int files = 0;
    for (const char *folder : { "include", "src", "tests", "template", "tools" })
        for (const auto &file : scan::Files(sdk / folder, { ".cpp", ".h", ".inc", ".rc" }, true))
        {
            ++files;
            const scan::Lexed lexed = scan::Lex(scan::Body(file));
            That(lexed.comments == 0, "no comments in " + scan::Text(file) + (lexed.comments ? " (line " + std::to_string(lexed.firstComment) + ")" : std::string()));
            That(lexed.closed, "every literal is closed in " + scan::Text(file));
        }
    That(files >= 60, "the style scan sees the SDK sources, " + std::to_string(files));
    int scripts = 0;
    for (const char *folder : { "build", "tools", "template" })
        for (const auto &file : scan::Files(sdk / folder, { ".ps1", ".psm1", ".py" }, true))
        {
            ++scripts;
            std::istringstream lines(scan::Body(file));
            std::string line;
            int number = 0;
            bool clean = true;
            while (std::getline(lines, line))
            {
                ++number;
                const size_t at = line.find_first_not_of(" \t");
                if (at == std::string::npos) continue;
                if (line[at] == '#' || line.find("<#") != std::string::npos) clean = false;
            }
            That(clean, "no comments in " + scan::Text(file));
            const std::string rel = scan::Generic(std::filesystem::relative(file, sdk));
            if (rel == "tools/doctor.ps1" || rel == "tools/check-scaffold.ps1" || rel == "template/ZoneTemplate/build/pack.ps1") continue;
            const std::string body = scan::Body(file);
            That(body.find("Steam" "Library") == std::string::npos && body.find("-Deploy") == std::string::npos && body.find("Binaries\\Win64") == std::string::npos,
                 "no deploy path in " + scan::Text(file));
        }
    That(scripts >= 4, "the script scan sees the build scripts, " + std::to_string(scripts));
    const std::regex drive("(^|[^\\\\A-Za-z0-9_])[A-Z]" ":" "[\\\\/]+[A-Za-z_]");
    int shipped = 0;
    for (const char *folder : { "include", "src", "tests", "build", "tools", "template", "inventory\\tools" })
        for (const auto &file : scan::Files(sdk / folder, { ".cpp", ".h", ".inc", ".ps1", ".psm1", ".py", ".cfg", ".lock" }, true))
        {
            const std::string name = scan::Text(file);
            if (name.find("\\obj\\") != std::string::npos || name.find("\\bin\\") != std::string::npos || name.find("\\sandbox\\") != std::string::npos) continue;
            if (file == sdk / "build" / "local.cfg") continue;
            ++shipped;
            That(!std::regex_search(scan::Body(file), drive), "no absolute drive path in " + name);
        }
    That(shipped >= 80, "the drive path scan sees the shipped files, " + std::to_string(shipped));
    return check::Finish("style");
}
