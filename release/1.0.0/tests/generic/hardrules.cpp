#include <zsdk/present.h>
#include <zsdk/test/addon.h>
#include <zsdk/test/check.h>
#include <zsdk/test/hardrules.h>

#include <windows.h>

#include <bcrypt.h>

#include <cstdio>
#include <string>
#include <vector>

namespace
{
    using check::That;

    std::string Sha(const std::string &body)
    {
        BCRYPT_ALG_HANDLE alg = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        std::string out;
        if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) && BCRYPT_SUCCESS(BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0)))
        {
            uint8_t digest[32]{};
            if (BCRYPT_SUCCESS(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(body.data())), static_cast<ULONG>(body.size()), 0)) &&
                BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0)))
            {
                char hex[65]{};
                for (int i = 0; i < 32; ++i) std::snprintf(hex + i * 2, 3, "%02X", digest[i]);
                out = hex;
            }
        }
        if (hash) BCryptDestroyHash(hash);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
        return out;
    }

#if __has_include(<zsdk/job.h>)
    constexpr bool kJobs = true;
#else
    constexpr bool kJobs = false;
#endif
}

int main()
{
    const addon::Tree t = addon::Load();
    std::vector<std::filesystem::path> all = t.present;
    all.insert(all.end(), t.overlay.begin(), t.overlay.end());
    all.push_back(t.entry);
    const std::set<std::string> facade = addon::Allowed(t, "facade");
    const std::set<std::string> memory = addon::Allowed(t, "memory");
    if (!kJobs) std::printf("hardrules: this ZoneSDK has no zsdk/job.h yet, so the timer and thread-pool words are warnings, not failures\n");
    int addons = 0, mains = 0, fresh = 0;
    for (const auto &file : all)
    {
        const std::string rel = addon::Rel(t, file);
        const std::string code = scan::Lex(scan::Body(file)).code;
        const std::vector<std::string> tokens = scan::Tokens(code);
        for (const hardrules::Rule &rule : hardrules::kRules)
        {
            const bool seen = scan::Uses(tokens, rule.pattern);
            if (rule.kind == hardrules::Kind::Fresh && !kJobs)
            {
                if (seen)
                {
                    ++fresh;
                    check::Warn(hardrules::Notice(rule, rel));
                }
                continue;
            }
            That(!seen, hardrules::Refusal(rule, rel));
        }
        const std::string body = scan::Body(file);
        if (!facade.count(rel)) That(code.find("ledger::raw") == std::string::npos && code.find("ledger_raw.h") == std::string::npos, "only the listed test facade reaches the raw ledger: " + rel);
        else
        {
            std::vector<std::pair<size_t, size_t>> blocks;
            for (size_t gate = body.find("#if defined(ZSDK_TESTS)"); gate != std::string::npos; gate = body.find("#if defined(ZSDK_TESTS)", gate + 1))
            {
                const size_t close = body.find("#endif", gate);
                blocks.emplace_back(gate, close == std::string::npos ? body.size() : close);
            }
            bool inside = true;
            for (const char *word : { "ledger::raw", "ledger_raw.h" })
                for (size_t use = body.find(word); use != std::string::npos; use = body.find(word, use + 1))
                {
                    bool covered = false;
                    for (const auto &[from, to] : blocks) covered = covered || (use > from && use < to);
                    inside = inside && covered;
                }
            That(inside, "the facade reaches the raw ledger only inside #if defined(ZSDK_TESTS) blocks: " + rel);
        }
        if (!memory.count(rel)) That(code.find("mem::Poke") == std::string::npos && code.find("mem::Stamp") == std::string::npos, "raw game memory writes outside a crash window only in a listed memory facade: " + rel);
        addons += static_cast<int>(scan::Count(code, "ZSDK_ADDON("));
        mains += static_cast<int>(scan::Count(code, "DllMain"));
        That(code.find("dllexport") == std::string::npos, "exports come only from ZSDK_ADDON: " + rel);
    }
    That(addons == 1 && mains == 1, "one ZSDK_ADDON and one DllMain");
    const std::string entry = scan::Lex(scan::Body(t.entry)).code;
    That(entry.find("DllMain") != std::string::npos && entry.find("return zsdk::shell::Main(") != std::string::npos, "DllMain only hands over to zsdk::shell::Main");
    const std::map<std::string, std::string> lock = scan::Config(t.root / "build" / "zsdk.lock");
    That(lock.count("version") && lock.at("version") == ZSDK_VERSION, "zsdk.lock pins this ZoneSDK version");
    const std::string manifest = scan::Body(t.sdk / "MANIFEST.sha256");
    That(!manifest.empty() && lock.count("manifest") && lock.at("manifest") == Sha(manifest), "zsdk.lock pins the manifest of the release it builds against");
    That(t.sdk.parent_path().filename() == "release", "the add-on builds against a release snapshot, not the working tree: " + scan::Text(t.sdk));
    for (const auto &file : t.tests)
    {
        const std::vector<std::string> tokens = scan::Tokens(scan::Lex(scan::Body(file)).code);
        That(!scan::Uses(tokens, "__try*") && !scan::Uses(tokens, "ProcessEvent*"), "tests drive no native code: " + scan::Text(file.filename()));
    }
    if (fresh > 0) std::printf("hardrules: %d timer or thread-pool uses are warnings until this ZoneSDK ships zsdk::job::Every\n", fresh);
    return check::Finish("zsdk-hardrules");
}
