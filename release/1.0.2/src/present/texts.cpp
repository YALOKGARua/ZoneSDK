#include <zsdk/present.h>

namespace zsdk::lang
{
    namespace
    {
        constexpr Row kDefaults[] = {
            { "phase.off", { "Вимкнено", "Off" } },
            { "phase.hooking", { "Під'єднуюся до гри", "Hooking into the game" } },
            { "phase.binding", { "Шукаю функції гри", "Binding game functions" } },
            { "phase.wiring", { "Готую виклики", "Wiring calls" } },
            { "phase.waiting", { "Чекаю на світ гри", "Waiting for the game world" } },
            { "phase.ready", { "Готово", "Ready" } },
            { "phase.failed", { "Не працює з цією версією гри", "Does not work with this game version" } },
            { "phase.halted", { "Виклики гри зупинено до кінця сесії", "Game calls are stopped for this session" } },
            { "phase.lost", { "Потік гри не відповідає понад 2 с", "The game thread has not answered for over 2 s" } },
            { "wait.consoles", { "Чекаю на консолі гри", "Waiting for the game consoles" } },
            { "diag.title", { "Діагностика", "Diagnostics" } },
            { "diag.phase", { "Хук {0}, зв'язано {1}, виклики {2}, світ {3}, епоха {4}", "Hook {0}, bound {1}, calls {2}, world {3}, epoch {4}" } },
            { "diag.exe", { "Версія гри: {0}", "Game build: {0}" } },
            { "diag.known", { "відома ({0})", "known ({0})" } },
            { "diag.other", { "інша", "other" } },
            { "diag.unknown", { "ще не перевірена", "not checked yet" } },
            { "diag.foreign", { "{0}, не Steam-версія", "{0}, not the Steam build" } },
            { "diag.verbs", { "Функції: обов'язкові {0}/{1}, додаткові {2}/{3}", "Functions: required {0}/{1}, optional {2}/{3}" } },
            { "diag.crashed", { "Вимкнено після збою: {0}", "Disabled after a crash: {0}" } },
            { "diag.none", { "немає", "none" } },
            { "diag.retry", { "Спробувати ще", "Try again" } },
            { "diag.log", { "Журнал: {0}", "Log: {0}" } },
            { "diag.sdk", { "ZoneSDK {0}, старі правила: {1}", "ZoneSDK {0}, legacy rules: {1}" } },
            { "diag.markers", { "Мітки збоїв не зберігаються: тека даних недоступна", "Crash markers are not saved: the data folder is unavailable" } },
            { "diag.full", { "Список вимкнених викликів заповнено, найстаріші вмикаються знову", "The disabled list is full, the oldest calls are enabled again" } },
            { "diag.pump",
              { "Потік гри: надіслано {0}, отримано {1}, тактів {2}, останній {3} мс тому; підключення: спроб {4}, {5} мс", "Game thread: posted {0}, seen {1}, ticks {2}, last {3} ms ago; wiring: {4} attempts, {5} ms" } },
            { "diag.report", { "Скопіювати звіт", "Copy report" } },
            { "diag.copied", { "Звіт скопійовано і збережено: {0}", "Report copied and saved: {0}" } },
            { "diag.unsaved", { "Звіт скопійовано, але зберегти його в теці даних не вдалося", "Report copied, but it could not be saved in the data folder" } },
            { "yes", { "так", "yes" } },
            { "no", { "ні", "no" } },
            { "key.press", { "Натисніть клавішу (Esc скасовує)", "Press a key (Esc cancels)" } },
            { "key.bad", { "Цю клавішу не можна призначити", "This key cannot be bound" } },
            { "key.game", { "Цю клавішу також використовує гра", "The game uses this key too" } },
            { "sdk.unsupported", { "Ця версія гри не підтримується, аддон нічого не змінює", "This game build is not supported, the add-on changes nothing" } },
            { "sdk.lanes", { "Частини аддона {0} вимкнено після помилки: {1}. Подробиці в журналі.", "Parts of {0} were switched off after an error: {1}. The log has the details." } },
            { "sdk.restart", { "Аддон запуститься знову після перезапуску гри.", "The add-on starts again after the game restarts." } },
            { "lane.boot", { "запуск аддона", "the add-on start" } },
            { "lane.core", { "основна робота аддона", "the core work of the add-on" } },
            { "lane.frame", { "робота в кожному кадрі", "the work in every frame" } },
            { "lane.data", { "збереження налаштувань і журналу", "saving the settings and the log" } },
            { "lane.block", { "шрифти", "the fonts" } },
            { "lane.overlay", { "малювання поверх гри", "drawing over the game" } },
            { "lane.toasts", { "підказки на екрані", "the on-screen messages" } },
            { "lane.page", { "ця вкладка", "this tab" } },
            { "lane.open", { "відкриття і закриття ReShade", "opening and closing ReShade" } },
            { "lane.device", { "звільнення графіки", "releasing the graphics" } },
            { "lane.deviceDestroy", { "звільнення графічного пристрою", "releasing the graphics device" } },
            { "lane.screen", { "скидання підказок після зміни екрана", "resetting the messages after a screen change" } },
            { "lane.save", { "збереження під час закриття", "saving on close" } },
            { "lane.pipelineInit", { "створення графічних конвеєрів", "creating graphics pipelines" } },
            { "lane.pipelineBind", { "використання графічних конвеєрів", "using graphics pipelines" } },
            { "lane.pipelineDestroy", { "видалення графічних конвеєрів", "removing graphics pipelines" } },
        };
    }

    std::span<const Row> Defaults()
    {
        return kDefaults;
    }
}
