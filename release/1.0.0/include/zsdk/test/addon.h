#pragma once

#include "scan.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace addon
{
    struct Tree
    {
        std::filesystem::path root;
        std::filesystem::path sdk;
        std::map<std::string, std::string> cfg;
        std::vector<std::filesystem::path> present;
        std::vector<std::filesystem::path> overlay;
        std::filesystem::path entry;
        std::vector<std::filesystem::path> tests;
    };

    inline bool Under(const std::filesystem::path &file, const std::filesystem::path &dir)
    {
        const std::wstring a = file.lexically_normal().native();
        std::wstring b = dir.lexically_normal().native();
        while (!b.empty() && (b.back() == L'\\' || b.back() == L'/')) b.pop_back();
        return a.size() > b.size() + 1 && a.compare(0, b.size(), b) == 0 && (a[b.size()] == L'\\' || a[b.size()] == L'/');
    }

    inline Tree Load()
    {
        Tree t;
        t.root = scan::AddonRoot();
        t.sdk = scan::SdkRoot();
        t.cfg = scan::Config(t.root / "tests" / "zsdk.cfg");
        const std::filesystem::path src = t.root / "src";
        for (const auto &file : scan::Files(src, { ".cpp", ".h", ".inc" }, true))
        {
            if (file.filename() == "entry.cpp" && file.parent_path() == src) t.entry = file;
            else if (Under(file, src / "overlay")) t.overlay.push_back(file);
            else t.present.push_back(file);
        }
        t.tests = scan::Files(t.root / "tests", { ".cpp", ".h" }, false);
        return t;
    }

    inline std::set<std::string> Allowed(const Tree &t, const std::string &key)
    {
        std::set<std::string> out;
        const auto found = t.cfg.find(key);
        if (found == t.cfg.end()) return out;
        for (const std::string &one : scan::List(found->second)) out.insert(one);
        return out;
    }

    inline std::string Rel(const Tree &t, const std::filesystem::path &file)
    {
        return scan::Generic(file.lexically_relative(t.root));
    }

    struct Declared
    {
        std::string id;
        std::string name;
        std::string file;
    };

    inline std::vector<Declared> GuardSets(const std::string &body)
    {
        const scan::Lexed lexed = scan::Lex(body);
        const std::vector<std::string> tokens = scan::Tokens(lexed.code);
        const bool open = scan::Uses(tokens, "using namespace zsdk :: guard ;") || scan::Uses(tokens, "using namespace guard ;") || scan::Uses(tokens, "using zsdk :: guard :: Set ;") || scan::Uses(tokens, "using guard :: Set ;");
        std::vector<Declared> out;
        size_t quotes = 0;
        for (size_t i = 0; i < tokens.size(); ++i)
        {
            if (tokens[i] == "\"")
            {
                ++quotes;
                continue;
            }
            if (tokens[i] != "Set") continue;
            const bool qualified = i >= 2 && tokens[i - 1] == "::" && tokens[i - 2] == "guard";
            const bool bare = open && (i == 0 || tokens[i - 1] != "::");
            if (!qualified && !bare) continue;
            size_t k = i + 1;
            while (k < tokens.size() && (tokens[k] == "const" || tokens[k] == "constexpr")) ++k;
            if (k >= tokens.size() || !scan::Ident(tokens[k][0]) || std::isdigit(static_cast<unsigned char>(tokens[k][0]))) continue;
            Declared d;
            d.id = tokens[k++];
            if (k < tokens.size() && tokens[k] == "=") ++k;
            if (k >= tokens.size() || (tokens[k] != "{" && tokens[k] != "(")) continue;
            ++k;
            if (k < tokens.size() && tokens[k] == "\"" && quotes / 2 < lexed.literals.size()) d.name = lexed.literals[quotes / 2];
            else
            {
                size_t seen = quotes;
                int depth = 1;
                for (size_t j = k; j < tokens.size() && depth > 0; ++j)
                {
                    const std::string &one = tokens[j];
                    if (one == "{" || one == "(") ++depth;
                    else if (one == "}" || one == ")") --depth;
                    else if (one == "\"")
                    {
                        if (depth == 1 && j >= 3 && tokens[j - 1] == "=" && tokens[j - 2] == "name" && tokens[j - 3] == "." && seen / 2 < lexed.literals.size()) d.name = lexed.literals[seen / 2];
                        ++seen;
                    }
                }
            }
            out.push_back(d);
        }
        return out;
    }

    inline std::vector<Declared> GuardSets(const Tree &t)
    {
        std::vector<std::filesystem::path> files = t.present;
        files.insert(files.end(), t.overlay.begin(), t.overlay.end());
        if (!t.entry.empty()) files.push_back(t.entry);
        std::vector<Declared> out;
        for (const auto &file : files)
            for (Declared d : GuardSets(scan::Body(file)))
            {
                d.file = Rel(t, file);
                out.push_back(d);
            }
        return out;
    }
}
