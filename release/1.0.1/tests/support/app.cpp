#include <zsdk/present.h>

#include "probe.h"

namespace probe
{
    namespace
    {
        constexpr const char *kLooks[] = { "cold", "toxic" };
        constexpr const char *kLanguages[] = { "auto", "uk", "en" };
        constexpr zsdk::ini::Owned kOwned[] = { { "probe", static_cast<size_t>(-1) }, { "memory", 4 } };

        void Visit(Settings &s, zsdk::ini::Visitor &v)
        {
            v.Word("general", "language", s.language, "auto", kLanguages, 3);
            v.Flag("general", "enabled", s.enabled);
            v.Whole("general", "log_level", s.logLevel, 1, 0, 2);
            v.Real("alpha", "radius_cm", s.radiusCm, 150, 100, 300);
            zsdk::ini::Choice(v, "alpha", "look", s.look, kLooks, Look::Cold);
            v.Flag("alpha", "enabled", s.alphaOn);
            v.Key("keys", "next", s.next, Settings{}.next);
            v.Key("keys", "back", s.back, Settings{}.back);
            v.Text("names", "callsign", s.callsign, kCallsignMost);
            v.Word("names", "mark", s.mark, "Alpha", nullptr, 0);
        }

        void Fix(Settings &s)
        {
            if (s.back.key && s.back == s.next) s.back = zsdk::keys::Chord{};
        }

        int Level(const Settings &s)
        {
            return s.logLevel;
        }

        const char *Language(const Settings &s)
        {
            return s.language.c_str();
        }

        constexpr zsdk::ini::Schema kSchema = zsdk::ini::Make<Settings, Visit, Fix, Level, Language>("general", kOwned, 2);

        constexpr zsdk::lang::Row kRows[] = {
            { "title", { "Перевірка {0}", "Probe {0}" } },
            { "say.one", { "Один {0}", "One {0}" } },
            { "lang.uk", { "Українська", "Українська" } },
            { "lang.en", { "English", "English" } },
        };

        constexpr const char *kStatics[] = { "GameplayStatics", "KismetSystemLibrary" };
        constexpr const char *kHosts[] = { "CustomConsoleManagerRK" };
    }

    const zsdk::App &Make()
    {
        return Mutable();
    }

    zsdk::App &Mutable()
    {
        static zsdk::App app = [] {
            zsdk::App a{};
            a.identity = zsdk::Identity{ "ZoneSdkProbe", "ZoneSdkProbe", "ZoneSDK probe", "0.0.1", L"ZoneSdkProbe", L"probe.ini", L"probe.log", L"probe.1.log", "template" };
            a.specs = kSpecs;
            a.world = zsdk::WorldVerbs{ 0, 1, 2, 3, -1, 4, "WorldMap_WP" };
            a.census = zsdk::CensusSpec{ kStatics, 2, kHosts, 1 };
            a.rows = kRows;
            a.schema = &kSchema;
            return a;
        }();
        return app;
    }
}

namespace zsdk::app
{
    const App &Get()
    {
        return probe::Make();
    }
}
