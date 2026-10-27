#pragma once

#if defined(ZSDK_PRESENT_UNIT)
#error zsdk overlay headers are not allowed in a present unit
#endif

#include <imgui.h>
#include <reshade.hpp>

#include "zsdk.h"
#include "paint.h"

#include <span>
#include <string>

namespace zsdk
{
    struct Overlay
    {
        reshade::api::effect_runtime *runtime = nullptr;
        float scale = 1.0f;
        lang::Lang ui = lang::Lang::Uk;
    };

    namespace overlay
    {
        bool Inside();
        lang::Lang Ui();

        class Scope
        {
        public:
            Scope();
            ~Scope();
            Scope(const Scope &) = delete;
            Scope &operator=(const Scope &) = delete;
        };
    }

    namespace tab
    {
        inline constexpr ImU32 kShade = IM_COL32(8, 9, 8, 215);
        inline constexpr ImU32 kAccent = IM_COL32(217, 162, 39, 255);
        inline constexpr ImU32 kWarn = IM_COL32(224, 80, 60, 255);
        inline constexpr ImU32 kGood = IM_COL32(159, 209, 74, 255);
        inline constexpr ImU32 kText = IM_COL32(236, 232, 222, 255);
        inline constexpr ImU32 kDim = IM_COL32(160, 154, 138, 255);

        struct KeyTexts
        {
            const char *label = nullptr;
            const char *press = nullptr;
            const char *bad = nullptr;
            const char *game = nullptr;
        };

        struct DiagTexts
        {
            const char *title = nullptr;
            const char *phase = nullptr;
            const char *exe = nullptr;
            const char *known = nullptr;
            const char *other = nullptr;
            const char *unknown = nullptr;
            const char *verbs = nullptr;
            const char *crashed = nullptr;
            const char *none = nullptr;
            const char *retry = nullptr;
            const char *log = nullptr;
            const char *sdk = nullptr;
            const char *yes = nullptr;
            const char *no = nullptr;
            const char *markers = nullptr;
            const char *full = nullptr;
            const char *pump = nullptr;
            const char *report = nullptr;
            const char *copied = nullptr;
            const char *unsaved = nullptr;
            const char *unsupported = nullptr;
            const char *foreign = nullptr;
        };

        float Size();
        float Field();
        std::string Fixed(double value, int digits);
        ImU32 Fade(ImU32 tone, float k);
        void Label(const std::string &text, ImU32 tone = kText, bool framed = false);
        void Wrapped(const std::string &text, ImU32 tone = kText);
        void Beside(const std::string &text, ImU32 tone = kText);
        bool Box(const char *id, const std::string &text, bool &value, ImU32 tone = kText);
        bool Whole(const char *id, const char *text, int &value, int low, int high);
        bool Real(const char *id, const char *text, double &value, double low, double high, const char *format, double step);
        bool Button(const char *id, const std::string &text, float width = 0.0f);
        bool Clipped(const char *id, const std::string &text, float width);
        void Heading(const std::string &text);
        int KeyField(Overlay &o, const char *id, keys::Chord &chord, std::span<const keys::Chord> others, const KeyTexts &texts, float width);
        bool Capturing(const char *id);
        void Release();
        void Diagnostics(Overlay &o, const DiagTexts &texts = {});
        std::string Report(Overlay &o, const DiagTexts &texts = {});
    }

    namespace toast
    {
        void Draw(Overlay &o, float lane, std::string (*words)(const Item &, lang::Lang), uint32_t (*tone)(const Item &));
    }
}
