#include <zsdk/present.h>
#include <zsdk/test/check.h>
#include <zsdk/test/scan.h>

#include <map>
#include <regex>
#include <string>

namespace
{
    using check::That;

    const std::map<std::string, int> kLayers = {
        { "version.h", 0 }, { "base.h", 0 }, { "policy.h", 0 }, { "text.h", 0 }, { "log.h", 0 }, { "lang.h", 0 }, { "paths.h", 0 }, { "keys.h", 0 }, { "math.h", 0 }, { "glyphs.h", 0 },
        { "registry.h", 0 }, { "ini.h", 0 }, { "ledger.h", 0 }, { "toast.h", 0 }, { "guard.h", 1 }, { "exe.h", 1 }, { "ue.h", 2 }, { "census.h", 2 }, { "verbs.h", 3 },
        { "call.h", 3 }, { "mem.h", 3 }, { "world.h", 3 }, { "thread.h", 3 }, { "input.h", 5 }, { "app.h", 5 }, { "hooks.h", 5 }, { "zsdk.h", 6 }, { "present.h", 6 },
        { "shell.h", 6 }, { "paint.h", 5 }, { "overlay.h", 6 }, { "job.h", 3 },
        { "report.h", 0 },
    };
}

int main()
{
    const std::filesystem::path sdk = scan::SdkRoot();
    That(std::filesystem::exists(sdk / "include" / "zsdk" / "zsdk.h"), "ZSDK_HOME points at the SDK");
    const std::regex include("#include\\s*[<\"](?:zsdk/)?([a-z_]+\\.h)[>\"]");
    int headers = 0;
    for (const auto &file : scan::Files(sdk / "include" / "zsdk", { ".h" }, false))
    {
        ++headers;
        const std::string name = file.filename().string();
        That(kLayers.count(name) == 1, "every public header has a layer: " + name);
        if (!kLayers.count(name)) continue;
        const std::string body = scan::Body(file);
        for (std::sregex_iterator it(body.begin(), body.end(), include), end; it != end; ++it)
        {
            const std::string used = (*it)[1].str();
            if (!kLayers.count(used)) continue;
            That(kLayers.at(used) <= kLayers.at(name), name + " may include " + used);
        }
        That(body.find("detail/") == std::string::npos, "public headers do not reach detail: " + name);
    }
    That(headers == static_cast<int>(kLayers.size()), "the layer table covers the public headers, " + std::to_string(headers));
    for (const char *name : { "paint.h", "overlay.h" })
        That(scan::Body(sdk / "include" / "zsdk" / name).find("#if defined(ZSDK_PRESENT_UNIT)\n#error") != std::string::npos, std::string(name) + " refuses present units");
    That(scan::Body(sdk / "include" / "zsdk" / "present.h").find("#if defined(IMGUI_VERSION)") != std::string::npos, "present.h refuses a visible ImGui");
    for (const auto &file : scan::Files(sdk / "src" / "present", { ".cpp" }, false))
    {
        const scan::Lexed lexed = scan::Lex(scan::Body(file));
        That(lexed.code.find("ImGui::") == std::string::npos && lexed.code.find("imgui.h") == std::string::npos, "no ImGui in the present unit " + file.filename().string());
        That(lexed.code.find("overlay.h") == std::string::npos && lexed.code.find("paint.h") == std::string::npos, "no overlay header in " + file.filename().string());
        That(lexed.code.find("#include <zsdk/present.h>") == 0, "a present unit starts with zsdk/present.h: " + file.filename().string());
    }
    for (const auto &file : scan::Files(sdk / "src" / "overlay", { ".cpp" }, false))
        That(scan::Body(file).find("#include <zsdk/overlay.h>") == 0, "an overlay unit starts with zsdk/overlay.h: " + file.filename().string());
    const std::string shell = scan::Body(sdk / "src" / "entry" / "shell.cpp");
    That(shell.find("#include <imgui.h>\n#include <reshade.hpp>") == 0, "the entry unit includes imgui.h before reshade.hpp");
    That(shell.find("static_assert(RESHADE_API_VERSION == ZSDK_RESHADE_API && IMGUI_VERSION_NUM == ZSDK_IMGUI_NUM") != std::string::npos, "the entry unit pins the header versions");
    int rides = 0;
    for (const auto &file : scan::Files(sdk / "src", { ".cpp" }, true))
        rides += static_cast<int>(scan::Count(scan::Lex(scan::Body(file)).code, "core::Ride("));
    That(rides == 1, "exactly one call site drives native code, " + std::to_string(rides));
    const std::string call = scan::Lex(scan::Body(sdk / "src" / "present" / "call.cpp")).code;
    That(call.find("core::Ride(") != std::string::npos && call.find("detail::Gate(key, marked)") < call.find("core::Ride("), "the choke point gates before it rides");
    const std::string engine = scan::Lex(scan::Body(sdk / "src" / "present" / "engine.cpp")).code;
    That(engine.find("if (!thread::InPump())") != std::string::npos, "the engine refuses rides and wiring outside the pump");
    That(scan::Lex(scan::Body(sdk / "include" / "zsdk" / "ledger.h")).code.find("raw") == std::string::npos, "the public ledger header has no raw marker calls");
    That(scan::Body(sdk / "include" / "zsdk" / "test" / "ledger_raw.h").find("#if !defined(ZSDK_TESTS)\n#error") != std::string::npos, "the raw ledger header refuses builds without ZSDK_TESTS");
    const std::string shellCode = scan::Lex(shell).code;
    const size_t detach = shellCode.find("DLL_PROCESS_DETACH");
    That(detach != std::string::npos && shellCode.find("detail::engine::Dusk()", detach) < shellCode.find("detail::thread::Stop()", detach), "the detach path closes the engine before it stops the threads");
    const std::string thread = scan::Lex(scan::Body(sdk / "src" / "present" / "thread.cpp")).code;
    const size_t fenced = thread.find("bool Fenced(std::string &where)");
    That(fenced != std::string::npos && thread.find("detail::Fend([] { Step(); })", fenced) != std::string::npos && thread.find("const bool clean = Fenced(where);") != std::string::npos,
         "the pump runs its step inside the SDK fence");
    const std::string job = scan::Lex(scan::Body(sdk / "src" / "present" / "job.cpp")).code;
    const std::string sift = job.substr(job.find("int Sift(") == std::string::npos ? job.size() : job.find("int Sift("));
    That(!sift.empty() && sift.find("record->ExceptionCode == kCppException) return EXCEPTION_CONTINUE_SEARCH;") != std::string::npos && sift.find("__except (Sift(GetExceptionInformation(), at, from))") != std::string::npos,
         "the fence filter lets C++ exceptions through to their catch and stops structured exceptions at its __except");
    const size_t fend = job.find("Fended Fend(void (*work)(void *), void *context)");
    const size_t walled = fend == std::string::npos ? fend : job.find("out.code = Walled(", fend);
    const size_t reset = walled == std::string::npos ? walled : job.find("if (out.code == EXCEPTION_STACK_OVERFLOW) _resetstkoflw();", walled);
    That(fend != std::string::npos && walled != std::string::npos && reset != std::string::npos, "the fence restores the stack guard page after it stopped an overflow");
    for (const auto &file : scan::Files(sdk / "src", { ".cpp", ".h", ".inc" }, true))
        That(scan::Lex(scan::Body(file)).code.find("_set_se_translator") == std::string::npos, "no structured-exception translator in the SDK, it would throw on an exhausted stack: " + file.filename().string());
    That(thread.find("Wiring(now, &detail::engine::Wire)") != std::string::npos && scan::Count(thread, "engine::Wire)") == 1 && thread.find("const Wired done = WireStep(wire);") != std::string::npos &&
             thread.find("ledger::raw::Arm(kWireKey)") != std::string::npos,
         "wiring runs behind its crash marker: the step reaches engine::Wire only through Wiring, which calls WireStep");
    int wires = 0;
    for (const scan::Function &f : scan::Functions(engine))
    {
        if (f.name != "Wire") continue;
        ++wires;
        That(f.body.find("Restock()") == std::string::npos && f.body.find("Survey()") == std::string::npos && f.body.find("Want()") != std::string::npos,
             "wiring on the game thread never restocks the function book or walks the memory map, a stale book is wanted from the census thread");
    }
    That(wires == 1, "engine.cpp has one Wire, " + std::to_string(wires));
    That(call.find("policy.strictMarks") != std::string::npos && call.find("verbs::Ready(verb)") != std::string::npos, "the choke point refuses unverified verbs and unwritten markers");
    return check::Finish("layers");
}
