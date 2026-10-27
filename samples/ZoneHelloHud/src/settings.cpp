#include <zsdk/present.h>

#include "hellohud.h"

namespace zhellohud
{
    namespace
    {
        constexpr const char *kLanguages[] = { "auto", "uk", "en" };

        int Level(const Settings &s)
        {
            return s.logLevel;
        }

        const char *Language(const Settings &s)
        {
            return s.language.c_str();
        }

        constexpr zsdk::ini::Schema kSchema = zsdk::ini::Make<Settings, Visit, Fix, Level, Language>("general", nullptr, 0);
    }

    void Visit(Settings &s, zsdk::ini::Visitor &v)
    {
        v.Word("general", "language", s.language, "auto", kLanguages, 3);
        v.Flag("general", "enabled", s.enabled);
        v.Flag("general", "toasts", s.toasts);
        v.Whole("general", "log_level", s.logLevel, 1, 0, 2);
        v.Flag("hud", "shown", s.shown);
        zsdk::ini::Choice(v, "hud", "corner", s.corner, kCorners, Corner::TopLeft);
        zsdk::ini::Choice(v, "hud", "speed", s.unit, kUnits, Unit::Kmh);
        v.Key("keys", "toggle", s.toggle, Settings{}.toggle);
    }

    void Fix(Settings &)
    {
    }

    const zsdk::ini::Schema &Schema()
    {
        return kSchema;
    }
}
