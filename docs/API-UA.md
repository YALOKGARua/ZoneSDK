# Довідник API ZoneSDK

Це довідник C++ API ZoneSDK 1.0.0: по одному розділу на кожен заголовний файл у `include/zsdk`. Кожен розділ пояснює, для чого заголовок, з якого потоку можна викликати його функції, які в ньому виклики й типи, і дає короткий приклад та тег стабільності.

Англійська версія: [API.md](API.md). Назви в коді, приклади й теги стабільності тут ті самі, що й в англійській версії. Повідомлення журналу і тестів SDK пишуться англійською, тож вони наведені без перекладу.

Щоб створити і зібрати аддон, спершу прочитайте [GETTING_STARTED-UA.md](GETTING_STARTED-UA.md), а правила, які перевіряють загальні набори тестів, описано в [RULES-UA.md](RULES-UA.md). [VERSIONING-UA.md](VERSIONING-UA.md) пояснює, що обіцяють теги стабільності, а [PATCH_DAY-UA.md](PATCH_DAY-UA.md) — що відбувається після оновлення гри.

## Як читати цей довідник

### Де лежить код

Аддон, створений на основі стартового, має три види вихідних файлів. Збирання компілює їх по-різному, а заголовки перевіряють, що кожен файл підключає лише те, що йому дозволено.

| Файл | Починається з | Збирається як | Може використовувати |
|---|---|---|---|
| `src/*.cpp` і підтеки, крім `src/overlay` і `src/entry.cpp` | `#include <zsdk/present.h>` | модулі кадру, з `ZSDK_PRESENT_UNIT` і `IMGUI_DISABLE` | усе з `zsdk/zsdk.h`; без ImGui, без `zsdk/overlay.h` і `zsdk/paint.h` |
| `src/overlay/*.cpp` | `#include <zsdk/overlay.h>` | модулі оверлею | ImGui, `zsdk::tab`, `zsdk::paint` і все з `zsdk/zsdk.h` |
| `src/entry.cpp` | `#include <zsdk/shell.h>` | разом із модулями оверлею | `ZSDK_ADDON`, `DllMain` і таблиця `zsdk::Hooks` |
| `tests/*.cpp` | `#include <zsdk/present.h>` і заголовки `zsdk/test` | кожен як окрема тестова програма, з `ZSDK_TESTS` | модулі кадру аддона, `check`, `scan` і `addon` |

Вихідні файли SDK компілюються в кожен аддон із закріпленого випуску, тож між аддонами немає двійкового інтерфейсу: кожен аддон має власну копію ZoneSDK, і аддони на різних версіях ZoneSDK працюють поруч.

ZoneSDK 1.0.0 знає дві Steam-збірки S.T.A.L.K.E.R. 2: збірку exe `2bd3f115` (версія гри 2.1, збірка Steam 26123434) і збірку exe `61bc1e03` (версія гри 2.0.6). Він потребує ReShade 6.8 або новішого з повною підтримкою аддонів (with full add-on support, ReShade API 20). На збірках з інших крамниць аддон завантажується, але лишається вимкненим і повідомляє про це. Після оновлення гри можливості, що спираються на рефлексію (reflection) гри, зазвичай працюють далі, а можливості під захистом байтів самі вимикаються ([PATCH_DAY-UA.md](PATCH_DAY-UA.md)).

### Назви потоків

Код ZoneSDK виконується в кількох потоках. Кожен розділ називає потік, з якого можна викликати його функції, такими словами:

| Слово | Потік | Хуки, що там виконуються |
|---|---|---|
| **потік гри** (game) | потік гри, усередині помпи SDK | `refresh`, `prime`, `advance`, `step`, `hot`, `publish`, `turn`, `order`, `orderName` |
| **потік кадру** (present) | потік рендерингу, у події present від ReShade | `boot`, `frame`, `opened` |
| **оверлей** (overlay) | потік рендерингу, усередині події overlay від ReShade (кадр ImGui) | `overlay`, `page`, `words`, `tone`, `report` |
| **робочий потік** (worker) | потік перепису SDK, один на аддон | `census`, `complete`, запуски `job::Every`; тут може виконуватися `quit` |
| **D3D** | будь-який потік пристрою ReShade, часто кілька паралельно | `pipelineInit`, `pipelineBind`, `pipelineDestroy`, `deviceDestroy` |
| **завантажувач** (loader) | `DllMain`, під блокуванням завантажувача Windows | `stop` |
| **будь-який** (any) | безпечно з будь-якого з потоків вище | функції з внутрішнім блокуванням або чисті функції |

Викликати гру може лише потік гри: створення `zsdk::Call`, `zsdk::Fire` і записи `zsdk::mem` у будь-якому іншому потоці відхиляються. Під час виходу з процесу (`hooks.stop`, коли `shell::Exiting()` дає true) інших потоків уже немає; що там дозволено, перелічує [RULES-UA.md](RULES-UA.md#4-вихід-із-гри-hooksquit-і-hooksstop).

### Теги стабільності

Кожен розділ має рядок **Стабільність**, а назви з іншим тегом перелічено в окремому рядку. Теги беруться з `tools/api/tags.txt`, за яким інструмент випуску пише файл поверхні API. Самі теги лишаються англійськими словами, як у файлі тегів:

- **Stable**: зберігається в усіх випусках 1.x; поля лише додаються в кінець, значення переліків лише додаються перед `Count`.
- **Provisional**: може змінитися або зникнути в мінорному випуску, із приміткою в [CHANGES-UA.md](../CHANGES-UA.md).
- **Legacy**: залишено для аддонів, написаних до SDK; може зникнути у 2.0; нові аддони лишають типові значення.
- **Internal**: для SDK і його тестів; може змінитися в будь-якому випуску; аддони цього не викликають.

### Приклади

Приклади взято зі стартового аддона (`template/ZoneTemplate`) і зразків (`samples/ZoneHelloHud`, `samples/ZoneFieldWatch`). Назви на кшталт `Settings`, `kSpecs`, `kLocation`, `kLocateKey`, `Order` і `core::Snapshot` належать самому стартовому аддону. `tools/api/doccheck.ps1 -Release <release folder>` компілює кожен приклад цієї сторінки в тимчасовій копії стартового аддона і запускає її тести, тож приклади збираються з тим випуском, який описують. Той самий сценарій перевіряє, що ця сторінка описує кожну назву з заголовків і що її приклади слово в слово збігаються з англійськими.

## `zsdk/app.h`

Опис аддона: хто він, які функції гри викликає, його тексти, налаштування, смуга підказок і набори захисту. Аддон визначає `zsdk::app::Get()` один раз, у модулі кадру, і повертає один статичний `zsdk::App`. SDK читає його з усіх потоків і ніколи не змінює.

**Потік:** будь-який. `App` створюється один раз, під час першого звертання, і далі не змінюється.

**Стабільність:** Stable

| Поле `App` | Що містить |
|---|---|
| `identity` | `Identity`: назви і файли аддона |
| `policy` | `Policy` (див. `zsdk/policy.h`); стартовий аддон лишає `zsdk::Policy{}` |
| `specs` | таблиця дієслів, `std::span` з `verbs::Spec`; `verbs::Prime` звіряє її з грою під час підготовки (prime), а `Call(table, verb)` приймає лише цю таблицю |
| `world` | `WorldVerbs`: де в `specs` стоять шість дієслів світу |
| `census` | `CensusSpec`: назви класів, які тримає перепис |
| `rows` | мовні рядки аддона (`lang::Row`); типові рядки SDK заповнюють кожен id, якого аддон не має |
| `schema` | схема налаштувань, `ini::Schema` з `ini::Make`; ZoneSDK 1.0.0 її вимагає: без схеми аддон лишається вимкненим, не отримує ні теки даних, ні налаштувань, ні журналу і пише один рядок у вивід налагодження Windows (`ZoneSDK: <name> stays off: the add-on has no settings schema ...`); загальний набір тестів `identity` такий аддон не пропускає |
| `row` | власний `registry::Row` аддона: id, тег, смуга підказок і типові клавіші |
| `sets` | кожен `guard::Set`, який оголошує аддон |

`App::sets` має перелічувати кожен набір захисту аддона:

```text
constexpr const zsdk::guard::Set *kSets[] = { &kCoreSet };
a.sets = kSets;
```

SDK перевіряє перелічені набори під час підготовки, по одному рядку журналу на набір. Загальний набір тестів `guards` порівнює з exe гри лише перелічені набори захисту; набір захисту, знайдений у вихідних файлах, але відсутній у `sets`, дає попередження, а з `make.ps1 -RequireExe` — помилку. Під час роботи `guard::Of` щонайбільше раз на хвилину пише в журнал, що набору немає в `App::sets`.

| Поле `Identity` | Для чого воно |
|---|---|
| `name` | назва аддона; нею підписано вкладку аддона в ReShade і рядки журналу, і вона має збігатися з назвою, з якою аддон збирають |
| `tag` | унікальний тег ASCII; від нього походять назви повідомлення пробудження потоку гри і м'ютексу одного екземпляра, тож два аддони з одним тегом не можуть завантажитися обидва |
| `description` | текст, який ReShade показує для аддона |
| `version` | версія аддона: у рядку журналу під час запуску, у звіті і в `ledger::Addon()` |
| `folder` | тека даних у `%LOCALAPPDATA%` |
| `ini`, `log`, `rolled` | назви файлів у теці даних: налаштування, журнал, попередній журнал |
| `registry` | id у таблиці ZoneSDK; якщо власного рядка немає, `app::Row()` бере рядок таблиці SDK із цим id |

`WakeName(identity)` дає назву повідомлення пробудження `<tag>.GameThread.1`, а `MutexShape(identity)` — шаблон назви м'ютексу `Local\<tag>.Addon.%lu` (у нього підставляється id процесу). Якщо м'ютекс уже існує, `DllMain` повертає FALSE і пише один рядок через `OutputDebugString`: другий аддон із цим тегом не завантажується.

| Поле `WorldVerbs` | Роль | Типово |
|---|---|---|
| `controller` | `GameplayStatics.GetPlayerController` | `verbs::common::kPlayerController` |
| `pawn` | `GameplayStatics.GetPlayerPawn` | `kPlayerPawn` |
| `camera` | `GameplayStatics.GetPlayerCameraManager` | `kCameraManager` |
| `seconds` | `GameplayStatics.GetTimeSeconds` | `kTimeSeconds` |
| `paused` | `GameplayStatics.IsGamePaused` | `kGamePaused` |
| `frames` | `KismetSystemLibrary.GetFrameCount` | `kFrameCount` |
| `map` | назва карти відкритого світу | `"WorldMap_WP"` |

Кожна роль — це індекс у `App::specs`. Типові значення — індекси каталогу, тож таблиці, що починається з `common::kWorld` (див. `zsdk/verbs/common.h`), власний `WorldVerbs` не потрібен. У 1.0.0 це змінилося: у 0.1.x `WorldVerbs{}` означав «жодних дієслів світу» (усі -1), а тепер означає індекси каталогу. Щоб вимкнути роль, задайте їй -1. На першому кроці світу SDK перевіряє кожну роль: якщо індекс не вказує на дієслово каталогу цієї ролі (за власником і назвою), роль вимикається з одним рядком журналу і ніколи не викликається, а загальний набір `specs` уже під час збирання вважає таку роль помилкою.

| Поле `CensusSpec` | Що містить |
|---|---|
| `statics`, `staticCount` | назви класів, чиї типові об'єкти класу (class default objects) приймають статичні виклики, наприклад `common::kStatics` |
| `hosts`, `hostCount` | назви класів живих об'єктів, на яких виконуються виклики, наприклад `common::kConsoleHost` |

Кожен список містить щонайбільше `census::kSlots` (16) назв. `App::schema` вказує на `ini::Schema`; `app.h` лише оголошує `zsdk::ini::Schema`, тож щоб створити схему, підключіть `zsdk/ini.h` (або `zsdk/zsdk.h`).

| Виклик | Що робить |
|---|---|
| `app::Get()` | `App` аддона; його пише сам аддон |
| `app::Row()` | власний `row` аддона, якщо задано `row.id` і `row.tag`; інакше рядок таблиці SDK для `identity.registry`; інакше `nullptr` |
| `app::Lane()` | смуга підказок: смуга з рядка; 0 (без підказок), якщо смуга рядка дорівнює 0 або становить 1 і більше; `registry::kSpareLane` з одним рядком журналу, якщо рядка немає |
| `WakeName(identity)` | назва повідомлення пробудження |
| `MutexShape(identity)` | шаблон назви м'ютексу |

`src/app.cpp` стартового аддона (модуль кадру):

<!-- doccheck: present -->
```cpp
const zsdk::App &Make()
{
    static const zsdk::App app{
        .identity = zsdk::Identity{ .name = "ZoneTemplate", .tag = "ZoneTemplate", .description = TEMPLATE_DESCRIPTION, .version = kVersion, .folder = L"ZoneTemplate", .ini = L"template.ini", .log = L"template.log", .rolled = L"template.1.log", .registry = "template" },
        .specs = kSpecs,
        .census = zsdk::CensusSpec{ .statics = common::kStatics, .staticCount = 2 },
        .rows = Rows(),
        .schema = &Schema(),
        .row = zsdk::registry::Row{ .id = "template", .tag = "ZoneTemplate", .lane = 0.54f, .keys = "ALT+L" },
    };
    return app;
}
```

`zsdk::app::Get()` стартового аддона повертає цей `Make()`. Пишіть позначені ініціалізатори (designated initializers), як вище: поля лише додаються в кінець, тож код і далі збиратиметься з пізнішими випусками 1.x.

## `zsdk/base.h`

Годинники і дрібні помічники, потрібні всюди.

**Потік:** будь-який.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Now()` | мілісекунди від запуску Windows (`GetTickCount64`); той самий `now`, який отримують хуки |
| `Ms()` | годинник високої точності в мілісекундах, як `double`, для вимірювання часу |
| `Age(now, at)` | `now - at` або 0, якщо `at` пізніше за `now`; користуйтеся ним для кожної різниці часових позначок |
| `Addr(thing)` | вказівник як `uintptr_t`, для `mem::Read` і `mem::Write` |
| `kNaN` | тихий NaN типу `double` |

Загальний набір `style` попереджає про сире `now - x` у вихідних файлах аддона, бо позначка часу, взята в іншому потоці, може бути пізнішою за `now`. Крок зразка FieldWatch:

<!-- doccheck: present body -->
```cpp
static uint64_t readAt = 0;
if (zsdk::Age(now, readAt) < 250) return;
readAt = now;
const double from = zsdk::Ms();
const double fov = zsdk::exe::Known() ? 90.0 : zsdk::kNaN;
zsdk::Note(2, "fieldwatch: read %.0f at %llu in %.3f ms", fov, static_cast<unsigned long long>(zsdk::Now()), zsdk::Ms() - from);
```

## `zsdk/call.h`

Єдиний спосіб викликати функцію гри. `Call` тримає блок параметрів однієї `UFunction`; `Fire` виконує її під міткою збою.

**Потік:** потік гри. У будь-якому іншому потоці конструктор `Call`, `Lookup` і `Fire` кидають `Refused`, тож нативний код там не виконується.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Call(table, verb, on)` | прив'язує рядок `verb` таблиці дієслів `table` (`App::specs` аддона) до об'єкта `on` |
| `Call(owner, name, on)` | прив'язує функцію за класом-власником і назвою, без рядка таблиці |
| `Set<T>(name, value)` | записує параметр; розмір має дорівнювати `sizeof(T)` |
| `Put(name, source, bytes)` | записує параметр із сирих байтів |
| `Try<T>(name, value)` | записує параметр, лише якщо він існує з таким розміром; інакше мовчить, і без перевірок, описаних нижче |
| `Wide(name, value)` | записує цілий параметр розміром від 1 до 8 байтів |
| `CopyFrom(name, source, sourceName)` | копіює параметр з іншого `Call` |
| `Get<T>(name)` | читає параметр або, після `Fire`, результат; розмір має дорівнювати `sizeof(T)` |
| `Yes(name)` | читає результат типу `bool` |
| `Widened(name)` | читає цілий параметр розміром від 1 до 8 байтів як `int64_t` |
| `CopyText(name)` | читає параметр або результат `FString` як `std::wstring`; власний буфер гри не звільняється, тож рядок, який гра створила для результату, лишається в пам'яті (невеликий витік на кожен виклик, так задумано) |
| `Has(name)`, `Find(name)`, `Parameter(name)` | шукають параметр: `Has` каже, чи він існує, `Find` дає `const ue::Param *` або `nullptr`, `Parameter` дає `const ue::Param &` або кидає виняток |
| `Data()` | блок параметрів на 2048 байтів, вирівняний на 16 байтів |
| `Key()` | ключ реєстру збоїв `Owner.Name` |
| `Function()` | кешований `Fn` |
| `Target()` | об'єкт, на якому виконується виклик |
| `Risky()` | чи має рядок таблиці `verbs::Mark::Risky` |
| `Fire(call)` | виконує виклик під його власним ключем `Owner.Name`; з міткою, якщо `Policy::marks` дорівнює `Marks::All` (типово) або рядок позначено `Risky` |
| `Fire(call, key)` | виконує виклик під ключем реєстру `key` (наприклад, `Template.Locate`); завжди з міткою |
| `Lookup(owner, name, target)` | кешований `Fn` функції; ним користуються конструктори |

| Поле `Fn` | Що містить |
|---|---|
| `function` | `UFunction` |
| `owner` | її клас-власник |
| `sign` | її параметри, `ue::Sign` |
| `missing` | true, якщо функцію не знайдено або її блок не вміщується у 2048 байтів |
| `widened` | true, якщо пошук охопив і клас цілі (Legacy `Policy::widen`) |

Що робить `Fire`, по черзі:

1. Відхиляє виклик (`Refused`), коли SDK зупиняється або гра попросила вийти («engine calls are stopped»), поза помпою потоку гри, коли сесію зупинено і коли ключ вимкнено після збою («disabled after a crash: <key>»).
2. Перший виклик ключа з міткою на цій збірці гри спершу записує мітку збою (`pending` в ini, зі скиданням на диск). З `Policy::strictMarks` виклик, мітку якого неможливо записати, відхиляється («crash marker unavailable: <key>»).
3. Виконує функцію. Коли вона повертається, ключ доведено для цієї збірки, і подальші виклики йдуть без запису мітки.
4. Нативний збій усередині виклику вимикає ключ і на наступні сесії (до «Спробувати ще / Try again» у «Діагностиці») та кидає `Fault`. З `Halt::Session` він ще й зупиняє всі виклики до кінця сесії.

`Refused` і `Fault` обидва походять від `std::runtime_error` (`using std::runtime_error::runtime_error`). `Refused` означає, що нічого не виконалося: спробуйте пізніше. `Fault` означає, що гра впала всередині функції, і ключ тепер вимкнено. Звичайний `std::runtime_error` означає відсутню функцію, відсутній параметр або розмір, що не підходить.

З `Policy::verifiedOnly` (типово) SDK відхиляє ще більше, перш ніж виконається будь-який нативний код: `Call(table, verb)` з дієсловом, якого `verbs::Prime` не підтвердив, або з таблицею, відмінною від `App::specs`; явну ціль, яка `ue::Doomed`; приречений об'єкт (той, що знищується), переданий через `Set`, `Put` чи `CopyFrom` у параметр-об'єкт; виклик без цілі, чий об'єкт-хост відсутній («host not ready: <key>»). `Set`, `Put` і `CopyFrom` також відмовляються позичати текст (`ue::Lend`) у параметр `FString`, який гра записує назад (вихідний параметр або результат).

Виклик locate стартового аддона (`src/core.cpp`):

<!-- doccheck: present body -->
```cpp
const zsdk::world::Grip &w = zsdk::world::Hold();
if (!w.stable || !zsdk::ue::Here(w.body, w.bodyType) || zsdk::ledger::Blocked(kLocateKey)) return;
try
{
    zsdk::Call c(kSpecs, kLocation, w.body);
    zsdk::Fire(c, kLocateKey);
    const zsdk::math::Vec3 at = c.Get<zsdk::math::Vec3>("ReturnValue");
    zsdk::Log("template: located at %.0f %.0f %.0f in %s", at.x, at.y, at.z, w.worldName.c_str());
}
catch (const zsdk::Fault &error) { zsdk::Log("template: %s", error.what()); }
catch (const zsdk::Refused &error) { zsdk::Log("template: refused, %s", error.what()); }
catch (const std::exception &error) { zsdk::Log("template: locate failed: %s", error.what()); }
```

## `zsdk/census.h`

Перепис: прохід масивом об'єктів гри в робочому потоці SDK, який знаходить екземпляр гри (game instance), хости статичних викликів і об'єкти-хости, названі в `App::census`.

**Потік:** `Current`, `Want`, `Epoch`, `Sweeps` і `Fresh`: будь-який. `Host` і `Consoles`: потік гри (вони порівнюють із живим тримачем світу). Прохід і хуки `census` та `complete` виконуються в робочому потоці.

**Стабільність:** Provisional

| Виклик | Що робить |
|---|---|
| `Current()` | останній знімок, `std::shared_ptr<const Snapshot>`, або `nullptr` до першого проходу |
| `Epoch()` | епоха світу, для якої працює перепис |
| `Sweeps()` | кількість завершених проходів |
| `Want(reason)` | просить про прохід; визначає час проходу лише з Legacy `CensusMode::OnDemand`, а з типовим `CensusMode::Interval` лише пише в журнал |
| `Fresh(snapshot, epoch, since)` | true, якщо знімок належить до епохи світу `epoch` і зроблений не раніше за `since` |
| `Host(slot)` | живий об'єкт-хост для `CensusSpec::hosts[slot]` зі свіжого знімка цього світу, не приречений, або `nullptr` |
| `Consoles()` | true, коли є всі перелічені хости; доти тримач світу чекає з `wait.consoles` |
| `kSlots` | 16, найбільша кількість статичних класів і хостів |
| `kInstanceCap` | 8, найбільша кількість екземплярів гри в одному знімку |

| Поле `Snapshot` | Що містить |
|---|---|
| `epoch` | епоха світу, у якій зроблено прохід |
| `at` | `Now()` на момент завершення проходу |
| `gi` | останній знайдений екземпляр гри |
| `gis` | до `kInstanceCap` екземплярів гри |
| `statics` | типовий об'єкт класу для кожної назви з `CensusSpec::statics` |
| `hosts` | живий об'єкт для кожної назви з `CensusSpec::hosts` |
| `sweepMs` | скільки тривав прохід |
| `objects` | кількість об'єктів у масиві |

Перепис робить прохід кожні 2 с, доки `hooks.complete` не скаже, що в знімку є все потрібне, потім кожні 10 с, а після зміни світу — одразу. `hooks.census` бачить кожен новий знімок нового світу і кожен знімок, поки той неповний. Хук `complete` стартового аддона і читання в потоці гри:

<!-- doccheck: present -->
```cpp
bool Complete(const zsdk::census::Snapshot &snapshot)
{
    return snapshot.gi && snapshot.statics[0] && snapshot.statics[1];
}

void Census()
{
    const std::shared_ptr<const zsdk::census::Snapshot> snap = zsdk::census::Current();
    if (snap && zsdk::census::Fresh(*snap, zsdk::world::Epoch(), 0)) zsdk::Note(1, "census: %d objects in %.1f ms, sweep %d", snap->objects, snap->sweepMs, zsdk::census::Sweeps());
}
```

## `zsdk/exe.h`

Відомості про exe гри, у якому працює аддон.

**Потік:** будь-який. `Hash()` читає весь файл exe: SDK запускає його один раз у робочому потоці одразу після того, як зв'яже рушій; аддони натомість читають `Sha()`, `Short()` і `Hashed()`.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `File()` | назва файлу exe, наприклад `Stalker2-Win64-Shipping.exe` |
| `Name()` | та сама назва друкованими символами ASCII |
| `Game(file)` | true для будь-якого `Stalker2-<name>-Shipping.exe`; в інші процеси `DllMain` аддон не завантажує |
| `Steam(file)`, `Steam()` | true для назви exe Steam-версії `kSteamFile`; на збірках з усіх інших крамниць аддон лишається вимкненим і пише «this game build is not supported» |
| `Base()` | база образу exe в пам'яті |
| `ImageSize()`, `Stamp()` | розмір образу і часова позначка із заголовка PE, відомі одразу |
| `Hash()` | обчислює SHA-256 файлу exe |
| `Sha()` | обчислений SHA-256 великими шістнадцятковими цифрами; до обчислення порожній |
| `Short()` | перші 8 шістнадцяткових цифр малими літерами (ідентифікатор збірки, наприклад `61bc1e03`) або `?` до обчислення |
| `Hashed()` | true, щойно хеш відомий |
| `Build()` | `guard::Build`, яким є цей exe, або `nullptr` |
| `Known()` | true, якщо `Build()` не `nullptr` |

З вердикту зразка FieldWatch:

<!-- doccheck: present body -->
```cpp
if (zsdk::exe::Hashed())
    zsdk::Log("fieldwatch: game build %s is %s", zsdk::exe::Short().c_str(), zsdk::exe::Known() ? "known to this ZoneSDK" : "not known to this ZoneSDK");
else if (!zsdk::exe::Steam())
    zsdk::Log("fieldwatch: %s is not the Steam build, game calls stay off", zsdk::exe::Name().c_str());
```

## `zsdk/glyphs.h`

Діапазони символів, які вміє малювати шрифт SDK.

**Потік:** будь-який.

**Стабільність:** Stable

| Назва | Що це |
|---|---|
| `Run` | діапазон кодових точок: `first` і `count` |
| `kRuns` | діапазони, які малюються: базова латиниця, Latin-1, апостроф U+02BC, кирилиця з українськими літерами (зокрема Ґ ґ), загальна пунктуація і № |
| `Drawable(point)` | true, якщо кодова точка є в `kRuns`; `constexpr` |
| `Undrawn(utf8)` | знаки тексту UTF-8, які не намалюються, як шматки `" U+XXXX"`, або порожній рядок |

Загальний набір `texts` перевіряє кожен мовний рядок через `Undrawn`.

<!-- doccheck: present body -->
```cpp
static_assert(zsdk::glyphs::Drawable(0x0490) && !zsdk::glyphs::Drawable(0x2603));
const std::string bad = zsdk::glyphs::Undrawn("Ґанок №5 — «так»");
if (!bad.empty()) zsdk::Log("these signs are not drawn:%s", bad.c_str());
```

## `zsdk/guard.h`

Захисти байтів і типізовані поля: як аддон користується сирим зсувом чи адресою exe гри лише тоді, коли exe досі містить очікувані байти. Захист, що не збігся, вимикає лише ті можливості, які від нього залежать.

**Потік:** `Check`, `Of`, `Ready`, `Usable`, `Use`, `Hex`, `Matches`, `Base`, `At`, `Target`, `Resolve` і `Find`: будь-який (вони читають байти образу через `ue::Copy`, який ніколи не падає). `Confirm` проходить даними рефлексії гри: потік гри або робочий потік. `Recheck`: його викликає SDK у потоці гри, коли з'являється хеш exe.

**Стабільність:** Stable

**Internal:** `guard::detail` (помічники часу компіляції, на яких тримаються докази полів).

| Поле `Build` | Що містить |
|---|---|
| `id` | ідентифікатор збірки, перші 8 шістнадцяткових цифр SHA-256 |
| `sha256` | повний SHA-256 exe |
| `imageSize`, `stamp` | розмір образу PE і часова позначка |
| `objectsRoot`, `objectsCount` | де в цій збірці лежить масив об'єктів рушія |

`k2bd3f115` (версія гри 2.1, збірка Steam 26123434) і `k61bc1e03` (версія гри 2.0.6) — це Steam-збірки, які знає ZoneSDK 1.0.0. `kBuilds` перелічує всі відомі збірки, у 1.0.0 це `{ &k61bc1e03, &k2bd3f115 }`; patch-випуск може додати рядки, тож не покладайтеся на його довжину чи порядок. `exe::Build()` і `Find` шукають лише там, а `kImageBase` — бажана база exe `0x140000000` (адреса `Anchor` задається відносно неї).

| Тип | Що це |
|---|---|
| `Guard` | одне перевірене місце: `rva` і `bytes`, пари шістнадцяткових цифр через пробіл, `??` для байта, який може відрізнятися |
| `Need` | що потрібно набору, крім байтів: `Bytes` (нічого), `ImageSize` (розмір образу своєї збірки), `KnownBuild` (розмір образу і SHA-256 exe своєї збірки; набір чекає, доки хеш стане відомим) |
| `Set` | іменована група захистів: `name` (унікальна, ніколи не `zsdk.*`), `build`, `guards`, `count`, `need` |
| `State` | вердикт набору: `Pending`, `Ok`, `Off` |
| `Report` | результат `Check`: скільки захистів збіглося (`ok` із `total`), `image` і `imageOk` (розмір образу і чи він відповідає збірці), `known` (SHA-256 дорівнює SHA-256 збірки), `bad` (RVA, що не збіглися), `state` |
| `Anchor` | шаблон, який розв'язується під час роботи: `va` (адреса для бази `kImageBase`), `bytes`, `disp` (де в шаблоні стоїть 32-бітне зміщення), `next` (де починається наступна інструкція) |
| `Proof` | як доведено `Field`: `Guard`, `Rip`, `Reflected`, `Unproven` |
| `Field` | типізований зсув або адреса з доказом |

| Член `Field` | Що містить |
|---|---|
| `name` | назва поля, для рядків журналу |
| `at` | зсув (або RVA для поля `Rip`) |
| `set`, `guard`, `offset` | набір, індекс захисту в ньому і позиція значення в байтах захисту |
| `reflectedClass`, `reflectedProperty` | клас і властивість, з якими звіряється поле з рефлексії |
| `proof` | `Proof` |
| `Field(name, at, set, guard, offset)` | поле із захистом; `consteval`: не компілюється, якщо байти захисту не містять `at` на позиції `offset` (4 байти little-endian або 1 байт, коли `at` менше за 0x80) |
| `Field(name, at, klass, property)` | поле з рефлексії; під час роботи `Confirm` порівнює `at` зі зсувом властивості в даних рефлексії гри |
| `Field::Rip(name, rva, set, guard, disp, next, addend)` | поле адреси; не компілюється, якщо RIP-відносне зміщення в захисті не веде до `rva` (плюс `addend`, типово 0) |
| `Field::Unproven(name, at)` | поле без доказу: компілюється, але ніколи не стає придатним; загальний набір `offsets` вимагає його назву з причиною в `tests/unproven-fields.txt` |
| `Raw()` | саме число; обхідний шлях для аддона, який перевіряє все сам, а набір `offsets` рахує кожне використання |

`Field` не перетворюється на ціле число: аддон отримує зсув лише через `Use`, `mem::Read` або `mem::Write`.

| Виклик | Що робить |
|---|---|
| `Check(set)` | перевіряє набір зараз і зберігає вердикт |
| `Of(set)` | збережений вердикт; набір, який ще `Pending`, перевіряється одразу, щойно хеш exe відомий |
| `Ready(set)` | true, щойно набір має вердикт (не `Pending`) |
| `Recheck()` | знову перевіряє кожен набір у стані `Pending`; SDK запускає це після обчислення хешу exe |
| `Usable(field)` | true, якщо поле можна використовувати: його набір `Ok` або перевірка за рефлексією пройшла |
| `Confirm(field)` | звіряє поле з рефлексії з даними рефлексії гри і зберігає результат |
| `Use(field, at)` | дає зсув в `at` і true, якщо поле придатне; інакше false і рядок журналу з причиною (щонайбільше раз на хвилину для кожного поля) |
| `Hex(bytes, out)` | розбирає текст захисту на значення байтів, -1 для `??` |
| `Matches(at, bytes)` | порівнює пам'ять за адресою з текстом захисту |
| `Base()` | база exe, як `exe::Base()` |
| `At(anchor)` | адреса якоря в цьому процесі |
| `Target(at, disp, next)` | RIP-відносна ціль інструкції за адресою `at` |
| `Resolve(anchor)` | ціль якоря, якщо його байти збігаються, інакше 0 |
| `Find(sha)` | відомий `Build` із цим SHA-256 або `nullptr` |
| `Own()` | власні набори SDK: по набору на кожну відому збірку для спостерігача за виходом із гри, `zsdk.exit.61bc1e03` і `zsdk.exit.2bd3f115`; спостерігач бере той, що збігається із запущеним exe, а загальний набір тестів `guards` перевіряє їх разом із наборами захисту аддона |

`src/fields.h` зразка FieldWatch (по одному набору захисту й одному полю із захистом на кожну відому збірку гри, одне поле з рефлексії):

<!-- doccheck: present group=camera -->
```cpp
namespace guard = zsdk::guard;
inline constexpr guard::Guard kCameraGuards61bc1e03[] = {
    { .rva = 0x23CA0E4, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00 0F 57 C9 0F 2E C1 77 22 48 8B 01 48 8B 80 A0 07 00 00 48 8D 15 ?? ?? ?? ?? 48 39 D0 75 11 48 81 C1 C0 17 00 00 F3 0F 10 41 30" },
};
inline constexpr guard::Guard kCameraGuards2bd3f115[] = {
    { .rva = 0x22B5CDE, .bytes = "48 83 EC 28 F3 0F 10 81 CC 02 00 00 0F 57 C9 0F 2E C1 77 22 48 8B 01 48 8B 80 A0 07 00 00 48 8D 15 ?? ?? ?? ?? 48 39 D0 75 11 48 81 C1 C0 17 00 00 F3 0F 10 41 30" },
};
inline constexpr guard::Set kCamera61bc1e03{ "fieldwatch.camera.61bc1e03", &guard::k61bc1e03, kCameraGuards61bc1e03, 1, guard::Need::KnownBuild };
inline constexpr guard::Set kCamera2bd3f115{ "fieldwatch.camera.2bd3f115", &guard::k2bd3f115, kCameraGuards2bd3f115, 1, guard::Need::KnownBuild };
inline constexpr guard::Field kDefaultFov{ "Camera.DefaultFOV", 0x2C8, "PlayerCameraManager", "DefaultFOV" };
inline constexpr guard::Field kLockedFov61bc1e03{ "Camera.LockedFOV", 0x2CC, kCamera61bc1e03, 0, 8 };
inline constexpr guard::Field kLockedFov2bd3f115{ "Camera.LockedFOV", 0x2CC, kCamera2bd3f115, 0, 8 };
inline constexpr const guard::Set *kSets[] = { &kCamera61bc1e03, &kCamera2bd3f115 };
inline constexpr const guard::Field *kLockedFovs[] = { &kLockedFov61bc1e03, &kLockedFov2bd3f115 };
```

Далі `App` зразка задає `a.sets = fields::kSets;`. Його `hooks.prime` один раз викликає `zsdk::guard::Confirm(fields::kDefaultFov)`. `LockedFOV` він читає через те поле з `kLockedFovs`, чий набір `zsdk::guard::Of` називає `Ok`, а його завдання показує вердикт цього набору; поки жоден набір не увімкнено, замість нього стоїть перше поле, і його читання відхиляється.

## `zsdk/hooks.h`

Таблиця зворотних викликів, яку аддон дає SDK. Кожне поле необов'язкове (`nullptr`); стартовий аддон підключає в `src/entry.cpp` ті, якими користується.

**Потік:** кожен хук виконується в потоці, зазначеному в таблиці нижче. `hooks::Bind` виконується в `DllMain`; `hooks::Get` — будь-який.

**Стабільність:** Stable

| Поле `Hooks` | Потік | Коли виконується і що робить |
|---|---|---|
| `boot()` | потік кадру | один раз, на першому кадрі, після того як задано теку даних і мову, і до запуску робочого потоку SDK; єдине місце для `job::Every`; коли він повертається, `shell::Ready()` стає true |
| `refresh(again)` | потік гри | під час підготовки і перед кожним кроком; перечитайте налаштування, якщо змінився `ini::Revision()`; `again` дорівнює true одразу після того, як з'явився хеш exe і набори захисту перевірено знову |
| `prime()` | потік гри | один раз, після того як рушій підключено, `verbs::Prime` виконано і перелічені набори захисту перевірено |
| `advance()` | потік гри | на кожному кадрі рушія, перед спостереженням за світом |
| `step(now)` | потік гри | раз за кадр рушія; основна робота аддона з грою |
| `hot()` | потік гри | після кожного кроку; повертайте true, поки аддону потрібен крок на кожному кадрі рушія і публікація після кожного кроку; поки він дає false і жоден наказ не чекає, SDK будить потік гри приблизно раз на секунду |
| `publish(now)` | потік гри | кожні 250 мс, а поки хук hot повертає true — на кожному кроці; скопіюйте стан, який показують інші потоки, під власним блокуванням аддона |
| `turn(turn)` | потік гри | коли змінюється світ (`world::Turn`); забудьте вказівники на старий світ |
| `order(order)` | потік гри | для кожного наказу від `thread::Ask`, щонайбільше 2 за крок; накази, старші за 2 с або подані до зміни світу, відкидаються |
| `orderName(order)` | потік гри | назва наказу для рядків журналу |
| `quit()` | потік гри або робочий | один раз, коли гра просить вийти; виклики рушія вже зупинено |
| `census(snapshot)` | робочий потік | після проходу нового світу і після кожного проходу, поки знімок неповний |
| `complete(snapshot)` | робочий потік | повертайте true, коли знімок містить усе, що потрібно аддону; без цього хука повним вважається знімок з екземпляром гри |
| `frame(input)` | потік кадру | на кожному кадрі; лише клавіші та швидкі перевірки, з `Input` цього кадру |
| `overlay(o)` | оверлей | на кожному кадрі, усередині події overlay від ReShade, незалежно від того, відкрито оверлей ReShade чи ні; малюйте HUD через `zsdk::paint`; після нього SDK малює підказки |
| `page(o)` | оверлей | вкладка аддона в оверлеї ReShade |
| `opened(open)` | потік кадру | коли оверлей ReShade відкривається або закривається; поза кадром ImGui, тож ImGui тут не можна |
| `stop()` | завантажувач | коли аддон вивантажується або процес завершується, після того як SDK зупинив власні потоки; всередині огорожі, де збій чи виняток завершує його без рядка в журналі |
| `words(item, ui)` | оверлей | текст підказки аддона мовою `ui`; без нього підказки аддона не малюються |
| `tone(item)` | оверлей | колір смужки підказки аддона (колір ImGui); типово `tab::kAccent` |
| `pipelineInit(device, layout, count, subobjects, pipeline)` | D3D | подія ReShade `init_pipeline` |
| `pipelineBind(list, stages, pipeline)` | D3D | подія ReShade `bind_pipeline`; гарячий шлях: тримайте його складність на рівні O(1) і без блокувань |
| `pipelineDestroy(device, pipeline)` | D3D | подія ReShade `destroy_pipeline` |
| `deviceDestroy(device)` | D3D | подія ReShade `destroy_device` |
| `report()` | оверлей | додаткові рядки для «Скопіювати звіт / Copy report» у «Діагностиці» |

Хуки GPU реєструються, лише якщо аддон їх задає. Дескриптори надходять як `uint64_t`, а маска стадій — як `uint32_t`. `hooks.h` лише оголошує `reshade::api::device`, `reshade::api::command_list` і `reshade::api::pipeline_subobject`; щоб ними користуватися, підключіть заголовки ReShade `reshade_api*.hpp` (ніколи не `reshade.hpp`). Хуки D3D виконуються паралельно в кількох потоках: там не викликайте `Fire` і `world::Hold`, не користуйтеся ImGui, не відкривайте вікон збою і не пишіть в ini.

`quit` виконується один раз за процес, у потоці гри або в робочому потоці SDK, ніколи одночасно зі `step`. Він запускається, коли гра ставить власний прапорець виходу (його читають лише там, де набір захисту відомої збірки, `2bd3f115` або `61bc1e03`, збігається із запущеним exe) або коли хук бачить `WM_QUIT`. У ньому немає `Fire`, `Call` і `mem::Write`; він може ненадовго записати файл, взяти власні блокування і сказати власній роботі зупинитися. Коли жоден такий набір не збігається, наприклад на збірці гри, якої SDK не знає, лишається тільки `WM_QUIT`, а ця гра надсилає його рідко, тож усьому, що має пережити будь-який вихід, у `quit` не місце.

`stop` виконується під блокуванням завантажувача Windows. Під час виходу з процесу (`shell::Exiting()` дає true) усіх інших потоків уже немає: не беріть блокувань, нічого не чекайте, не пишіть файлів і не викликайте функцій рушія.

Кожен хук захищено. Виняток C++ з хука перехоплюється і пишеться в журнал; `stop` відкидає обидва види винятків без рядка в журналі, бо виконується під блокуванням завантажувача. Структурований виняток (наприклад, порушення доступу) у хуку потоку гри пишеться в журнал як «structured exception 0x<code> at <module>+0x<offset> in the game thread tick», а з `Policy::haltOnSeh` (типово) зупиняє всі виклики рушія до кінця сесії (`Phase::Halted`). Структурований виняток у `census` чи `complete` вимикає обидва хуки до кінця сесії і вимикає ключ реєстру `census.hook` до «Спробувати ще / Try again».

Кожен зворотний виклик ReShade аддона виконується всередині огорожі SDK, у власній лінії. Структурований виняток там вимикає лінію до кінця сесії, пише в журнал один рядок «structured exception 0x<code> at <module>+0x<offset> in the <lane> lane, <scope> for this session and present.<lane> is disabled until Try again», де <scope> — «this lane is off», «the frame and toasts lanes are off» або «the add-on is off», і одразу скидає журнал на диск; виклик через нульовий вказівник названо як «a call through a null pointer from <module>+0x<offset>». Ключ реєстру `present.<lane>` лишається вимкненим і в наступних сесіях, до «Спробувати ще / Try again». Виняток C++ у зворотному виклику, як і раніше, перехоплює внутрішній обробник і пише в журнал як «<lane> error: ...» щонайбільше раз на хвилину для кожної лінії, а лінія лишається увімкненою. Кожен модуль аддона збирається з `/EHa`, тож блокування, які тримав зупинений код, звільняються. Потік зі стеком від 1 МБ при першому вході в огорожу отримує гарантію стека 32 КБ, тож переповнення стека у зворотному виклику теж зупиняє огорожа.

| Лінія | Ключ реєстру | Що в ній виконується |
|---|---|---|
| boot | `present.boot` | запуск аддона на першому кадрі: ini, мова, `boot` і потоки SDK |
| core | `present.core` | власна робота SDK на кожному кадрі: хук потоку гри, нагляд за помпою і пробудження помпи |
| frame | `present.frame` | `frame` з `Input` цього кадру |
| data | `present.data` | такт налаштувань: відкладене збереження і запис журналу на диск |
| block | `present.block` | обслуговування текстур шрифтів SDK перед оверлеєм |
| overlay | `present.overlay` | `overlay` |
| toasts | `present.toasts` | підказки |
| page | `present.page` | `page` |
| open | `present.open` | `opened` |
| device | `present.device` | SDK звільняє свої текстури шрифтів цього пристрою на `destroy_device` |
| device destroy | `present.deviceDestroy` | `deviceDestroy` |
| runtime | `present.runtime` | скидання підказок на `destroy_effect_runtime` |
| save | `present.save` | збереження налаштувань і запис журналу на диск на `destroy_effect_runtime` |
| pipeline init, bind, destroy | `present.pipelineInit`, `present.pipelineBind`, `present.pipelineDestroy` | `pipelineInit`, `pipelineBind`, `pipelineDestroy` |

Збій у лінії frame чи toasts вимикає обидві. Лінія data виконує лише код SDK і вимикається сама. Збій у boot чи core вимикає весь аддон, крім ліній device, runtime і save, щоб SDK усе одно прибрав за собою і зберіг налаштування наприкінці. Поки лінія page вимкнена, вкладка аддона показує власний текст SDK `sdk.lanes` з назвою аддона і вимкненими частинами (їхні рядки `lane.*`), шлях до журналу, кнопку «Спробувати ще» і «Скопіювати звіт / Copy report», а після збою в boot чи core — ще й `sdk.restart`. «Спробувати ще» одразу знову вмикає лінії; після збою в boot чи core в тій самій сесії сам аддон знову запуститься з наступним запуском гри. Наступний запуск, у якому ключі ще вимкнені, пише в журнал `safety: the <lanes> lanes are disabled after a crash in an earlier session, Try again in the tab clears them`.

Хуки GPU і `deviceDestroy` можуть прийти ще до першого кадру. Тоді SDK спершу відкриває теку даних, у лінії boot, тож ключ, вимкнений у попередній сесії, діє і там. Лінія, що впала, поки тека даних ще не відкрита, лишається вимкненою, щойно її відкрито, з рядком `safety: the <lanes> lanes faulted before the data folder opened, they stay disabled`.

`hooks::Bind(hooks)` зберігає таблицю (його викликає `shell::Main`), а `hooks::Get()` її повертає. Хуки — це іменовані функції: загальний набір `present` не пропускає лямбд і перевіряє, що `page` і `overlay` вказують на функції оверлею, а решта — на функції модулів кадру. `src/entry.cpp` стартового аддона:

<!-- doccheck: entry -->
```cpp
zsdk::Hooks Wire()
{
    zsdk::Hooks h;
    h.refresh = &ztemplate::core::Refresh;
    h.step = &ztemplate::core::Step;
    h.order = &ztemplate::core::Order;
    h.frame = &ztemplate::frame::Frame;
    h.page = &ztemplate::page::Page;
    h.words = &ztemplate::page::Words;
    return h;
}
```

## `zsdk/ini.h`

Налаштування і файли даних аддона. Одна функція-відвідувач описує структуру налаштувань; SDK читає її, обмежує значення, записує і порівнює, зберігає невідомі ключі і тримає реєстр збоїв у тому самому файлі ini.

**Потік:** будь-який (одне блокування). `Open`, `Close` і `Tick` належать SDK: `Open` — під час запуску аддона, `Tick` — на кожному кадрі (він скидає журнал кожні 500 мс і зберігає змінені налаштування через 1 с після останньої зміни), `Close` — під час вивантаження. Файловим викликам (`Save`, `Store`, `WriteAtomic`, `ReadFile`) не місце в хуках D3D і в `stop`.

**Стабільність:** Stable

**Internal:** `ParseRaw`, `TextRaw`, `SanitizeRaw`, `ReadRaw`, `ApplyRaw` (нетипізована робота, що стоїть за типізованими викликами нижче).

Структура налаштувань — це прості дані з `operator==`. Аддон описує її однією функцією, яка викликає `Visitor`:

| Виклик `Visitor` | Налаштування |
|---|---|
| `Flag(section, key, value)` | `bool`, записується як `1` або `0` (читаються також `true`, `yes` і `on`) |
| `Whole(section, key, value, def, low, high)` | `int` у межах `low`..`high` |
| `Real(section, key, value, def, low, high)` | `double` у межах `low`..`high` (`def`, якщо це не число), записується через `Real` |
| `Word(section, key, value, def, choices, count)` | рядок; з `choices` — одне з `count` слів малими літерами, інакше `def`; без них — вільний текст |
| `Pick(section, key, value, names, count, def)` | перелік, що зберігається за назвою |
| `Key(section, key, value, def)` | `keys::Chord`, записується назвою на кшталт `ALT+L`; `none` — без клавіші, `def` — для клавіші, яку не можна призначити |
| `Text(section, key, value, most)` | вільний текст до `most` байтів (щонайбільше `Limits::kTextMost`), без керівних символів |

Типові значення — це власні ініціалізатори членів структури налаштувань: відсутній або нечитабельний ключ їх не змінює. `Visitor` реалізує SDK (у нього є віртуальний деструктор, `~Visitor`); функція обходу аддона лише викликає його. `Choice(v, section, key, value, names, def)` — це `Pick` для `enum class` з масивом назв. Назви секцій і ключів пишуться малими літерами. Секції `safety` і `proven` та власні секції аддона зарезервовано.

| Поле `Schema` | Що містить |
|---|---|
| `head` | секція для ключів, що стоять до першого рядка `[section]`, `"general"` |
| `visit` | функція-відвідувач |
| `fix` | виконується після розбору і після `Apply`, щоб виправити значення, які залежать одне від одного |
| `make`, `drop`, `copy`, `same` | створення, видалення, копіювання і порівняння структури налаштувань зі стертим типом |
| `level` | дає рівень журналу (від 0 до 2) з налаштувань |
| `language` | дає код мови (`auto`, `uk`, `en`) з налаштувань |
| `owned`, `ownedCount` | секції `Owned`, які аддон тримає як прості словники ключ/значення |

`Make<S, Visit, Fix, Level, Language>(head, owned, ownedCount)` будує `constexpr` `Schema` з типізованих функцій. Він користується `Typed<S>` (з `Make`, `Drop`, `Copy`, `Same`) і адаптерами `VisitThunk`, `FixThunk`, `LevelThunk` та `LanguageThunk`. Секція `Owned` має `name` і `cap` (найбільшу кількість ключів, яку вона тримає).

| Виклик | Що робить |
|---|---|
| `Config<S>()` | копія чинних налаштувань |
| `Apply<S>(settings)` | обмежує налаштування (відвідувач, потім `fix`) і зберігає їх, якщо вони змінилися; на диск вони потрапляють приблизно через 1 с |
| `Revision()` | число, яке зростає, коли змінюються налаштування (`Apply`), замінюється власна секція (`Store`; `Put` його не збільшує), змінюється список вимкнених ключів або викликано `Touch`; порівнюйте його, щоб дешево перечитувати налаштування |
| `Touch()` | збільшує `Revision()` без зміни |
| `Dirty()` | true, поки змінені налаштування чекають на збереження |
| `Save()` | записує ini зараз |
| `Parse<S>(text, settings, keep)` | читає текст ini у структуру налаштувань, а решту секцій — у `Kept`, якщо передано `keep` |
| `Text<S>(settings, keep)` | текст ini (рядки CRLF) з налаштувань і `Kept` |
| `Sanitize<S>(settings)` | обмежує значення копії |
| `Check(make)` | кидає `std::logic_error`, якщо тип структури відрізняється від прив'язаної схеми; ним користуються типізовані виклики |
| `Bound()` | прив'язана схема (`App::schema`) |
| `Language()` | код мови з налаштувань або `auto` |
| `Ledger()` | копія блоку реєстру збоїв, `Kept` |
| `Folder()`, `LogPath()`, `ExitPath()` | тека даних, файл журналу і файл мітки виходу (`<ini name>.exit`) |
| `Open(folder)` | відкриває теку даних (типово `%LOCALAPPDATA%\<Identity::folder>`), читає ini і оцінює минулий запуск; тести передають власну теку |
| `Close()` | зберігає і скидає на диск |
| `Tick()` | файловий такт SDK |
| `Section(name)` | власна секція як `std::map` |
| `Store(name, values)` | замінює власну секцію і одразу записує ini |
| `Put(name, key, value)` | задає один ключ власної секції (щонайбільше `cap` ключів, 16, якщо секцію не перелічено); зберігається пізніше |
| `Stamp()` | місцевий час як `YYYY-MM-DD HH:MM` |
| `Real(value)` | `double` як текст, до 4 знаків після коми, без кінцевих нулів і без `-0` |
| `WriteAtomic(path, body)` | записує файл через тимчасовий файл, який скидається на диск і перейменовується поверх старого |
| `ReadFile(path, out, limit)` | читає весь файл розміром до `limit` байтів (типово 1 МіБ) |

Блок реєстру збоїв в ini — це `Kept`: `session` (id цього запуску), `pending` (виклик, для якого стоїть мітка збою), `disabled`, `watch` (відкриті вікна збою), `proven` (ключ і ідентифікатор збірки, для якої його доведено), `owned` (власні секції), `unknown` (невідомі ключі, кожен — `Entry` із `section`, `key` і `value`) і `addon` (збірка аддона, яка записала докази). `Kept` і `Entry` порівнюються через `==`. `Limits` містить межі: `kUnknownKept` — 64 невідомі ключі, `kDisabledKept` — 32 вимкнені ключі, `kWatchKept` — 8 і `kWatchCap` — 32 вікна збою, `kTextMost` — 256 байтів вільного тексту.

`src/settings.cpp` стартового аддона:

<!-- doccheck: present group=settings -->
```cpp
constexpr const char *kLanguages[] = { "auto", "uk", "en" };

void Visit(Settings &s, zsdk::ini::Visitor &v)
{
    v.Word("general", "language", s.language, "auto", kLanguages, 3);
    v.Flag("general", "enabled", s.enabled);
    v.Flag("general", "toasts", s.toasts);
    v.Whole("general", "log_level", s.logLevel, 1, 0, 2);
    v.Key("keys", "locate", s.locate, Settings{}.locate);
}
```

<!-- doccheck: present group=settings -->
```cpp
void Fix(Settings &) {}
int Level(const Settings &s) { return s.logLevel; }
const char *Language(const Settings &s) { return s.language.c_str(); }
constexpr zsdk::ini::Owned kOwned[] = { { "memory", 16 } };
constexpr zsdk::ini::Schema kSchema = zsdk::ini::Make<Settings, Visit, Fix, Level, Language>("general", kOwned, 1);
```

Тоді свіжий ini стартового аддона має такий вигляд:

```ini
[general]
language=auto
enabled=1
toasts=1
log_level=1

[keys]
locate=ALT+L

[safety]
session=<32 hex digits>
pending=
disabled=
watch=
addon=0.1.0/<8 hex digits>

[proven]

[memory]
```

## `zsdk/input.h`

Клавіатура і миша такими, якими їх бачить ReShade у цьому кадрі, і чи призначена клавіша для аддона.

**Потік:** потік кадру. `Input`, переданий у `hooks.frame`, чинний лише під час цього виклику.

**Стабільність:** Stable

| Виклик `Input` | Що робить |
|---|---|
| `Down(vk)`, `Pressed(vk)`, `Released(vk)` | віртуальну клавішу утримують, натиснули в цьому кадрі, відпустили в цьому кадрі |
| `MouseDown(button)`, `MousePressed(button)` | кнопку миші утримують, натиснули в цьому кадрі |
| `Cursor(x, y, wheel)` | положення курсора і, якщо передано `wheel`, рух коліщатка |
| `Async(vk)` | стан клавіші від Windows (`GetAsyncKeyState`) |
| `Mods()` | утримувані модифікатори як біти `keys::kCtrl`, `kShift`, `kAlt` |
| `Fires(chord)` | сполучення натиснули в цьому кадрі з правильними модифікаторами (`keys::Fires`) |
| `Focused()` | вікно гри у фокусі |
| `Overlaid()` | оверлей ReShade відкрито |
| `Menu()` | гравець у меню: гру поставлено на паузу або системний курсор показано і він стоїть стабільно (400 мс без Alt, Ctrl чи Shift і з закритим оверлеєм) |
| `Paused()` | гра на паузі |

Реагуйте на клавіші лише тоді, коли `Focused()`, і не тоді, коли `Overlaid()` чи `Menu()`, а роботу передавайте потоку гри через `thread::Ask`. `src/frame.cpp` стартового аддона:

<!-- doccheck: present -->
```cpp
void Keys(const zsdk::Input &input)
{
    static uint64_t revision = 0;
    static Settings s;
    if (zsdk::ini::Revision() != revision)
    {
        revision = zsdk::ini::Revision();
        s = zsdk::ini::Config<Settings>();
    }
    if (!s.enabled || !input.Focused() || input.Overlaid() || input.Menu()) return;
    if (input.Fires(s.locate)) zsdk::thread::Ask(static_cast<int>(Order::Locate));
}
```

## `zsdk/job.h`

Фонова робота без власних потоків: періодичні завдання, які виконуються в робочому потоці SDK цього аддона.

**Потік:** `Every`: лише з `hooks.boot`. Функції завдань виконуються в робочому потоці. `Stopping` і `Off`: будь-який.

**Стабільність:** Provisional

| Виклик | Що робить |
|---|---|
| `Every(name, periodMs, run)` | реєструє `run(now)`, щоб той запускався щонайчастіше раз на `periodMs`; false (з рядком журналу), якщо реєстрацію відхилено; період — це мінімум, огрублений тактом робочого потоку в 100 мс |
| `Stopping()` | true, поки SDK зупиняється, після того як гра попросила вийти, і під час виходу з процесу; довге завдання перевіряє його і повертається |
| `Off(name)` | true, якщо завдання вимкнено (до кінця сесії або після збою) |
| `kMost` | 8, найбільша кількість завдань одного аддона |
| `kShortestMs` | 100, найкоротший період |
| `kBudgetMs` | 50, час, який один прохід може витратити на запуск завдань; решта завдань чекає наступного проходу |
| `kSlowMs` | 50, запуск, довший за це, потрапляє в журнал |
| `kStallMs` | 1000, запуск, довший за це, вимикає завдання до кінця сесії |

Назва завдання має від 1 до 32 символів: літер, цифр, `.`, `_` чи `-`. Завдання виконуються одне за одним на будь-якій збірці гри, зокрема й там, де рушій не зв'язано. Завдання не робить викликів гри (`Call` і `Fire` поза потоком гри відхиляються): свій результат воно передає потоку гри під власним блокуванням і через `thread::Ask`. Структурований виняток у завданні вимикає його до кінця сесії і вимикає ключ реєстру `job.<name>` до «Спробувати ще / Try again»; виняток C++ пишеться в журнал щонайбільше раз на хвилину, а завдання зберігає свій період. Завдання, яке ніколи не повертається, ніщо не зупинить: через `kStallMs` SDK один раз пише в журнал «job <name> has not returned for <n> ms, the SDK worker waits for it», і доки воно не повернеться, не виконується жодне інше завдання, крок перепису чи перевірка виходу. Загальний набір `hardrules` не пропускає у вихідних файлах аддона власних потоків, таймерів і зворотних викликів пулу потоків і радить натомість `job::Every`. Завдання вердикту зразка FieldWatch:

<!-- doccheck: present -->
```cpp
void Judge(uint64_t)
{
    if (zsdk::job::Stopping() || zsdk::job::Off("fieldwatch.verdict")) return;
    zsdk::Note(2, "fieldwatch: the game build is %s", zsdk::exe::Known() ? "known" : "not known yet");
}

void Boot()
{
    if (!zsdk::job::Every("fieldwatch.verdict", 500, &Judge)) zsdk::Log("fieldwatch: the verdict job was refused");
}
```

## `zsdk/keys.h`

Сполучення клавіш: розбір, назви, показ і зіставлення клавіш так само, як у кожному аддоні ZoneSDK.

**Потік:** будь-який.

**Стабільність:** Stable

`Chord` — це віртуальна клавіша `key` і біти модифікаторів `mods` (`kCtrl`, `kShift`, `kAlt`); сполучення порівнюються через `==`.

| Виклик | Що робить |
|---|---|
| `Parse(text)` | перетворює `"ALT+K"`, `"CTRL+SHIFT+F7"`, `"PAGEUP"` тощо на сполучення; невідомий текст або `NONE` дає порожнє сполучення |
| `Name(chord)` | назва для збереження, наприклад `ALT+K`, або `none` |
| `Shown(chord, lang)` | назва, яку бачить гравець, наприклад `Alt+K` |
| `Glyph(key, lang)` | показувана назва однієї клавіші; українською клавіші символів називають ще й українську літеру, яка на них є |
| `Known(key)` | клавіша має назву |
| `Modifier(key)` | Shift, Ctrl, Alt або Windows |
| `Bindable(chord)` | відома клавіша, яка не є модифікатором, Esc чи Tab |
| `Game(key)` | гра сама використовує цю клавішу (рух, зброя, Space, Tab, =, F5, F8 та інші) |
| `Numpad(key)` | клавіша цифрового блока; ZoneSDK ніколи не бере таку типовою, бо на багатьох клавіатурах його немає |
| `Mods(ctrl, shift, alt)` | біти модифікаторів із трьох прапорців |
| `Match(chord, key, mods)` | правило Alt: сполучення з модифікаторами потребує саме їх; проста клавіша спрацьовує з утримуваним Ctrl чи Shift, але не з Alt |
| `Exact(chord, key, mods)` | модифікатори мають збігатися точно |
| `Fires(chord, key, mods)` | `Match` або `Exact`, як каже `Policy::mods` |
| `Which(next, back, key, mods)` | 1 для `next`, -1 для `back`, 0, якщо жодне, для пари сполучень, що можуть мати спільну клавішу |

Назви: `A`-`Z`, `0`-`9`, `F1`-`F24`, `INSERT`, `DELETE`, `HOME`, `END`, `PAGEUP`, `PAGEDOWN`, `TAB`, `ENTER`, `SPACE`, `BACKSPACE`, `UP`, `DOWN`, `LEFT`, `RIGHT`, `CAPSLOCK`, `SCROLLLOCK`, `PAUSE`, `OEM102`, символи `. , - = [ ] ; ' / \` і зворотний апостроф (backquote), а також клавіші цифрового блока `NUM0`-`NUM9`, `NUMMULTIPLY`, `NUMADD`, `NUMSUBTRACT`, `NUMDECIMAL` і `NUMDIVIDE`.

<!-- doccheck: present body -->
```cpp
const zsdk::keys::Chord chord = zsdk::keys::Parse("ALT+K");
const bool fine = zsdk::keys::Bindable(chord) && !zsdk::keys::Numpad(chord.key) && !zsdk::keys::Game(chord.key);
zsdk::Log("%s, shown as %s, %s", zsdk::keys::Name(chord).c_str(), zsdk::keys::Shown(chord, zsdk::lang::Lang::Uk).c_str(), fine ? "a good default" : "pick another key");
const bool fires = zsdk::keys::Fires(chord, 'K', zsdk::keys::Mods(false, false, true));
zsdk::Note(2, "Alt+K fires: %s", fires ? "yes" : "no");
```

## `zsdk/lang.h`

Дві мови кожного аддона ZoneSDK, українська та англійська, і текстові рядки.

**Потік:** будь-який. `Detect()` один раз читає налаштування гри і кешує результат; SDK запускає його під час запуску аддона.

**Стабільність:** Stable

`Row` має `id` і по одному `text` для кожної мови, українська перша: `{ "say.located", { "Ви тут: {0}", "You are here: {0}" } }`. `Lang` має значення `Uk`, `En` і `Count`.

| Виклик | Що робить |
|---|---|
| `Current()` | мова, що діє зараз |
| `Set(lang)` | задає її |
| `Set(code)` | `uk` або `en`; `auto`, порожній, невідомий чи російський код — усі вибирають `Detect()` |
| `Detect()` | мова гри: uk дає `Uk`, будь-яка інша мова — `En`; визначається один раз з налаштувань гри, потім зі Steam, потім з Windows, і пишеться в журнал |
| `Detect(roots)` | те саме рішення для заданих `paths::Roots`, без кешу, для тестів |
| `Code(lang)` | `uk` або `en` |
| `Valid(code)` | true для `auto`, `uk` і `en` |
| `Text(id)`, `Text(id, lang)` | текст рядка поточною або заданою мовою: спершу рядок аддона, потім типовий рядок SDK; `?` для невідомого id |
| `Has(id)` | рядок із цим id існує |
| `Ids()` | id власних рядків аддона |
| `Defaults()` | типові рядки SDK (`phase.*`, `diag.*`, `key.*`, `wait.consoles`, `sdk.unsupported`, `sdk.lanes`, `sdk.restart`, `lane.*`, `yes`, `no`) |
| `Format(pattern, args)` | підставляє аргументи в заповнювачі від `{0}` до `{99}`; два перевантаження: список або вектор |
| `Slots(pattern)` | номери заповнювачів, які використовує шаблон |
| `Russian(utf8)` | true, якщо текст містить літеру, яка є лише в російській абетці, або російське слово; ним користуються загальний набір `texts` і перевірка випуску |

Російської мови як варіанта немає: будь-яка мова, крім української, дає англійську. Загальний набір `texts` перевіряє, що обидва тексти рядка є, мають ті самі заповнювачі і малюються шрифтом SDK. Рядки стартового аддона і текст підказки:

<!-- doccheck: present -->
```cpp
constexpr zsdk::lang::Row kRows[] = {
    { "say.located", { "Ви тут: {0}", "You are here: {0}" } },
    { "why.world", { "світ гри ще не готовий", "the game world is not ready" } },
};

std::string Located(const std::string &where)
{
    return zsdk::lang::Format(zsdk::lang::Text("say.located"), { where });
}
```

## `zsdk/ledger.h`

Реєстр збоїв: які виклики гри і вікна роботи з пам'яттю виконувалися, які з них призвели до збою гри і лишаються вимкненими, і чи чисто завершився минулий запуск. Він живе в секціях `[safety]` і `[proven]` ini аддона.

**Потік:** будь-який (одне блокування; виклики, що змінюють реєстр, записують ini). `Window` навколо роботи з пам'яттю гри відкривайте в потоці гри.

**Стабільність:** Stable

**Internal:** `ledger::Calm` (ним послідовність виходу SDK закриває відкриті вікна).

Як це працює: перш ніж захищений виклик чи вікно роботи з пам'яттю вперше виконається на збірці гри, його ключ записується в ini. Коли виклик повертається, ключ доведено для цієї збірки. Якщо гра падає, поки ключ записано, наступний запуск вимикає цей ключ: можливість лишається вимкненою, доки гравець не натисне «Спробувати ще / Try again» у «Діагностиці» (`Retry`). Чистий вихід із гри лишає поруч з ini мітку виходу, тож звичайний вихід ніколи не сприймається як збій.

| Виклик | Що робить |
|---|---|
| `Blocked(key)` | true, якщо ключ вимкнено після збою |
| `Disabled()` | усі вимкнені ключі |
| `Booted()` | ключі, вимкнені під час цього запуску (гра завершилася, поки вони були відкриті) |
| `Retry()` | очищає список вимкнених; його викликає «Діагностика» |
| `Full()` | список вимкнених містить 32 ключі; новий ключ витісняє найстаріший |
| `Proven(key)` | ідентифікатор збірки, для якої доведено ключ, або порожньо |
| `Preloaded(key)`, `Preloaded(key, build)` | ключ доведено в списку SDK для цієї (або заданої) збірки, `data/builds/<id>/proven.txt` |
| `PreloadCount()` | кількість ключів, доведених наперед |
| `Watched()` | ключі вікон збою, відкритих зараз |
| `Watch(key)`, `Unwatch(key, build)` | відкривають і закривають вікно збою вручну; `Window` робить це за вас |
| `WatchLimit()` | найбільша кількість відкритих вікон, `Policy::watchKept` (від 1 до 32, інакше 8) |
| `Addon()` | збірка аддона, `<version>/<8 hex digits of the image time stamp>`; докази іншої збірки відкидаються під час запуску |
| `CleanStart()` | минулий запуск завершився чистою міткою виходу |
| `Last()`, `Current()` | id сесії минулого і цього запуску |
| `Closed()` | вікна, які минулий запуск лишив відкритими і які закрив чистий вихід |
| `Stuck()` | стару мітку виходу не вдалося прибрати |

`Window` — це вікно збою навколо відрізка роботи з пам'яттю гри:

| Виклик `Window` | Що робить |
|---|---|
| `Window(key)` | відкриває вікно: записує ключ як спостережуваний, якщо його ще не доведено для цієї збірки; відхиляється, якщо ключ вимкнено або його неможливо записати |
| `Close()` | закриває вікно і доводить ключ для цієї збірки |
| `~Window()` | вікно, знищене без `Close`, припиняє спостереження без доказу |
| `Open()`, `Armed()`, `Refused()` | вікно відкрито, його ключ записано, його відхилено |
| `Key()` | його ключ |

`Window` не можна копіювати. Ключі — це короткі назви `Addon.Feature`; ключ, у якому впала гра, видно в журналі, у «Діагностиці» і у звіті. Хук prime стартового аддона:

<!-- doccheck: present body -->
```cpp
for (const std::string &key : zsdk::ledger::Booted()) zsdk::Log("template: %s was disabled at this boot, Try again in the tab clears it", key.c_str());
if (zsdk::ledger::Blocked(kLocateKey)) zsdk::Log("template: the locate call stays off");
if (!zsdk::ledger::CleanStart()) zsdk::Note(1, "template: the last run %s did not end cleanly", zsdk::ledger::Last().c_str());
const std::string proof = zsdk::ledger::Proven(kLocateKey);
zsdk::Note(2, "template: %s proven for build %s", kLocateKey, proof.empty() ? "-" : proof.c_str());
```

## `zsdk/log.h`

Файл журналу аддона в його теці даних.

**Потік:** будь-який. Рядки чекають у черзі (щонайбільше 4096; найстаріші відкидаються і рахуються), і SDK записує їх кожні 500 мс із такту кадру. Під час виходу з процесу виклики лише пробують узяти свої блокування і відкидають рядок, якщо блокування зайняте.

**Стабільність:** Stable

**Internal:** `log::Hushed`, `log::Due` (розклад, за яким SDK скидає журнал).

| Виклик | Що робить |
|---|---|
| `Log(shape, ...)` | один рядок у стилі `printf`, пишеться завжди; рядок має щонайбільше 4095 байтів |
| `Note(level, shape, ...)` | рядок, який пишеться, лише коли `level` не перевищує рівня журналу (`log_level` у налаштуваннях, від 0 до 2, типово 1) |
| `Rare(tag, shape, ...)` | рядок, що пишеться щонайбільше раз на 60 с для кожного `tag`; наступний такий рядок каже, скільки разів він повторився |
| `Flush()` | записує рядки з черги зараз |
| `Urgent()` | змушує наступний файловий такт записати одразу |
| `log::Path()` | файл журналу |
| `log::Level()`, `log::SetLevel(level)` | рівень журналу; SDK задає його з налаштувань |
| `log::Pending()`, `log::Dropped()` | рядки в черзі і рядки, відкинуті через переповнену чергу |
| `log::Open(file, rolled)`, `log::Roll(file, rolled)` | належать SDK: відкривають журнал і переносять його під назву попереднього журналу (за 8 МіБ під час роботи, за 4 МіБ на старті) |
| `log::Masked(utf8)` | текст, у якому приховано `%LOCALAPPDATA%`, `%USERPROFILE%` та ім'я користувача Windows |
| `log::Shown(path)` | шлях, оброблений так само, для рядків журналу і вкладки |

Рядок має вигляд `12:34:56.789 [1234] text`, з id потоку в дужках.

<!-- doccheck: present body -->
```cpp
zsdk::Log("template: located at %s", "1 2 3");
zsdk::Note(2, "template: detail only at log level 2, now %d", zsdk::log::Level());
zsdk::Rare("template locate", "template: locate failed again");
zsdk::Log("template: the log is %s", zsdk::log::Shown(zsdk::log::Path()).c_str());
```

## `zsdk/math.h`

Вектори й обертання в одиницях гри: сантиметри і градуси, X уперед, Y праворуч, Z угору, рискання (yaw) навколо Z.

**Потік:** будь-який.

**Стабільність:** Stable

`Vec3` має `x`, `y`, `z`; `Rot3` має `pitch`, `yaw`, `roll`; обидва порівнюються через `==` і відповідають полям типу double у `FVector` і `FRotator` гри, тож `Call::Get<zsdk::math::Vec3>` читає результат-розташування.

| Виклик | Що робить |
|---|---|
| `Add(a, b)`, `Sub(a, b)`, `Mul(a, k)` | сума і різниця векторів, множення на число |
| `Dot(a, b)`, `Cross(a, b)` | скалярний і векторний добутки |
| `Length(v)`, `Unit(v)` | довжина; одиничний вектор (`{0, 0, 1}` для нульового вектора) |
| `Flat(a, b)` | відстань по землі, без Z |
| `Wrap(yaw)` | кут у межах -180..180 |
| `Forward(heading)`, `Left(heading)`, `Right(heading)` | напрямки по землі для кута рискання |
| `Across(pos, heading, f, l)` | точка на землі на `f` попереду і на `l` ліворуч |
| `Ahead(rot)` | напрямок, куди дивиться обертання |
| `Axes(rot, x, y, z)`, `FromAxes(x, y, z)` | обертання в три його осі і назад |
| `Attitude(heading, pitch, roll)` | обертання, яке повертає меш, у якого «вперед» — вісь +Y, у бік `heading`, з `pitch` і `roll` |
| `Compose(outer, inner)` | обертання `inner`, застосоване всередині `outer` |
| `Place(origin, rot, flu)`, `Local(origin, rot, world)` | зсув «уперед, ліворуч, угору» в системі такого обертання з «уперед» по +Y — у точку світу, і назад: `flu.x` іде вздовж осі Y обертання, `flu.y` — уздовж його осі X, `flu.z` — уздовж Z |
| `Quarter(meshYaw, c, s)` | косинус і синус кута рискання меша, точні для 0, 90, 180 і 270 |
| `MeshHalf(f, l, u, meshYaw)` | половинні розміри коробки, поверненої на кут рискання меша |

<!-- doccheck: present body -->
```cpp
const zsdk::math::Vec3 at{ 100.0, 200.0, 50.0 };
const zsdk::math::Rot3 rot{ 0.0, zsdk::math::Wrap(270.0), 0.0 };
const zsdk::math::Vec3 ahead = zsdk::math::Add(at, zsdk::math::Mul(zsdk::math::Ahead(rot), 300.0));
zsdk::Note(2, "3 m ahead is %.0f %.0f, %.0f cm away on the ground", ahead.x, ahead.y, zsdk::math::Flat(at, ahead));
```

## `zsdk/mem.h`

Сира пам'ять гри через типізовані поля. Читання йде через безпечне копіювання SDK; запис потребує відкритого вікна збою в потоці гри.

**Потік:** `Read`: будь-який (він ніколи не падає), зазвичай потік гри. `Write`, `Put`, `Poke` і `Writable`: потік гри, усередині помпи.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Read(base, field, out)` | читає `out` за адресою `base` плюс зсув поля; false, якщо поле непридатне (`guard::Use`) або пам'ять не читається |
| `Write(window, base, field, value)` | записує `value` за адресою `base` плюс зсув поля |
| `Write(window, at, value)` | записує за адресою |
| `Put(window, at, from, bytes)` | записує сирі байти |
| `Writable(window)` | вікно відкрите і не відхилене, його ключ не вимкнено, і це помпа потоку гри |
| `Poke(at, from, bytes)` | записує без вікна; лише у файлах, перелічених у `memory=` в `tests/zsdk.cfg` |
| `Pokes()` | кількість сирих записів досі |

Кожен запис відхиляється поза помпою потоку гри, без відкритого вікна і коли ключ вікна вимкнено. Сирі записи нічого не втримують живим у грі: тримайте об'єкти через власні посилання гри, а зміни робіть зворотними в межах сесії ([RULES-UA.md](RULES-UA.md#7-обєкти-і-стан-гри)). Зразок FieldWatch вибирає поле із захистом для запущеної збірки і читає його всередині вікна збою:

<!-- doccheck: present group=camera -->
```cpp
const guard::Field &Locked()
{
    for (const guard::Field *field : kLockedFovs)
        if (zsdk::guard::Of(*field->set) == zsdk::guard::State::Ok) return *field;
    return *kLockedFovs[0];
}

float LockedFov(const void *camera)
{
    zsdk::ledger::Window window("FieldWatch.LockedFov");
    float value = -1.0f;
    if (!window.Refused() && !zsdk::mem::Read(zsdk::Addr(camera), Locked(), value)) value = -1.0f;
    window.Close();
    return value;
}
```

## `zsdk/overlay.h`

Усе для оверлею: вкладка аддона, стандартний блок «Діагностика» і віджети, що малюють шрифтом SDK. Підключайте його лише у файлах `src/overlay`; у модулі кадру він зупиняє збирання через `#error`.

**Потік:** оверлей. Кожен віджет перевіряє, що виконується всередині події overlay чи page, а інакше одразу повертається з одним рядком журналу. `tab::Fixed` і `tab::Fade` — проста математика.

**Стабільність:** Stable

`Overlay` передається в `hooks.overlay` і `hooks.page`: `runtime` (середовище виконання ефектів ReShade), `scale` (висота екрана, поділена на 1080, щонайменше 1) і `ui` (мова: `overlay::Ui()` у `page`, `lang::Current()` в `overlay`, тож HUD сам викликає `overlay::Ui()`). `overlay::Inside()` дає true всередині цих подій; `overlay::Ui()` — мова, якою малювати: `lang::Current()` або англійська, якщо шрифт не вміє малювати кирилицю. `overlay::Scope` позначає код як такий, що виконується всередині події overlay; SDK відкриває такий навколо хуків, і його не можна копіювати.

| Виклик `tab` | Що робить |
|---|---|
| `Label(text, tone, framed)` | один рядок тексту шрифтом SDK (кирилиця малюється); `framed` вирівнює його з віджетом |
| `Wrapped(text, tone)` | текст із перенесенням по межах слів |
| `Beside(text, tone)` | текст у рядку останнього віджета |
| `Heading(text)` | роздільник і акцентний підпис |
| `Box(id, text, value, tone)` | прапорець; true, якщо його змінили |
| `Whole(id, text, value, low, high)` | повзунок для цілого числа |
| `Real(id, text, value, low, high, format, step)` | повзунок для `double`, заокруглений до `step` |
| `Button(id, text, width)` | кнопка; true, якщо її натиснули |
| `Clipped(id, text, width)` | кнопка, текст якої обрізано до її ширини |
| `KeyField(o, id, chord, others, texts, width)` | кнопка захоплення клавіші: 1, якщо вибрано нове сполучення, -1, якщо скасовано через Esc, інакше 0; відхиляє клавіші, які не `keys::Bindable` або є в `others`, і позначає клавіші, які використовує гра, якщо в сполученні немає ALT |
| `Capturing(id)` | це поле клавіші чекає на клавішу |
| `Release()` | зупиняє будь-яке захоплення клавіші; SDK викликає це, коли оверлей закривається |
| `Diagnostics(o, texts)` | стандартний блок: фаза, лічильники потоку гри, збірка гри, знайдені функції, виклики, вимкнені після збою, з кнопкою «Спробувати ще / Try again», файл журналу, версія ZoneSDK і старі правила, а також «Скопіювати звіт / Copy report» |
| `Report(o, texts)` | складає звіт, копіює його в буфер обміну, зберігає `report.txt` у теці даних і повертає рядок стану |
| `Size()`, `Field()` | розмір шрифту і зручна ширина для повзунка чи поля клавіші (45 % рядка, від 120 до 360 пікселів) |
| `Fixed(value, digits)` | число як текст із фіксованою кількістю знаків після коми |
| `Fade(tone, k)` | колір з альфою, помноженою на `k` |

Кольори: `kText`, `kDim`, `kAccent`, `kGood`, `kWarn` і `kShade` (тло підказки). `KeyTexts` містить тексти поля клавіші: `label` (показується поруч із кнопкою), `press`, `bad` і `game`. `DiagTexts` містить тексти «Діагностики»: `title`, `phase`, `exe`, `known`, `other`, `unknown`, `foreign`, `verbs`, `crashed`, `none`, `retry`, `log`, `sdk`, `yes`, `no`, `markers`, `full`, `pump`, `report`, `copied`, `unsaved` і `unsupported`. Кожне поле типово дорівнює `nullptr`, і тоді береться типовий рядок SDK мовою оверлею, тож `tab::Diagnostics(o)` текстів не потребує. `toast::Draw(o, lane, words, tone)` малює підказки; SDK викликає його після `hooks.overlay`.

`src/overlay/page.cpp` стартового аддона, скорочено:

<!-- doccheck: overlay body -->
```cpp
Settings s = zsdk::ini::Config<Settings>();
const Settings before = s;
const zsdk::tab::KeyTexts texts{ .label = zsdk::lang::Text("key.locate", o.ui) };
zsdk::tab::Label(zsdk::lang::Format(zsdk::lang::Text("title", o.ui), { kVersion }), zsdk::tab::kAccent);
zsdk::tab::Box("enabled", zsdk::lang::Text("mod.enabled", o.ui), s.enabled);
zsdk::tab::KeyField(o, "locate", s.locate, {}, texts, std::min(zsdk::tab::Field(), 200.0f));
if (zsdk::tab::Button("locatebtn", zsdk::lang::Text("btn.locate", o.ui))) zsdk::thread::Ask(static_cast<int>(Order::Locate));
zsdk::tab::Diagnostics(o);
if (!(s == before)) zsdk::ini::Apply(s);
```

## `zsdk/paint.h`

Примітиви малювання і власний шрифт SDK (з кирилицею), для HUD, які малюються в `hooks.overlay`. Підключайте його лише у файлах `src/overlay` (`zsdk/overlay.h` підключає його сам).

**Потік:** оверлей. Кожен виклик перевіряє, що виконується всередині події overlay. `Serve` і `Drop` належать SDK; `ink`, `terminal`, `Theme` і `Probe` лише читають чи задають вказівник.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Box(sheet, a, b, tone)` | заповнений прямокутник |
| `Frame(sheet, a, b, tone, thick)` | контур прямокутника |
| `Rule(sheet, a, span, tone, thick)` | горизонтальна лінія довжиною `span` |
| `Grain(sheet, a, b, step, tone)` | лінії розгортки через кожні `step` пікселів |
| `Corners(sheet, a, b, tone, len, thick)`, `Bracket(...)` | кутові позначки прямокутника |
| `Text(sheet, at, size, tone, line)` | рядок тексту UTF-8 |
| `Mid(sheet, at, size, tone, line)` | текст із центром на `at.x` |
| `Caps(sheet, at, size, tone, line, gap)` | текст із розрядкою; повертає його ширину |
| `Span(size, line)`, `CapsSpan(size, line, gap)` | ширина, яку займе текст |
| `Cyrillic()` | шрифт, що діє, уміє малювати кирилицю |
| `ink()` | палітра, що діє; `terminal()` — зелена палітра термінала, а `Theme(palette)` змушує `ink()` повертати іншу (`nullptr` — типову) |
| `Serve(device)`, `Drop(device)` | належать SDK: тримають текстуру шрифту пристрою і звільняють її разом із пристроєм |
| `Probe(sink)` | викликає `sink(pixels)` з розміром кожного намальованого тексту, для тестів та інструментів |

`sheet` — це список малювання ImGui, зазвичай `ImGui::GetForegroundDrawList()`. `paint.h` оголошує лише `reshade::api::device`. Палітра `Ink` має ролі `glass`, `edge`, `hair`, `amber`, `wash`, `paper`, `text`, `warm`, `dim`, `faint`, `tile`, `dark`, `foe`, `veiled`, `life`, `soot`, `glow` і `rail`. Кутова панель зразка HelloHud, скорочено:

<!-- doccheck: overlay -->
```cpp
void Panel(zsdk::Overlay &o, const std::string &line)
{
    namespace paint = zsdk::paint;
    if (!o.runtime || !zsdk::overlay::Inside()) return;
    ImDrawList *sheet = ImGui::GetForegroundDrawList(nullptr);
    const float size = 18.0f * o.scale;
    const ImVec2 a(24.0f, 24.0f);
    const ImVec2 b(a.x + paint::Span(size, line.c_str()) + 16.0f, a.y + size + 16.0f);
    paint::Box(sheet, a, b, paint::ink().glass);
    paint::Corners(sheet, a, b, paint::ink().amber, 8.0f, 1.0f);
    paint::Text(sheet, ImVec2(a.x + 8.0f, a.y + 8.0f), size, paint::ink().text, line.c_str());
}
```

## `zsdk/paths.h`

Теки гри як чисті функції двох коренів: шляху до exe і `%LOCALAPPDATA%`.

**Потік:** будь-який. Лише `SaveGamesData` читає диск (список тек).

**Стабільність:** Stable

| Виклик | Що дає |
|---|---|
| `Current()` | `Roots` цього процесу: `exe` (шлях до exe гри) і `local` (`%LOCALAPPDATA%`) |
| `GameRoot(roots)` | теку гри над `Binaries\Win64` або порожньо, якщо exe лежить не в такій теці |
| `Mods(roots)` | `<game>\Content\Paks\~mods` |
| `Saved(roots)` | `%LOCALAPPDATA%\Stalker2\Saved` |
| `Logs(roots)` | `Saved\Logs` |
| `AppliedSettings(roots)` | `Saved\GameSettings\AppliedSettingsWin64.cfg` |
| `SteamManifest(roots)` | `steamapps\appmanifest_1643320.acf`, якщо гра лежить у `steamapps\common` |
| `SaveGamesData(roots)` | наявні теки `Saved\<folder>\SaveGames\Data`, відсортовані, щонайбільше 64 |
| `PlayerSave(file)` | чи є файл збереженням гравця (`.sav` у `SaveGames\Data`, не `CampaignsSave` і не `AnalyticsData`) |

Кожен виклик, крім `PlayerSave`, має й просту форму без `roots`, яка бере `Current()`. На збірках з інших крамниць, де exe лежить не в `Binaries\Win64` (як-от збірка Game Pass), `GameRoot`, `Mods` і `SteamManifest` порожні; ZoneSDK підтримує лише Steam-версію. У тестовій програмі із заданим `ZSDK_TEST_HOME` виклик `Current()` вказує всередину цієї теки, тож жоден тест не читає справжніх тек гри чи користувача.

<!-- doccheck: present body -->
```cpp
const zsdk::paths::Roots roots = zsdk::paths::Current();
zsdk::Log("game %s, mods %s", zsdk::log::Shown(zsdk::paths::GameRoot(roots)).c_str(), zsdk::log::Shown(zsdk::paths::Mods(roots)).c_str());
for (const std::filesystem::path &folder : zsdk::paths::SaveGamesData(roots))
    zsdk::Note(2, "save folder %s", zsdk::log::Shown(folder).c_str());
```

## `zsdk/policy.h`

Перемикачі поведінки SDK, які задаються один раз в `App::policy`. Нові аддони лишають `zsdk::Policy{}`.

**Потік:** будь-який (політика — частина незмінного `App`).

**Стабільність:** Stable

**Legacy, може зникнути у 2.0:** `Policy::marks`, `Policy::gone`, `Policy::spelling`, `Policy::widen`, `Policy::census`, `Policy::knock`, `Policy::knockMs`, `Policy::idleMs`, `Policy::wire`, `Policy::wireMs` і їхні переліки `Marks`, `GoneFlags`, `Spelling`, `CensusMode`, `Knock`, `WireRetry`.

| Поле `Policy` | Типово | Що змінює |
|---|---|---|
| `strictMarks` | `true` | виклик з міткою, мітку збою якого неможливо записати, відхиляється |
| `verifiedOnly` | `true` | непідтверджені дієслова, приречені цілі та приречені параметри-об'єкти відхиляються ще до виклику |
| `halt` | `Halt::Key` | `Key`: нативний збій вимикає лише свій ключ; `Session`: він ще й зупиняє всі виклики до кінця сесії |
| `haltOnSeh` | `true` | структурований виняток, що вийшов за межі такту потоку гри, зупиняє всі виклики до кінця сесії |
| `watchKept` | 8 | найбільша кількість одночасно відкритих вікон збою, від 1 до 32 (`ledger::WatchLimit`) |
| `files` | `FileThread::Render` | файли журналу і налаштувань пишуться з такту кадру; `Render` — єдине значення |
| `mods` | `ModRule::Alt` | `Alt`: проста клавіша спрацьовує з утримуваним Ctrl чи Shift; `Exact`: модифікатори мають збігатися точно |
| `marks` | `Marks::All` | `All`: кожен `Fire` іде з міткою; `Flagged`: лише рядки `Risky` і `Fire(call, key)` |
| `gone` | `GoneFlags::Strict` | маска прапорців, яку використовує `ue::Gone`: `Strict` (`kGoneStrict`) або `Legacy` (`kGoneLegacy`) |
| `spelling` | `Spelling::Tolerant` | `Tolerant`: параметр, написаний в іншому регістрі літер, приймається з рядком журналу; `Strict`: не приймається |
| `widen` | `false` | відсутню функцію шукають і в класі цілі |
| `census` | `CensusMode::Interval` | `Interval`: проходи за таймером; `OnDemand`: проходи, коли просить `census::Want` |
| `knock`, `knockMs`, `idleMs` | `Knock::BusyIdle`, 100, 1000 | як часто будять потік гри, поки аддон не «гарячий» і жоден наказ не чекає: `BusyIdle` — кожні `idleMs`, `Fixed` — кожні `knockMs`, `Configured` — як задає `thread::Knock`; поки аддон «гарячий» або наказ чекає, потік гри будять на кожному кадрі |
| `wire`, `wireMs` | `WireRetry::Backoff`, 2000 | повторні спроби підключити рушій: `Backoff` — кожні `wireMs` для перших 5 спроб, потім кожні 30 с; `Fixed` — кожні `wireMs`; щонайбільше 10 спроб |

Значення `Policy` порівнюються через `==`. `Legacy(policy)` дає true, якщо будь-яке поле, крім `watchKept`, відрізняється від типового; `LegacyText(policy)` перелічує такі поля (`marks=flagged, knock=fixed 100/1000`) або пише `none`. Цей список показують журнал запуску і «Діагностика».

<!-- doccheck: present body -->
```cpp
const zsdk::App &app = zsdk::app::Get();
if (zsdk::Legacy(app.policy)) zsdk::Log("legacy rules: %s", zsdk::LegacyText(app.policy).c_str());
constexpr zsdk::Policy kStrict{ .halt = zsdk::Halt::Session, .watchKept = 16 };
static_assert(kStrict.marks == zsdk::Marks::All && kStrict.mods == zsdk::ModRule::Alt && kStrict.files == zsdk::FileThread::Render);
zsdk::Note(2, "this add-on runs the default policy: %s", app.policy == zsdk::Policy{} ? "yes" : "no");
```

## `zsdk/present.h`

Заголовок, який кожен модуль кадру підключає першим. Він підключає `zsdk/zsdk.h` і зупиняє збирання (`#error`), якщо в модулі видно ImGui або модуль зібрано без `IMGUI_DISABLE`: коду ImGui місце в `src/overlay`.

**Потік:** жоден (його лише підключають).

**Стабільність:** Stable

<!-- doccheck: present file -->
```cpp
#include <zsdk/present.h>

#include "template.h"

namespace ztemplate::core
{
    const char *Hello()
    {
        return kVersion;
    }
}
```

## `zsdk/registry.h`

Таблиця SDK з випущеними аддонами, їхніми тегами, смугами підказок і типовими клавішами. Аддон приносить власний рядок в `App::row`; таблиця потрібна для перехресної перевірки на конфлікти.

**Потік:** будь-який.

**Стабільність:** Provisional

`Row` має `id`, `tag`, `lane` (смуга підказок як частка висоти екрана, 0 — без підказок) і `keys` (типові сполучення через `|`).

| Виклик | Що робить |
|---|---|
| `Rows()` | усі рядки таблиці |
| `Find(id)` | рядок за id або `nullptr` |
| `Lane(id, fallback)` | смуга для id, якщо вона більша за 0, інакше `fallback` |
| `kSpareLane` | 0.75, смуга аддона без рядка |

Загальний набір `identity` дає помилку, якщо інший рядок таблиці використовує тег аддона, і попереджає, якщо id аддона немає в таблиці або його смуга спільна з іншим аддоном.

<!-- doccheck: present body -->
```cpp
const zsdk::registry::Row *row = zsdk::app::Row();
const zsdk::registry::Row *listed = row ? zsdk::registry::Find(row->id) : nullptr;
if (row && !listed) zsdk::Note(1, "%s is not in the ZoneSDK table, its own row is used", row->id);
const float lane = zsdk::registry::Lane(row ? row->id : "", zsdk::registry::kSpareLane);
zsdk::Note(2, "toast lane %.2f, %zu rows in the table", static_cast<double>(lane), zsdk::registry::Rows().size());
```

## `zsdk/report.h`

Текст, що стоїть за «Скопіювати звіт / Copy report»: версії, збірка гри, версія ReShade, аддони поруч з exe гри, реєстр збоїв, власні рядки аддона і кінець журналу, з прихованими шляхами користувача.

**Потік:** оверлей («Діагностика» складає звіт там) або будь-який потік, крім D3D і завантажувача: `Text` скидає журнал і читає файли.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Text(reshade)` | звіт; `reshade` — модуль ReShade, з якого береться версія (`nullptr` дає `unknown`) |
| `Version(module)` | версія файлу завантаженого модуля, `a.b.c.d` або `unknown` |
| `File()` | `report.txt` у теці даних або порожньо |
| `Save(text)` | записує звіт у `File()` |
| `kLogLines` | 80, кількість рядків журналу у звіті |
| `kTailBytes` | 64 КіБ, найбільший шматок журналу, який для них читається |

`hooks.report` додає власні рядки аддона під заголовком з його назвою. Хук звіту зразка HelloHud, переписаний у термінах стартового аддона:

<!-- doccheck: present -->
```cpp
std::string Section()
{
    const Status s = core::Snapshot();
    return std::string("template: located ") + (s.located.empty() ? "-" : s.located) + ", refusal " + (s.refusal.empty() ? "-" : s.refusal);
}
```

## `zsdk/shell.h`

Точка входу аддона: експорти, які читає ReShade, і передача керування з `DllMain`. Підключайте його лише в `src/entry.cpp`.

**Потік:** `Main`: завантажувач (`DllMain`). `Module`, `Stopping`, `Ready` і `Exiting`: будь-який.

**Стабільність:** Stable

| Назва | Що робить |
|---|---|
| `ZSDK_ADDON(name, description)` | експортує `NAME` і `DESCRIPTION` для ReShade; один раз на аддон |
| `Main(module, reason, reserved, hooks)` | усе, що робить `DllMain`: під час приєднання перевіряє exe гри та унікальність тегу, реєструється в ReShade і прив'язує хуки; під час від'єднання зупиняє SDK і викликає `hooks.stop` всередині огорожі |
| `Module()` | дескриптор модуля аддона |
| `Ready()` | true, щойно `hooks.boot` повернувся на першому кадрі; лишається false після збою під час запуску |
| `Stopping()` | true, щойно аддон почав вивантажуватися |
| `Exiting()` | true, коли процес завершується (від'єднання з ненульовим `reserved`); усіх інших потоків уже немає |

Загальний набір `hardrules` вимагає рівно одного `ZSDK_ADDON` і одного `DllMain`, який лише повертає `zsdk::shell::Main(...)`. `src/entry.cpp` стартового аддона, після таблиці хуків:

<!-- doccheck: entry -->
```cpp
ZSDK_ADDON("ZoneTemplate", TEMPLATE_DESCRIPTION)

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    static const zsdk::Hooks hooks = Wire();
    return zsdk::shell::Main(module, reason, reserved, hooks);
}
```

## `zsdk/text.h`

Помічники для тексту UTF-8, які знають кирилицю.

**Потік:** будь-який. `Read` і `Write` працюють із диском.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Trim(raw)` | текст без пробілів, табуляцій і кінців рядків з обох боків |
| `Split(line, mark)` | частини між символами `mark` |
| `Tokens(line)` | слова, розділені пробілами, табуляціями, комами чи кінцями рядків; подвійні лапки зберігають роздільники всередині слова |
| `Lines(body)` | рядки без кінців рядків, BOM пропущено |
| `Wide(utf8)`, `Narrow(wide)` | UTF-8 в UTF-16 і назад |
| `Fold(point)`, `Raise(point)` | одна кодова точка в нижній чи верхній регістр, для латиниці й кирилиці |
| `Lower(utf8)`, `Upper(utf8)` | текст у нижній чи верхній регістр |
| `Latin(utf8)` | приблизний латинський ключ пошуку для латинського чи кириличного тексту, тож `Zaton` і `Затон` дають той самий ключ |
| `Cyrillic(utf8)` | у тексті є кирилична літера |
| `Points(utf8)` | кодові точки |
| `Starts(text, head)`, `Ends(text, tail)` | перевірки початку і кінця |
| `Same(left, right)` | рівність без урахування регістру літер ASCII |
| `Color(token, argb)` | `#RRGGBB` у непрозоре значення ARGB |
| `Format(shape, ...)` | `printf` у `std::string` |
| `Read(path)`, `Write(path, body)` | увесь файл як байти |

<!-- doccheck: present body -->
```cpp
const std::vector<std::string> parts = zsdk::text::Split("ALT+K|F7", '|');
const std::string lower = zsdk::text::Lower("Зона ВІДЧУЖЕННЯ");
uint32_t argb = 0;
const bool read = zsdk::text::Color("#D9A227", argb);
zsdk::Note(2, "%zu keys, %s, %zu wide chars, color %08X %s", parts.size(), lower.c_str(), zsdk::text::Wide(lower).size(), argb, read ? "read" : "bad");
```

## `zsdk/thread.h`

Помпа потоку гри і фаза аддона. SDK під'єднується до черги повідомлень потоку гри і, коли будить його, виконує там свою помпу; кожен виклик гри відбувається всередині цієї помпи.

**Потік:** будь-який.

**Стабільність:** Stable

**Internal:** `thread::Booked`, `thread::Rejected` (стани робочого потоку; їх показують «Діагностика» і звіт).

`Phase` — це те, що показує вкладка; її дає `thread::State(stable)`, де `stable` береться з тримача світу:

| Значення `Phase` | Що означає |
|---|---|
| `Off` | SDK ще не запустився |
| `Hooking` | SDK чекає на вікно гри, щоб під'єднатися до його потоку |
| `Binding` | SDK шукає рушій в exe |
| `Wiring` | SDK готує перший виклик гри |
| `Waiting` | SDK чекає на світ гри; `Grip::wait` каже, на що саме |
| `Ready` | виклики гри працюють |
| `Failed` | ця збірка гри чи exe не підтримується, або спроби підключення вичерпано |
| `Halted` | після збою виклики гри зупинено до кінця сесії |
| `Lost` | потік гри не відповідає понад 2 с, хоча кадри малюються |
| `Count` | кількість фаз |

| Виклик | Що робить |
|---|---|
| `Ask(order)` | ставить у чергу наказ для `hooks.order` у потоці гри і будить його; чекати можуть щонайбільше 8 наказів, найстаріший відкидається |
| `Wake()` | будить потік гри для помпи |
| `Knock(ms)` | період пробудження з Legacy `Knock::Configured` |
| `OnGame()` | це потік гри |
| `Started()`, `Hooked()`, `Bound()`, `Wired()` | SDK запустився, під'єднався до потоку гри, знайшов рушій, підключив виклики гри |
| `Halted()` | виклики гри зупинено до кінця сесії |
| `Stopping()` | SDK зупиняється або гра попросила вийти |
| `Overlaid()` | оверлей ReShade відкрито |
| `LastMs()`, `WorstMs()` | скільки тривала остання помпа і найгірша за останні 10 с |
| `LastPumpAt()`, `PumpAlive()` | коли виконалася остання помпа і чи виконувалася помпа за останні 2 с |
| `Count()` | `Counters` помпи |
| `State(stable)` | `Phase` |

| Поле `Counters` | Що рахує |
|---|---|
| `posted`, `seen`, `ticked` | надіслані повідомлення пробудження, побачені хуком повідомлення, виконані помпи |
| `pumpAt` | коли виконалася остання помпа |
| `turns` | ітерації циклу робочого потоку SDK |
| `wires`, `wireMs` | спроби підключення і тривалість останньої |

Вкладка і код клавіш стартового аддона користуються ним так:

<!-- doccheck: present body -->
```cpp
const zsdk::world::Grip w = zsdk::world::Snapshot();
const zsdk::Phase phase = zsdk::thread::State(w.stable);
if (phase == zsdk::Phase::Ready && !zsdk::thread::Overlaid()) zsdk::thread::Ask(static_cast<int>(Order::Locate));
const zsdk::thread::Counters c = zsdk::thread::Count();
zsdk::Note(2, "pump: posted %llu, ticked %llu, last %.2f ms, alive %d", static_cast<unsigned long long>(c.posted), static_cast<unsigned long long>(c.ticked), zsdk::thread::LastMs(), zsdk::thread::PumpAlive() ? 1 : 0);
```

## `zsdk/toast.h`

Короткі повідомлення на екрані. Аддон ставить їх у чергу з будь-якого потоку; SDK малює їх у смузі підказок аддона (`app::Lane()`) за допомогою його `hooks.words` і `hooks.tone`.

**Потік:** будь-який (одне блокування). Малювання відбувається в події overlay.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Push(kind, detail, args, replaceKind)` | ставить підказку в чергу; `replaceKind` спершу відкидає найновіші підказки того самого виду, щоб повторне повідомлення не накопичувалося |
| `Live(now)` | підказки, які треба показати зараз, щонайбільше `kShown` |
| `Reset()` | відкидає всі підказки; SDK робить це, коли зникає середовище виконання ReShade |
| `Focused()` | вікно гри у фокусі; лише тоді підказки малюються |
| `kLifeMs`, `kFadeMs` | 3000 мс на екрані, з них останні 400 мс — згасання |
| `kShown`, `kKept` | 3 показано одночасно, 8 у черзі |
| `kUnsupported` | -1, власна підказка SDK про те, що ця збірка гри не підтримується |

`Item` має `kind` аддона (від 0 і більше; види, менші за 0, належать SDK), вільний текст `detail`, `args` для `hooks.words` і `born` (коли підказку поставлено в чергу). `Toast` стартового аддона і слова для нього:

<!-- doccheck: present -->
```cpp
void Toast(Say say, const std::string &arg)
{
    zsdk::toast::Push(static_cast<int>(say), {}, { arg }, false);
}
```

<!-- doccheck: overlay -->
```cpp
std::string Words(const zsdk::toast::Item &item, zsdk::lang::Lang ui)
{
    const char *id = item.kind == static_cast<int>(Say::Located) ? "say.located" : "say.refused";
    return zsdk::lang::Format(zsdk::lang::Text(id, ui), { item.args.empty() ? std::string() : item.args.front() });
}
```

## `zsdk/ue.h`

Доступ лише для читання до об'єктів і даних рефлексії гри: масив об'єктів, назви, класи, властивості і функції. Кожне читання йде через безпечне копіювання, яке ніколи не падає.

**Потік:** для окремих читань — будь-який, але об'єкт може загинути будь-коли в іншому потоці, тож рішення ухвалюйте в потоці гри. `FindClass`, `FindLive` і `Host` обходять увесь масив об'єктів: потік гри або робочий потік, ніколи не на кожному кадрі. `Function`, `Functions`, `Analyze` і `Choices`: будь-який.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Here(thing, type)` | об'єкт живе в масиві об'єктів під власним індексом (і має клас `type`, якщо його задано); перевіряйте так кожен вказівник, який тримаєте |
| `Doomed(thing)` | об'єкт не `Here` або знищується (прапорці об'єкта `kObjectDoomed` чи прапорці елемента `kItemGarbage` або `kItemUnreachable`); гра знищує об'єкти протягом кількох кадрів, тож об'єкт може бути водночас `Here` і приреченим |
| `Gone(thing)` | об'єкт має прапорці зникнення з політики (`kGoneStrict` або `kGoneLegacy`) |
| `Standing(thing, type)` | об'єкт є в масиві саме з цим класом |
| `Seal(thing)`, `Ours(mark)` | ідентифікаційна `Mark` об'єкта і перевірка, чи вказівник досі той самий об'єкт і не приречений |
| `Look(thing)` | текст для журналу з прапорцями і вердиктом об'єкта |
| `ClassOf(thing)`, `OuterOf(thing)` | клас і зовнішній об'єкт (outer) об'єкта |
| `Label(thing)`, `Path(thing)` | назва об'єкта з числовим суфіксом і ланцюжок його зовнішніх об'єктів |
| `Kin(type, base)`, `Inherits(type, name)` | клас успадковує інший клас або клас із такою назвою |
| `Flags(thing)` | прапорці об'єкта |
| `PropertyOffset(type, field)` | зсув властивості з рефлексії або -1 |
| `Bulk(type)` | розмір структури чи класу |
| `FindClass(name)` | клас за назвою |
| `FindLive(baseClass)` | найновіший об'єкт (не типовий об'єкт класу), чий клас успадковує `baseClass` |
| `Host(type)` | об'єкт, на якому виконувати виклики `type`; типові об'єкти класу — лише для бібліотек функцій |
| `Function(owner, name)`, `Functions(name)` | `UFunction` із довідника функцій або всі функції з такою назвою |
| `Analyze(function, out)` | параметри функції як `Sign` |
| `Choices(param, out)`, `Choose(picks, word, out)` | елементи параметра-переліку як `Pick` і значення одного елемента за назвою |
| `CopyText(at)` | `FString` у пам'яті гри як `std::wstring`, без звільнення |
| `Lend(text)` | подання `Lent` рядка для параметра `FString`; рядок має жити довше за `Fire` |
| `NameText(index, number)`, `Word(slot)` | `FName` як текст |
| `Copy(at, into, bytes)`, `Read<T>(at, out)`, `Held(spot, bytes)` | безпечні читання; чи можна прочитати діапазон пам'яті |
| `Grab(thing, at)` | вказівник, збережений за зсувом |
| `Total()`, `Stride()`, `Item(index)`, `At(index)`, `Serial(index)` | масив об'єктів: кількість, розмір елемента, елемент, об'єкт і серійний номер за індексом |
| `Ready()`, `Bound()` | виклики гри підключено; рушій знайдено |

`Lend` має дві форми: для `std::wstring` і для `const wchar_t *`; тимчасовий `std::wstring` не скомпілюється (`Lend(const std::wstring &&) = delete`), а текст із `kLendCap` (1 Mi) символів або довший дає порожнє подання. `Lent` — це `data`, `num` (довжина разом із кінцевим нулем) і `max`.

| Тип | Поля |
|---|---|
| `Param` | `name`, `type` (клас властивості, наприклад `IntProperty`), `field`, `at`, `size`, `flags` |
| `Sign` | `owner`, `bytes` (розмір блоку параметрів), `params` |
| `Pick` | `name`, `value` |
| `Mark` | `actor`, `type`, `index`, `name`, `serial` |

Константи розкладки: `kIndexAt`, `kClassAt`, `kNameAt`, `kOuterAt` і `kSuperAt` (поля об'єкта і класу), `kItemFlagsAt` і `kItemClusterAt` (поля елемента масиву об'єктів), а також маски прапорців `kGoneStrict`, `kGoneLegacy`, `kObjectDoomed`, `kItemGarbage` і `kItemUnreachable`. Тримайте об'єкти через власні посилання гри і перед кожним використанням перевіряйте їх через `Ours` або `Doomed`; ніколи не ставте бітів кореня ([RULES-UA.md](RULES-UA.md#7-обєкти-і-стан-гри)).

<!-- doccheck: present body -->
```cpp
const zsdk::world::Grip &w = zsdk::world::Hold();
if (!zsdk::ue::Here(w.body, w.bodyType) || zsdk::ue::Doomed(w.body)) return;
static zsdk::ue::Mark kept{};
if (!zsdk::ue::Ours(kept)) kept = zsdk::ue::Seal(w.body);
zsdk::Note(2, "pawn %s of class %s: %s", zsdk::ue::Path(w.body).c_str(), zsdk::ue::Label(zsdk::ue::ClassOf(w.body)).c_str(), zsdk::ue::Look(w.body).c_str());
```

## `zsdk/verbs.h`

Таблиці дієслів: функції гри, які викликає аддон, з параметрами, яких він очікує. SDK звіряє кожен рядок із даними рефлексії гри один раз, під час підготовки (prime), і непідтверджений рядок ніколи не викликається.

**Потік:** `Prime`: потік гри (його запускає SDK). `Last`, `Ready`, `Note` і `Primed`: будь-який. Решта викликів — чисті функції.

**Стабільність:** Stable

| Поле `Spec` | Що містить |
|---|---|
| `owner`, `name` | клас-власник і назва функції |
| `required` | функція потрібна ядру аддона; `Report::coreReady` дорівнює false, поки обов'язковий рядок не проходить перевірку |
| `params` | до 12 `Want` |
| `mark` | `Mark::Auto` або `Mark::Risky`, щоб завжди виконувати виклик під міткою збою |
| `count` | точна кількість параметрів або -1 |
| `bytes` | точний розмір блоку параметрів або -1 |

`Want` — це один очікуваний параметр: `name`, `types` (назви класів властивостей через `|` або `nullptr` — будь-який), `at` (зсув, -1 — будь-який) і `size` (0 або менше — будь-який). `Check` — вердикт рядка: `Unknown`, `Ok` або `Bad`.

| Виклик | Що робить |
|---|---|
| `Prime(specs)` | перевіряє таблицю і повертає `Report` |
| `Last()` | останній `Report`: `requiredOk`, `requiredAll`, `optionalOk`, `optionalAll`, `missing` (рядки, що не пройшли) і `coreReady` |
| `Ready(verb)` | рядок пройшов перевірку |
| `Note(verb)` | що перевірка знайшла для рядка |
| `Primed()` | таблицю перевірено |
| `Key(owner, name)`, `KeyOf(spec)` | ключ реєстру збоїв `Owner.Name` |
| `Verify(sign, missing, found, spec, tolerant, note, spelled)` | перевірка одного рядка, для тестів |
| `TypeFits(type, types)` | клас властивості є у списку через `|` |
| `Same(a, b)` | рівність рядків, `constexpr` |
| `Counted(specs, required)` | підрахунок обов'язкових чи необов'язкових рядків, `constexpr` |

Хук prime стартового аддона:

<!-- doccheck: present body -->
```cpp
const zsdk::verbs::Report r = zsdk::verbs::Last();
if (!r.coreReady) zsdk::Log("template: a required game function is missing: %s", r.missing.c_str());
if (zsdk::verbs::Primed() && !zsdk::verbs::Ready(kLocation))
    zsdk::Log("template: %s: %s", zsdk::verbs::KeyOf(kSpecs[kLocation]).c_str(), zsdk::verbs::Note(kLocation).c_str());
```

## `zsdk/verbs/common.h`

Спільний каталог дієслів і помічники для таблиць: дієслова світу, потрібні кожному аддону, перевірки екрана, дві консольні команди, а також `Join` і `Index`, щоб будувати таблицю під час компіляції.

**Потік:** жоден (дані часу компіляції і функції `constexpr`).

**Стабільність:** Stable

**Provisional:** `common::kScreen` і його значення `Screen` (перевірки екрана ще можуть змінитися в мінорному випуску).

| Назва | Що це |
|---|---|
| `Join(a, b)` | таблиця з двох таблиць (двох масивів C або `std::array` і масиву C); `constexpr` |
| `Index(specs, owner, name)`, `Index(specs, name)` | індекс рядка або -1; `constexpr` |
| `Counted(specs, required)` | форма `verbs::Counted` для `std::array` |
| `common::kWorld` | шість дієслів світу з індексами-значеннями `World`: `kPlayerController`, `kPlayerPawn`, `kCameraManager`, `kTimeSeconds`, `kGamePaused` (необов'язкове), `kFrameCount`, і `kWorldCount`, що дорівнює 6 |
| `common::Plays(specs, verb, role)` | рядок `verb` у `specs` — це дієслово каталогу для ролі `World` |
| `common::kStatics` | `GameplayStatics` і `KismetSystemLibrary`, для `CensusSpec::statics` |
| `common::kScreen` | необов'язкові перевірки екрана з індексами-значеннями `Screen`: `kInCinematic`, `kInStaticDialog`, `kUsingPda`, `kUsingBackpack`, `kInInteraction`, і `kScreenCount` |
| `common::kConsole` | дві консольні команди `kConsoleHost` (`CustomConsoleManagerAI2`) з індексами-значеннями `Console`: `kDestroyModel`, `kNeedsValue`, і `kConsoleCount`; необов'язкові і `Risky`; впишіть `kConsoleHost` у `CensusSpec::hosts` |
| `kObject`, `kInt`, `kFloat`, `kReal`, `kFrame`, `kBool`, `kStr`, `kEnum`, `kStruct` | списки типів властивостей для `Want::types` |

Тримайте `common::kWorld` на початку таблиці, щоб підходили типові значення `WorldVerbs`, а власні рядки аддона — після нього. `src/template.h` стартового аддона:

<!-- doccheck: present -->
```cpp
namespace common = zsdk::verbs::common;
inline constexpr zsdk::verbs::Spec kOwn[] = {
    { .owner = "Actor", .name = "K2_GetActorLocation", .required = true, .params = { { .name = "ReturnValue", .types = common::kStruct, .at = 0, .size = 24 } }, .mark = zsdk::verbs::Mark::Risky },
};
inline constexpr auto kSpecs = zsdk::verbs::Join(common::kWorld, kOwn);
enum Verb : int { kLocation = common::kWorldCount, kVerbs };
static_assert(kSpecs.size() == static_cast<size_t>(kVerbs) && zsdk::verbs::Counted(kSpecs, true) == 6);
static_assert(zsdk::verbs::Index(kSpecs, "Actor", "K2_GetActorLocation") == kLocation && zsdk::verbs::Index(kSpecs, "GetFrameCount") == common::kFrameCount);
```

## `zsdk/version.h`

Версія ZoneSDK і закріплені версії сторонніх бібліотек.

**Потік:** жоден (макроси).

**Стабільність:** Stable

| Макрос | Що це |
|---|---|
| `ZSDK_VERSION_MAJOR`, `ZSDK_VERSION_MINOR`, `ZSDK_VERSION_PATCH` | номери версії |
| `ZSDK_VERSION` | версія як текст, наприклад `"1.0.0"` |
| `ZSDK_VERSION_NUMBER` | версія як `1,0,0,0`, для ресурсу версії |
| `ZSDK_TEXT(value)` | значення макросу як текст |
| `ZSDK_RESHADE_API` | 20, ReShade API доданих заголовків (ReShade 6.8) |
| `ZSDK_IMGUI_NUM` | номер версії доданого Dear ImGui |

Рядок журналу під час запуску називає версію ZoneSDK і перші 8 цифр маніфесту випуску.

<!-- doccheck: present body -->
```cpp
static_assert(ZSDK_RESHADE_API == 20, "this add-on is built for ReShade API 20");
const char *api = ZSDK_TEXT(ZSDK_RESHADE_API);
zsdk::Log("built on ZoneSDK %s (%d.%d.%d), ReShade API %s, ImGui %d", ZSDK_VERSION, ZSDK_VERSION_MAJOR, ZSDK_VERSION_MINOR, ZSDK_VERSION_PATCH, api, ZSDK_IMGUI_NUM);
```

## `zsdk/world.h`

Світ гравця таким, яким його бачить SDK: екземпляр гри, контролер гравця, його персонаж (pawn) і камера, світ, його годинник і чи світ достатньо стабільний, щоб у ньому викликати функції гри.

**Потік:** `Hold`, `WorldOf`, `OwningWorld`, `Look`, `PovAt`, `OpenWorld`: потік гри. `Player`, `Clock` і `Paused(context)`: потік гри, бо вони викликають гру. `Snapshot`, `Paused()` і `Epoch`: будь-який.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Hold()` | живий `Grip` у потоці гри; в іншому потоці один раз пише в журнал і дає власну копію `Snapshot()` цього потоку, чинну до наступного `Hold()` у ньому |
| `Snapshot()` | копія тримача з останньої публікації, для інших потоків |
| `Paused()` | гра на паузі |
| `Epoch()` | епоха світу з останньої публікації |
| `WorldOf(thing)` | світ, до якого належить об'єкт; на карті відкритого світу це світ підвантаженої комірки |
| `OwningWorld(thing)` | постійний світ, через рівень об'єкта; `nullptr`, поки якийсь об'єкт на цьому шляху приречений або завантажується |
| `Look(lens, type, at, loc, rot)` | точка огляду камери за зсувом `at` у менеджері камери, перевірена на правдоподібні значення |
| `PovAt(lens, type)` | цей зсув для класу менеджера камери або -1 |
| `Player(verb, context)` | виконує гетер гравця з дієслів світу (контролер, персонаж чи камера) |
| `Clock(context)` | час світу в секундах |
| `OpenWorld()` | світ тримача — карта відкритого світу (`WorldVerbs::map`) |
| `Paused(context)` | питає гру, чи вона на паузі |

SDK стежить за світом кожні 500 мс у потоці гри. Світ `stable`, коли контролер, персонаж і світ є на карті відкритого світу 3 с поспіль і годинник іде; новий світ або годинник, що пішов назад (завантажене збереження), — це зміна світу: епоха зростає, виконується `hooks.turn`, і тримач знову нестабільний.

| Поле `Grip` | Що містить |
|---|---|
| `gi`, `giType` | екземпляр гри і його клас |
| `pilot` | контролер гравця |
| `body`, `bodyType` | персонаж гравця (pawn) і його клас |
| `lens`, `lensType` | менеджер камери гравця і його клас |
| `world`, `worldType`, `worldName` | світ персонажа, його клас і назва |
| `home` | світ, до якого належить епоха |
| `povAt` | зсув точки огляду камери або -1 |
| `seconds`, `frame` | час світу і лічильник кадрів рушія, -1, якщо невідомо |
| `frameless` | лічильника кадрів немає, тож кроки йдуть не по одному на кадр |
| `growing` | годинник світу зрушив з минулої перевірки |
| `fit` | контролер, персонаж і світ є на карті відкритого світу |
| `stable` | придатний 3 с поспіль, і годинник іде |
| `paused` | гра на паузі |
| `first` | жодного світу ще не бачили |
| `since` | коли востаннє змінився світ або персонаж |
| `polled`, `probed` | коли востаннє перевіряли світ і екземпляри гри |
| `epoch` | епоха світу, +1 з кожною зміною світу |
| `wait` | id мовного рядка того, на що чекає тримач (`wait.world`, `wait.settle`, `wait.consoles`), або порожньо |
| `at` | коли зроблено копію для `Snapshot()` |

`Turn` повідомляє `hooks.turn` про зміну світу: `why` (текст для журналу), `world`, `sameWorld` (збереження завантажено в ту саму карту) і `epoch`. Публікація стартового аддона і хуки hot та turn зразка HelloHud:

<!-- doccheck: present -->
```cpp
bool Hot()
{
    return zsdk::world::Hold().stable;
}

void Turn(const zsdk::world::Turn &turn)
{
    zsdk::Note(1, "template: world turn %u (%s)%s", turn.epoch, turn.why, turn.sameWorld ? ", same world" : "");
}

void Publish(uint64_t)
{
    const zsdk::world::Grip &w = zsdk::world::Hold();
    zsdk::Note(2, "template: world %s, stable %d, waits for %s", w.worldName.c_str(), w.stable ? 1 : 0, w.wait && *w.wait ? w.wait : "-");
}
```

## `zsdk/zsdk.h`

Збірний заголовок: усе, чим може користуватися модуль кадру. Він підключає `app.h`, `base.h`, `call.h`, `census.h`, `exe.h`, `glyphs.h`, `guard.h`, `hooks.h`, `ini.h`, `input.h`, `job.h`, `keys.h`, `lang.h`, `ledger.h`, `log.h`, `math.h`, `mem.h`, `paths.h`, `policy.h`, `registry.h`, `report.h`, `text.h`, `thread.h`, `toast.h`, `ue.h`, `verbs.h`, `version.h` і `world.h` (а `verbs/common.h` — через `app.h`). Він не підключає `overlay.h`, `paint.h`, `shell.h` і `present.h`.

**Потік:** жоден (його лише підключають).

**Стабільність:** Stable

Його підключає власний заголовок аддона, як це робить `src/template.h` стартового аддона після `#pragma once`:

<!-- doccheck: present file -->
```cpp
#include "version.h"

#include <zsdk/zsdk.h>

namespace ztemplate
{
    std::string Versions()
    {
        return std::string("ZoneSDK ") + ZSDK_VERSION + ", add-on " + TEMPLATE_VERSION;
    }
}
```

## `zsdk/test/addon.h`

Дерево вихідних файлів аддона таким, яким його бачать загальні набори: які файли є модулями кадру, модулями оверлею, точкою входу і тестами, а також ключі `tests/zsdk.cfg`.

**Потік:** лише тестові програми (`tests/*.cpp`), ніколи не вихідні файли аддона.

**Стабільність:** Stable

| Назва | Що робить |
|---|---|
| `Load()` | читає дерево аддона, який тестують (`ZSDK_ADDON_SOURCE`), у `Tree` |
| `Tree` | `root`, `sdk` (випуск, `ZSDK_HOME`), `cfg` (ключі `tests/zsdk.cfg`), `present`, `overlay`, `entry` і `tests` |
| `Allowed(tree, key)` | список через кому з `zsdk.cfg` як множина, наприклад `present.allow` |
| `Rel(tree, file)` | шлях до файлу відносно аддона, зі скісними рисками (/) |
| `Under(file, dir)` | файл лежить усередині теки |
| `GuardSets(body)`, `GuardSets(tree)` | оголошення `guard::Set` у тексті або в усьому дереві, кожне — `Declared` з `id`, `name` і `file`; назва — це перше значення у фігурних дужках, якщо воно рядковий літерал, або позначене `.name = "..."` |

Ключі `zsdk.cfg` (`name`, `define`, `registry`, `banned`, `facade`, `memory`, `present.allow`, `identity.same`, `workspace`, `comments`) описано в [RULES-UA.md](RULES-UA.md#zsdkcfg). Локальний набір у стилі загальних:

<!-- doccheck: test -->
```cpp
using check::That;
const addon::Tree t = addon::Load();
That(!t.present.empty() && !t.entry.empty(), "the add-on has present units and src/entry.cpp");
for (const auto &file : t.present)
    That(scan::Lex(scan::Body(file)).closed, "every literal is closed in " + addon::Rel(t, file));
const std::set<std::string> allowed = addon::Allowed(t, "present.allow");
That(allowed.size() < 100, "the present.allow list is short, " + std::to_string(allowed.size()));
```

## `zsdk/test/check.h`

Перевірки кожної тестової програми.

**Потік:** лише тестові програми.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `That(ok, what)` | одна перевірка; виводить `FAIL <what>`, якщо вона не проходить |
| `Warn(what)` | виводить `warning: <what>` і рахує це попередження; підсумок збирання рахує попередження |
| `Skip(what)` | попередження зі словом `SKIPPED` |
| `Finish(name)` | виводить `<name>: N checks, M failures` (і попередження) та дає код виходу; для загального набору `zsdk-*`, що не пройшов, додає рядок, який вказує на його частину розділу 10 RULES.md; `return check::Finish("local");` завершує `main` |
| `Count()`, `Failures()`, `Warnings()` | лічильники |

Збирання запускає кожен `tests/*.cpp` як окрему програму з визначеними `ZSDK_TESTS` і `<DEFINE>_TESTS`, в ізольованій теці (`ZSDK_TEST_HOME`); `ZSDK_ADDON_NAME`, `ZSDK_ADDON_SOURCE`, `ZSDK_HOME` і `ZSDK_SYMBOLS` кажуть програмі, що вона тестує. `tests/local.cpp` стартового аддона:

<!-- doccheck: test -->
```cpp
using check::That;
ztemplate::Settings s;
s.language = "en";
s.locate = zsdk::keys::Parse("ALT+K");
const std::string text = zsdk::ini::Text(s);
ztemplate::Settings back;
zsdk::ini::Parse(text, back);
That(back == s, "the template settings survive a round trip");
That(text.find("[keys]\r\nlocate=ALT+K\r\n") != std::string::npos, "the locate key is written by name");
```

## `zsdk/test/scan.h`

Невеликий сканер вихідних файлів C++ і файлові помічники для тестових програм: він прибирає коментарі й рядкові літерали, знаходить тіла функцій, виклики і шаблони лексем.

**Потік:** лише тестові програми.

**Стабільність:** Stable

| Виклик | Що робить |
|---|---|
| `Lex(body)` | `Lexed`: `code` (коментарі прибрано, літерали спорожнено), `literals`, `words`, `comments` (скільки їх), `firstComment` (рядок першого) і `closed` (кожен літерал закрито) |
| `Functions(code)` | тіла функцій у розібраному коді, кожне — `Function` з `name`, `at` і `body` |
| `Calls(code)` | назви, що викликаються в розібраному коді |
| `Tokens(code)` | код, розбитий на лексеми (`::` — одна лексема) |
| `Rule(rule)`, `Seek(tokens, rule)`, `Uses(tokens, rule)` | шаблон лексем на кшталт `"zsdk :: Fire ("`, де `word*` відповідає початку лексеми (кожна частина — `Piece` з `text` і `prefix`): розібрати шаблон, знайти його позицію, перевірити, чи він є |
| `Ticks(code)` | сирі різниці часових позначок на кшталт `now - at`, кожна — `Tick` з `line` і `text` |
| `Body(file)`, `Files(dir, kinds, deep)`, `Dirs(dir)` | прочитати файл; перелічити файли з певними розширеннями; перелічити теки |
| `Text(path)`, `Generic(path)` | шлях як UTF-8, для `Generic` — зі скісними рисками (/) |
| `Env(name, fallback)`, `EnvPath(name, fallback)`, `Wide(name)` | змінна середовища як текст, як шлях (безпечно для будь-яких літер), як UTF-16 |
| `AddonRoot()`, `SdkRoot()` | `ZSDK_ADDON_SOURCE` і `ZSDK_HOME` |
| `Config(file)`, `List(csv)`, `Trim(raw)` | прочитати рядки `key=value`; розбити список через кому; обрізати пробіли |
| `Ident(c)`, `LineOf(body, at)`, `Close(code, open)`, `Before(code, at)`, `Count(text, what)` | дрібні помічники: символ ідентифікатора, рядок для зсуву, відповідна `}`, слово перед зсувом, скільки разів трапляється текст |

<!-- doccheck: test -->
```cpp
using check::That;
int found = 0;
for (const auto &file : scan::Files(scan::AddonRoot() / "src", { ".cpp" }, true))
    for (const scan::Function &f : scan::Functions(scan::Lex(scan::Body(file)).code)) found += f.name == "Locate" ? 1 : 0;
That(found >= 1, "the add-on defines Locate");
const std::vector<std::string> tokens = scan::Tokens(scan::Lex("zsdk::Fire(c, key);").code);
That(scan::Uses(tokens, "zsdk :: Fire ("), "the token rule finds the call");
```

## `zsdk/test/hardrules.h`

Таблиця правил загального набору `hardrules` (виклики, яких не можна використовувати в коді аддона, і підказки, які він виводить).

**Потік:** лише тестові програми.

**Стабільність:** Internal

Аддони його не підключають. Що саме відхиляє цей набір, перелічує [RULES-UA.md](RULES-UA.md#zsdk-hardrules).

## `zsdk/test/ledger_raw.h`

Сирий реєстр збоїв (позначити, довести, підбити підсумок, вимкнути, мітка виходу) для власних наборів тестів SDK. Без `ZSDK_TESTS` він зупиняє збирання (`#error`), тож жоден модуль аддона до нього не дістається.

**Потік:** лише тестові програми SDK.

**Стабільність:** Internal

Аддони його не підключають: вони користуються `ledger::Window`, `ledger::Blocked` і `zsdk::Fire`, які ведуть реєстр за них.
