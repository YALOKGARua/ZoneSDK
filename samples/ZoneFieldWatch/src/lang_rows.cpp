#include <zsdk/present.h>

#include "fieldwatch.h"

namespace zfieldwatch
{
    namespace
    {
        constexpr zsdk::lang::Row kRows[] = {
            { "title", { "ZoneFieldWatch {0}", "ZoneFieldWatch {0}" } },
            { "wait.world", { "Завантажте збереження у відкритому світі", "Load a save in the open world" } },
            { "wait.settle", { "Світ завантажується", "The world is loading" } },
            { "mod.enabled", { "Читати поля гри", "Read the game fields" } },
            { "mod.paused", { "Читання вимкнено в налаштуваннях", "Reading is switched off in the settings" } },
            { "lang.label", { "Мова", "Language" } },
            { "lang.auto", { "Як у грі", "As in the game" } },
            { "lang.uk", { "Українська", "Українська" } },
            { "lang.en", { "English", "English" } },
            { "head.fields", { "Поля, за якими стежить аддон", "Watched fields" } },
            { "field.line", { "{0}: {1}", "{0}: {1}" } },
            { "value.fov", { "{0}°", "{0}°" } },
            { "value.free", { "{0}°, FOV не зафіксовано", "{0}°, the FOV is not locked" } },
            { "value.locked", { "{0}°, FOV зафіксовано", "{0}°, the FOV is locked" } },
            { "proof.reflected",
              { "Рефлексія: {0} на +{1}. Зсув звіряється з даними рефлексії гри, тож поле працює на кожній версії, де вони збігаються",
                "Reflected: {0} at +{1}. The offset is checked against the game reflection, so the field works on every build where the reflection agrees" } },
            { "proof.guard",
              { "Байтовий захист: код GetFOVAngle на {0} містить +{1}. Набір {2} потребує саме версії {3}", "Byte guard: the GetFOVAngle code at {0} holds +{1}. The set {2} needs exactly build {3}" } },
            { "proof.unproven",
              { "Не доведено: +{0}. Поле записане з причиною в tests/unproven-fields.txt, і SDK відмовляє в кожному його читанні",
                "Unproven: +{0}. The field is listed with its reason in tests/unproven-fields.txt, and the SDK refuses every read of it" } },
            { "verdict.pending", { "чекає на перевірку", "waiting for its check" } },
            { "verdict.waiting", { "чекає на камеру гравця", "waiting for the player camera" } },
            { "verdict.off", { "вимкнено на цій версії гри", "off on this game build" } },
            { "verdict.crashed", { "вимкнено після збою, див. «Спробувати ще» в діагностиці", "disabled after a crash, see Try again in Diagnostics" } },
            { "verdict.unmarked", { "мітку збою не вдалося записати, тож читання пропущено", "the crash marker could not be written, so the read is skipped" } },
            { "verdict.unproven", { "не доведено, не читається", "unproven, never read" } },
            { "marker.fresh", { "Мітка збою {0}: ставиться навколо першого читання на цій версії гри", "Crash marker {0}: set around the first read on this game build" } },
            { "marker.proven", { "Мітка збою {0}: читання на цій версії гри вже повернулося, мітка більше не пишеться", "Crash marker {0}: a read on this game build already returned, the marker is no longer written" } },
            { "marker.crashed", { "Мітка збою {0}: гра завершилася, поки мітка стояла, тож читання вимкнено", "Crash marker {0}: the game ended while the marker was set, so the read is off" } },
            { "head.build", { "Версія гри і вердикти", "Game build and verdicts" } },
            { "build.unchecked", { "Версію гри ще не перевірено", "The game build is not checked yet" } },
            { "build.known", { "Версія гри {0} відома цьому ZoneSDK", "Game build {0} is known to this ZoneSDK" } },
            { "build.other",
              { "Версія гри {0} невідома цьому ZoneSDK: поля з байтовим захистом вимкнено, поля з рефлексії працюють, якщо дані рефлексії збігаються",
                "Game build {0} is not known to this ZoneSDK: the byte-guarded fields are off, the reflected ones work where the reflection agrees" } },
            { "build.rejected",
              { "Ця версія гри не підтримується: ZoneSDK не зв'язав рушій, тож жодне поле не читається", "This game build is not supported: ZoneSDK did not bind the engine, so no field is read" } },
            { "set.on", { "Набір захисту {0} ({1}): увімкнено", "Guard set {0} ({1}): on" } },
            { "set.off", { "Набір захисту {0} ({1}): вимкнено", "Guard set {0} ({1}): off" } },
            { "set.pending", { "Набір захисту {0} ({1}): чекає на хеш exe гри", "Guard set {0} ({1}): waiting for the hash of the game exe" } },
            { "need.known", { "байти, розмір образу й точна версія", "bytes, image size and the exact build" } },
        };
    }

    std::span<const zsdk::lang::Row> Rows()
    {
        return kRows;
    }
}
