#include <zsdk/present.h>
#include <zsdk/test/addon.h>
#include <zsdk/test/check.h>
#include <zsdk/test/hardrules.h>

#include <set>
#include <string>
#include <vector>

namespace
{
    using check::That;

    struct Case
    {
        const char *word;
        const char *hit;
        const char *miss;
    };

    constexpr Case kCases[] = {
        { "Ride(", "void F() { Ride (nullptr, 0); }", "void F() { Rider(1); Override(2); int Ride = 3; }" },
        { "ProcessEvent", "void F(UObject *o) { o->ProcessEvent(fn, nullptr); }", "void F() { MyProcessEvent(1); OnProcessEvent(); }" },
        { "__try", "void F() { __try { G(); } __except (1) { } }", "void F() { int my__try = 0; }" },
        { "__except", "void F() { __try { G(); } __except (1) { } }", "void F() { int my__except = 0; }" },
        { "CreateThread", "HANDLE h = CreateThread(nullptr, 0, Work, nullptr, 0, nullptr);", "void F() { MyCreateThread(); OnCreateThread(); }" },
        { "std::thread", "std::thread worker([] {});", "void F() { std::this_thread::yield(); notstd::thread x; }" },
        { "SetWindowsHookEx", "HHOOK h = SetWindowsHookExW(WH_GETMESSAGE, Proc, nullptr, 0);", "void F() { MySetWindowsHookEx(); UnhookWindowsHookEx(h); }" },
        { "VirtualProtect", "VirtualProtect(p, 8, PAGE_READWRITE, &old);", "void F() { MyVirtualProtect(); VirtualQuery(p, &m, sizeof(m)); }" },
        { "WriteProcessMemory", "WriteProcessMemory(GetCurrentProcess(), p, b, 1, nullptr);", "void F() { ReadProcessMemory(h, p, b, 1, nullptr); MyWriteProcessMemory(); }" },
        { "_InterlockedOr", "_InterlockedOr8(&flag, 1);", "void F() { My_InterlockedOr(); _InterlockedAnd(&v, 1); }" },
        { "native_bindings::", "native_bindings::Ready();", "void F() { my_native_bindings::Ready(); native_bindings_catalog x; }" },
        { "detail::engine", "zsdk::detail::engine::Ready();", "void F() { mydetail::engine::Ready(); detail::world::Seen(); }" },
        { "detail/", "#include <zsdk/detail/ledger_raw.h>", "#include <zsdk/details/ledger_raw.h>" },
        { "zsdk::detail", "zsdk::detail::Halted();", "void F() { myzsdk::detail::Halted(); zsdk::guard::detail::ByteAt(x, 0); }" },
        { "CreateTimerQueueTimer", "CreateTimerQueueTimer(&t, nullptr, Cb, nullptr, 100, 100, 0);", "void F() { MyCreateTimerQueueTimer(); CreateTimerQueueTimerLater(); }" },
        { "QueueUserWorkItem", "QueueUserWorkItem(Cb, nullptr, 0);", "void F() { QueueUserWorkItems(); MyQueueUserWorkItem(); }" },
        { "TrySubmitThreadpoolCallback", "TrySubmitThreadpoolCallback(Cb, nullptr, nullptr);", "void F() { TrySubmitThreadpoolCallbackLater(); }" },
        { "_beginthread", "_beginthread(Work, 0, nullptr);", "void F() { _beginthreadex(nullptr, 0, Work, nullptr, 0, nullptr); my_beginthread(); }" },
        { "_beginthreadex", "_beginthreadex(nullptr, 0, Work, nullptr, 0, nullptr);", "void F() { _beginthread(Work, 0, nullptr); _beginthreadex_wrapper(); }" },
        { "std::async", "auto f = std::async(std::launch::async, Work);", "void F() { auto p = std::launch::async; mystd::async(); std::asynchronous x; }" },
        { "std::jthread", "std::jthread worker([] {});", "void F() { std::jthreads y; mystd::jthread z; }" },
        { "CreateRemoteThread", "CreateRemoteThread(p, nullptr, 0, Work, nullptr, 0, nullptr);", "CreateRemoteThreadEx(p, nullptr, 0, Work, nullptr, 0, nullptr, nullptr);" },
        { "CreateRemoteThreadEx", "CreateRemoteThreadEx(p, nullptr, 0, Work, nullptr, 0, nullptr, nullptr);", "CreateRemoteThread(p, nullptr, 0, Work, nullptr, 0, nullptr);" },
        { "SetTimer", "void F() { ::SetTimer(nullptr, 1, 100, nullptr); }", "void F() { MySetTimerThing(); SetTimerLater(); KillTimer(nullptr, 1); }" },
    };

    const hardrules::Rule *RuleOf(const std::string &word)
    {
        for (const hardrules::Rule &rule : hardrules::kRules)
            if (word == rule.word) return &rule;
        return nullptr;
    }

    std::string Names(const std::vector<addon::Declared> &found)
    {
        std::string out;
        for (const addon::Declared &d : found) out += (out.empty() ? "" : ",") + d.id + "=" + d.name;
        return out;
    }
}

int main()
{
    std::set<std::string> words;
    for (const hardrules::Rule &rule : hardrules::kRules)
    {
        That(words.insert(rule.word).second, std::string("one hard rule per word: ") + rule.word);
        const Case *found = nullptr;
        for (const Case &c : kCases)
            if (rule.word == std::string(c.word)) found = &c;
        That(found != nullptr, std::string("the hard rule ") + rule.word + " has a positive and a negative case");
        if (!found) continue;
        That(hardrules::Hits(found->hit, rule), std::string("the hard rule ") + rule.word + " catches: " + found->hit);
        That(!hardrules::Hits(found->miss, rule), std::string("the hard rule ") + rule.word + " lets a longer or a different name pass: " + found->miss);
        const bool threads = rule.kind != hardrules::Kind::Native;
        const std::string refusal = hardrules::Refusal(rule, "src/x.cpp");
        That(refusal.rfind(std::string("no ") + rule.word + " in src/x.cpp", 0) == 0, std::string("the refusal names the word and the file: ") + refusal);
        That((refusal.find("zsdk::job::Every") != std::string::npos) == threads, std::string("the refusal of ") + rule.word + (threads ? " points to zsdk::job::Every" : " carries no job hint"));
        const std::string notice = hardrules::Notice(rule, "src/x.cpp");
        That((notice.find("zsdk::job::Every") != std::string::npos) == threads, std::string("the warning of ") + rule.word + (threads ? " points to zsdk::job::Every" : " carries no job hint"));
    }
    for (const Case &c : kCases) That(words.count(c.word) == 1, std::string("the hard rule case ") + c.word + " belongs to a rule");
    for (const char *word : { "CreateTimerQueueTimer", "QueueUserWorkItem", "TrySubmitThreadpoolCallback", "_beginthread", "_beginthreadex", "std::async", "std::jthread", "CreateRemoteThread", "CreateRemoteThreadEx", "SetTimer" })
    {
        const hardrules::Rule *rule = RuleOf(word);
        That(rule && rule->kind == hardrules::Kind::Fresh, std::string("the timer or pool word ") + word + " is a new rule, a warning until zsdk::job::Every ships");
    }
    for (const char *word : { "CreateThread", "std::thread" })
    {
        const hardrules::Rule *rule = RuleOf(word);
        That(rule && rule->kind == hardrules::Kind::Worker, std::string("the thread word ") + word + " stays a failure with the job hint");
    }
    const hardrules::Rule *create = RuleOf("CreateThread");
    That(create && hardrules::Hits("PTP_WORK w = ::CreateThreadpoolWork(Cb, nullptr, nullptr);", *create), "the CreateThread prefix rule catches CreateThreadpoolWork");
    const hardrules::Rule *timer = RuleOf("SetTimer");
    That(timer && !hardrules::Hits("const char *s = \"SetTimer\"; int SetTimerCount = 0;\nint a = 1; /* SetTimer(nullptr, 1, 1, nullptr) */", *timer), "SetTimer inside a literal or a comment, or as part of a longer name, is not a use");
    const hardrules::Rule *jthread = RuleOf("std::jthread");
    const hardrules::Rule *thread = RuleOf("std::thread");
    That(jthread && thread && hardrules::Hits("std::jthread w([] {});", *jthread) && !hardrules::Hits("std::jthread w([] {});", *thread), "std::jthread has its own rule, the std::thread rule does not see it");

    const std::string sets =
        "#include \"guards.h\"\n"
        "constexpr const char *kText = \"a.text\";\n"
        "inline constexpr zsdk::guard::Guard kList[] = { { 0x41E14AE, \"48 8B 05\" } };\n"
        "inline constexpr zsdk::guard::Set kCoreSet{ \"addon.core\", &zsdk::guard::k61bc1e03, kList, 1, zsdk::guard::Need::ImageSize };\n"
        "constexpr guard::Set kEqual = { \"addon.equal\", nullptr, kList, 1, guard::Need::Bytes };\n"
        "constexpr zsdk::guard::Set kNamed{ kText, nullptr, kList, 1, zsdk::guard::Need::Bytes };\n"
        "const zsdk::guard::Set *const kSets[] = { &kCoreSet, &kEqual };\n"
        "extern const zsdk::guard::Set kOther;\n"
        "void Use(const zsdk::guard::Set &set);\n"
        "std::span<const zsdk::guard::Set *const> Sets();\n"
        "int a = 1; /* zsdk::guard::Set kComment{ \"in.comment\" }; */\n"
        "const char *b = \"zsdk::guard::Set kLiteral{ \\\"in.literal\\\" };\";\n"
        "constexpr Set kLoose{ \"not.a.guard\" };\n"
        "void G() { ini::Set(x); }\n";
    const std::vector<addon::Declared> found = addon::GuardSets(sets);
    That(Names(found) == "kCoreSet=addon.core,kEqual=addon.equal,kNamed=", "guard set declarations are found with their literal names, uses, pointers, comments and literals are not: " + Names(found));
    const std::string opened =
        "using namespace zsdk::guard;\n"
        "constexpr Set kBare{ \"bare.set\", nullptr, kList, 1, Need::Bytes };\n"
        "constexpr Set const kEast{ \"east.set\", nullptr, kList, 1, Need::Bytes };\n"
        "void G() { ini::Set(x); }\n";
    const std::vector<addon::Declared> bare = addon::GuardSets(opened);
    That(Names(bare) == "kBare=bare.set,kEast=east.set", "after using namespace zsdk::guard a bare Set declaration counts: " + Names(bare));
    That(addon::GuardSets(std::string("constexpr Set kLoose{ \"not.a.guard\" };\n")).empty(), "a bare Set without a using of zsdk::guard is not a guard set");
    const std::string designated =
        "inline constexpr zsdk::guard::Set kFirst{ .name = \"first.set\", .build = &zsdk::guard::k61bc1e03, .guards = kList, .count = 1 };\n"
        "inline constexpr zsdk::guard::Set kLater{ .build = nullptr, .guards = kList, .count = 1, .need = zsdk::guard::Need::Bytes, .name = \"later.set\" };\n"
        "inline constexpr zsdk::guard::Set kInner{ .build = Make({ .name = \"not.this\" }), .name = \"inner.set\" };\n"
        "inline constexpr zsdk::guard::Set kNone{ .guards = kList, .count = 1 };\n";
    const std::vector<addon::Declared> marked = addon::GuardSets(designated);
    That(Names(marked) == "kFirst=first.set,kLater=later.set,kInner=inner.set,kNone=", "a designated .name literal names the set wherever it stands, a .name inside a nested braced value does not: " + Names(marked));
    const std::string ticks =
        "bool A(uint64_t now) { return now - shown > 500; }\n"
        "bool B(uint64_t t) { return t - w.since > 3000; }\n"
        "bool C(uint64_t t) { return t - grip->since > 3000 && now - (at) > 1; }\n"
        "double D() { return static_cast<double>(GetTickCount64() - since); }\n"
        "void E(uint64_t now) { w.since = now; now -= 5; x = now - 1; y = w->since; z = zsdk::Age(now, w.since); }\n"
        "void F(uint64_t now) { known - x; nowMs - y; q = a - sinceStart; r = b - w.sinceTime; }\n";
    const std::vector<scan::Tick> raw = scan::Ticks(scan::Lex(ticks).code);
    std::string lines;
    for (const scan::Tick &one : raw) lines += (lines.empty() ? "" : ",") + std::to_string(one.line);
    That(lines == "1,2,3,4", "raw tick differences over now and since are found once per line, Age, -=, a number, -> and longer names are not: " + lines);
    That(raw.size() == 4 && raw[0].text == "now - shown" && raw[1].text == "- w.since", "a found tick difference carries its text for the warning");
    That(scan::Ticks(scan::Lex("const char *s = \"now - since\"; // now - x\n").code).empty(), "a tick difference in a literal or a comment is not code");
    return check::Finish("scan");
}
