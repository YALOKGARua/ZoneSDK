#include <zsdk/present.h>

#include "template.h"

namespace ztemplate
{
    namespace
    {
        constexpr zsdk::lang::Row kRows[] = {
            { "title", { "ZoneTemplate {0}", "ZoneTemplate {0}" } },
            { "wait.world", { "Завантажте збереження у відкритому світі", "Load a save in the open world" } },
            { "wait.settle", { "Світ завантажується", "The world is loading" } },
            { "mod.enabled", { "Увімкнено", "Enabled" } },
            { "mod.toasts", { "Підказки на екрані", "On-screen notes" } },
            { "key.locate", { "де я стою", "where I stand" } },
            { "btn.locate", { "Де я?", "Where am I?" } },
            { "say.located", { "Ви тут: {0}", "You are here: {0}" } },
            { "say.refused", { "Не зараз: {0}", "Not now: {0}" } },
            { "say.crashed", { "Вимкнено після збою: {0}", "Disabled after a crash: {0}" } },
            { "why.disabled", { "аддон вимкнено", "the add-on is off" } },
            { "why.world", { "світ гри ще не готовий", "the game world is not ready" } },
            { "why.verbs", { "деяких функцій гри немає", "some game functions are missing" } },
            { "why.crash", { "виклик вимкнено після збою", "the call was disabled after a crash" } },
            { "why.failed", { "виклик не вдався", "the call failed" } },
            { "lang.label", { "Мова", "Language" } },
            { "lang.auto", { "Як у грі", "As in the game" } },
            { "lang.uk", { "Українська", "Українська" } },
            { "lang.en", { "English", "English" } },
        };
    }

    std::span<const zsdk::lang::Row> Rows()
    {
        return kRows;
    }
}
