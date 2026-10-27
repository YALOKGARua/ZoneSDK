#include <zsdk/present.h>

#include "hellohud.h"

namespace zhellohud
{
    namespace
    {
        constexpr zsdk::lang::Row kRows[] = {
            { "title", { "ZoneHelloHud {0}", "ZoneHelloHud {0}" } },
            { "wait.world", { "Завантажте збереження у відкритому світі", "Load a save in the open world" } },
            { "wait.settle", { "Світ завантажується", "The world is loading" } },
            { "mod.enabled", { "Увімкнено", "Enabled" } },
            { "mod.toasts", { "Підказки на екрані", "On-screen notes" } },
            { "hud.shown", { "Показувати панель на екрані", "Show the panel on screen" } },
            { "hud.corner", { "Кут екрана", "Screen corner" } },
            { "hud.unit", { "Швидкість у", "Speed in" } },
            { "corner.top_left", { "угорі ліворуч", "top left" } },
            { "corner.top_right", { "угорі праворуч", "top right" } },
            { "corner.bottom_left", { "унизу ліворуч", "bottom left" } },
            { "corner.bottom_right", { "унизу праворуч", "bottom right" } },
            { "unit.kmh", { "км/год", "km/h" } },
            { "unit.ms", { "м/с", "m/s" } },
            { "key.toggle", { "показати або сховати панель", "show or hide the panel" } },
            { "hud.where", { "X {0}  Y {1}  Z {2} м", "X {0}  Y {1}  Z {2} m" } },
            { "hud.heading", { "Напрямок {0}° {1}", "Heading {0}° {1}" } },
            { "hud.kmh", { "Швидкість {0} км/год", "Speed {0} km/h" } },
            { "hud.ms", { "Швидкість {0} м/с", "Speed {0} m/s" } },
            { "hud.reading", { "читаю дані гри", "reading the game" } },
            { "hud.wait", { "ZoneHelloHud: {0}", "ZoneHelloHud: {0}" } },
            { "dir.n", { "Пн", "N" } },
            { "dir.ne", { "ПнСх", "NE" } },
            { "dir.e", { "Сх", "E" } },
            { "dir.se", { "ПдСх", "SE" } },
            { "dir.s", { "Пд", "S" } },
            { "dir.sw", { "ПдЗх", "SW" } },
            { "dir.w", { "Зх", "W" } },
            { "dir.nw", { "ПнЗх", "NW" } },
            { "say.shown", { "Панель на екрані", "The panel is on screen" } },
            { "say.hidden", { "Панель сховано", "The panel is hidden" } },
            { "say.crashed", { "Вимкнено після збою: {0}", "Disabled after a crash: {0}" } },
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
