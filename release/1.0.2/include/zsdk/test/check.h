#pragma once

#include <cstdio>
#include <string>

namespace check
{
    inline int &Failures()
    {
        static int value = 0;
        return value;
    }

    inline int &Count()
    {
        static int value = 0;
        return value;
    }

    inline int &Warnings()
    {
        static int value = 0;
        return value;
    }

    inline void That(bool ok, const std::string &what)
    {
        ++Count();
        if (!ok)
        {
            ++Failures();
            std::printf("FAIL %s\n", what.c_str());
        }
    }

    inline void Warn(const std::string &what)
    {
        ++Warnings();
        std::printf("warning: %s\n", what.c_str());
    }

    inline void Skip(const std::string &what)
    {
        Warn("SKIPPED " + what);
    }

    inline int Finish(const char *name)
    {
        if (Warnings() > 0) std::printf("%s: %d checks, %d failures, %d warnings\n", name, Count(), Failures(), Warnings());
        else std::printf("%s: %d checks, %d failures\n", name, Count(), Failures());
        if (Failures() > 0 && name && std::string(name).rfind("zsdk-", 0) == 0) std::printf("%s: docs\\RULES.md (docs\\RULES-UA.md in Ukrainian), section 10, %s, gives the rule behind each message of this suite\n", name, name);
        return Failures() == 0 ? 0 : 1;
    }
}
