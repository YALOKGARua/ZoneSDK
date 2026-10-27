#include <zsdk/present.h>

#include "template.h"

namespace ztemplate
{
    namespace
    {
        constexpr const char *kLanguages[] = { "auto", "uk", "en" };
        constexpr zsdk::ini::Owned kOwned[] = { { "memory", 16 } };

        int Level(const Settings &s)
        {
            return s.logLevel;
        }

        const char *Language(const Settings &s)
        {
            return s.language.c_str();
        }

        constexpr zsdk::ini::Schema kSchema = zsdk::ini::Make<Settings, Visit, Fix, Level, Language>("general", kOwned, 1);
    }

    void Visit(Settings &s, zsdk::ini::Visitor &v)
    {
        v.Word("general", "language", s.language, "auto", kLanguages, 3);
        v.Flag("general", "enabled", s.enabled);
        v.Flag("general", "toasts", s.toasts);
        v.Whole("general", "log_level", s.logLevel, 1, 0, 2);
        v.Key("keys", "locate", s.locate, Settings{}.locate);
    }

    void Fix(Settings &)
    {
    }

    const zsdk::ini::Schema &Schema()
    {
        return kSchema;
    }
}
