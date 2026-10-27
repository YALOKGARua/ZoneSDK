#include <zsdk/overlay.h>

#include "../detail/trace.h"

#include <reshade.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <imstb_truetype.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <vector>

namespace zsdk::paint
{
    namespace
    {
        std::atomic<void (*)(float)> fontSink{ nullptr };

        void Off()
        {
            Rare("paint off-overlay", "paint: a draw call outside the overlay callback was skipped");
        }

        void Measure(float size, const char *line)
        {
            if (line != nullptr && *line != '\0')
            {
                if (void (*sink)(float) = fontSink.load(std::memory_order_relaxed))
                {
                    sink(size);
                }
            }
        }

        using Run = glyphs::Run;
        constexpr const auto &kRuns = glyphs::kRuns;

        constexpr int Count()
        {
            int total = 0;
            for (const Run &run : kRuns)
            {
                total += run.count;
            }
            return total;
        }

        constexpr int kGlyphs = Count();
        constexpr int kSmallest = 6;
        constexpr int kLargest = 96;
        constexpr int kSheetLimit = 4096;
        constexpr size_t kFacesKept = 16;
        constexpr unsigned kGraveFrames = 4;

        struct Face
        {
            int size = 0;
            int wide = 0;
            int tall = 0;
            int over = 1;
            float ascent = 0.0f;
            unsigned used = 0;
            reshade::api::resource resource{};
            reshade::api::resource_view view{};
            std::vector<stbtt_packedchar> chars;
        };

        struct Grave
        {
            reshade::api::resource resource{};
            reshade::api::resource_view view{};
            unsigned due = 0;
        };

        enum class Font
        {
            Untried,
            Loaded,
            Missing
        };

        reshade::api::device *device = nullptr;
        unsigned frame = 0;
        Font font = Font::Untried;
        bool crippled = false;
        std::vector<unsigned char> bytes;
        stbtt_fontinfo info{};
        std::vector<std::unique_ptr<Face>> faces;
        std::vector<Grave> graves;
        const Ink *themed = nullptr;

        int Step(const char *at)
        {
            const auto *raw = reinterpret_cast<const unsigned char *>(at);
            const unsigned char lead = raw[0];
            const int wide = lead < 0x80 ? 1 : (lead >> 5) == 0x06 ? 2 : (lead >> 4) == 0x0E ? 3 : (lead >> 3) == 0x1E ? 4 : 1;
            for (int i = 1; i < wide; ++i)
            {
                if ((raw[i] & 0xC0u) != 0x80u)
                {
                    return 1;
                }
            }
            return wide;
        }

        unsigned Sign(const char *at, int wide)
        {
            const auto *raw = reinterpret_cast<const unsigned char *>(at);
            if (wide == 1)
            {
                return raw[0] < 0x80 ? raw[0] : '?';
            }

            unsigned sign = wide == 2 ? raw[0] & 0x1Fu : wide == 3 ? raw[0] & 0x0Fu : raw[0] & 0x07u;
            for (int i = 1; i < wide; ++i)
            {
                sign = (sign << 6) | (raw[i] & 0x3Fu);
            }
            return sign;
        }

        int Slot(unsigned sign)
        {
            int at = 0;
            for (const Run &run : kRuns)
            {
                if (sign >= static_cast<unsigned>(run.first) && sign < static_cast<unsigned>(run.first + run.count))
                {
                    return at + static_cast<int>(sign - static_cast<unsigned>(run.first));
                }
                at += run.count;
            }
            return '?' - kRuns[0].first;
        }

        bool Slurp(const char *path)
        {
            FILE *file = nullptr;
            if (fopen_s(&file, path, "rb") != 0 || file == nullptr)
            {
                return false;
            }

            bool whole = false;
            if (fseek(file, 0, SEEK_END) == 0)
            {
                const long size = ftell(file);
                if (size > 0 && fseek(file, 0, SEEK_SET) == 0)
                {
                    bytes.resize(static_cast<size_t>(size));
                    whole = fread(bytes.data(), 1, bytes.size(), file) == bytes.size();
                }
            }
            fclose(file);

            if (!whole)
            {
                bytes.clear();
            }
            return whole;
        }

        void Load()
        {
            font = Font::Missing;

            char root[MAX_PATH]{};
            if (GetWindowsDirectoryA(root, MAX_PATH) == 0)
            {
                return;
            }

            for (const char *name : { "segoeui.ttf", "micross.ttf", "arial.ttf", "tahoma.ttf" })
            {
                char path[MAX_PATH]{};
                snprintf(path, sizeof(path), "%s\\Fonts\\%s", root, name);
                if (!Slurp(path))
                {
                    continue;
                }

                if (stbtt_InitFont(&info, bytes.data(), stbtt_GetFontOffsetForIndex(bytes.data(), 0)) != 0 &&
                    stbtt_FindGlyphIndex(&info, 0x0407) != 0)
                {
                    font = Font::Loaded;
                    zsdk::detail::Paint("font: %s, %zu bytes", path, bytes.size());
                    return;
                }
                bytes.clear();
            }

            zsdk::detail::Paint("font: no system font with cyrillic, text goes through the reshade font");
        }

        bool Own()
        {
            if (font == Font::Untried)
            {
                Load();
            }
            return font == Font::Loaded && !crippled && device != nullptr;
        }

        void Burn(reshade::api::device *gone, reshade::api::resource resource, reshade::api::resource_view view)
        {
            if (gone == nullptr)
            {
                return;
            }
            if (view.handle != 0)
            {
                gone->destroy_resource_view(view);
            }
            if (resource.handle != 0)
            {
                gone->destroy_resource(resource);
            }
        }

        void Bury(Face &face)
        {
            if (face.resource.handle != 0 || face.view.handle != 0)
            {
                graves.push_back({ face.resource, face.view, frame + kGraveFrames });
            }
            face.resource = {};
            face.view = {};
        }

        void Wipe(reshade::api::device *gone)
        {
            for (const auto &face : faces)
            {
                Burn(gone, face->resource, face->view);
            }
            faces.clear();

            for (const Grave &grave : graves)
            {
                Burn(gone, grave.resource, grave.view);
            }
            graves.clear();
        }

        bool Build(Face &face, int size)
        {
            face.size = size;
            face.over = size < 32 ? 2 : 1;
            face.chars.assign(kGlyphs, stbtt_packedchar{});

            stbtt_pack_range runs[std::size(kRuns)]{};
            int at = 0;
            for (size_t i = 0; i < std::size(kRuns); ++i)
            {
                runs[i].font_size = static_cast<float>(size);
                runs[i].first_unicode_codepoint_in_range = kRuns[i].first;
                runs[i].num_chars = kRuns[i].count;
                runs[i].chardata_for_range = face.chars.data() + at;
                at += kRuns[i].count;
            }

            const float px = static_cast<float>(size);

            float need = 0.0f;
            {
                unsigned char dot[64]{};
                stbtt_pack_context probe{};
                if (stbtt_PackBegin(&probe, dot, 8, 8, 0, 1, nullptr) == 0)
                {
                    return false;
                }
                stbtt_PackSetOversampling(&probe, static_cast<unsigned>(face.over), 1);

                std::vector<stbrp_rect> rects(kGlyphs);
                const int counted = stbtt_PackFontRangesGatherRects(&probe, &info, runs, static_cast<int>(std::size(runs)), rects.data());
                stbtt_PackEnd(&probe);

                for (int i = 0; i < counted; ++i)
                {
                    need += static_cast<float>(rects[i].w) * static_cast<float>(rects[i].h);
                }
                need *= 1.3f;
            }

            int wide = 128;
            int tall = 128;
            while (static_cast<float>(wide) * static_cast<float>(tall) < need && wide < kSheetLimit)
            {
                (tall < wide ? tall : wide) *= 2;
            }

            std::vector<unsigned char> alpha;
            for (;;)
            {
                alpha.assign(static_cast<size_t>(wide) * static_cast<size_t>(tall), 0);

                stbtt_pack_context pack{};
                if (stbtt_PackBegin(&pack, alpha.data(), wide, tall, 0, 1, nullptr) == 0)
                {
                    return false;
                }
                stbtt_PackSetOversampling(&pack, static_cast<unsigned>(face.over), 1);
                const int fit = stbtt_PackFontRanges(&pack, bytes.data(), 0, runs, static_cast<int>(std::size(runs)));
                stbtt_PackEnd(&pack);

                if (fit != 0)
                {
                    break;
                }
                if (wide >= kSheetLimit && tall >= kSheetLimit)
                {
                    return false;
                }
                (tall < wide ? tall : wide) *= 2;
            }

            int ascent = 0;
            int descent = 0;
            int gap = 0;
            stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
            face.ascent = std::round(static_cast<float>(ascent) * stbtt_ScaleForPixelHeight(&info, px));
            face.wide = wide;
            face.tall = tall;

            std::vector<uint32_t> rgba(alpha.size());
            for (size_t i = 0; i < alpha.size(); ++i)
            {
                rgba[i] = (static_cast<uint32_t>(alpha[i]) << 24) | 0x00FFFFFFu;
            }

            const reshade::api::resource_desc desc(static_cast<uint32_t>(wide), static_cast<uint32_t>(tall), 1, 1,
                                                   reshade::api::format::r8g8b8a8_unorm, 1, reshade::api::memory_heap::default_,
                                                   reshade::api::resource_usage::shader_resource);
            reshade::api::subresource_data data{};
            data.data = rgba.data();
            data.row_pitch = static_cast<uint32_t>(wide) * 4u;
            data.slice_pitch = data.row_pitch * static_cast<uint32_t>(tall);

            if (!device->create_resource(desc, &data, reshade::api::resource_usage::shader_resource, &face.resource))
            {
                face.resource = {};
                return false;
            }
            if (!device->create_resource_view(face.resource, reshade::api::resource_usage::shader_resource,
                                              reshade::api::resource_view_desc(reshade::api::format::r8g8b8a8_unorm), &face.view))
            {
                face.view = {};
                return false;
            }

            zsdk::detail::Paint("font: %d px baked into %dx%d", size, wide, tall);
            return true;
        }

        Face *Bake(float wanted)
        {
            if (!Own())
            {
                return nullptr;
            }

            const int size = std::clamp(static_cast<int>(std::lround(wanted)), kSmallest, kLargest);
            for (const auto &face : faces)
            {
                if (face->size == size)
                {
                    face->used = frame;
                    return face.get();
                }
            }

            if (faces.size() >= kFacesKept)
            {
                const auto oldest = std::min_element(faces.begin(), faces.end(),
                                                     [](const auto &a, const auto &b) { return a->used < b->used; });
                Bury(**oldest);
                faces.erase(oldest);
            }

            auto face = std::make_unique<Face>();
            if (!Build(*face, size))
            {
                Bury(*face);
                crippled = true;
                zsdk::detail::Paint("font: %d px atlas failed, text goes through the reshade font", size);
                return nullptr;
            }

            face->used = frame;
            faces.push_back(std::move(face));
            return faces.back().get();
        }

        void Put(ImDrawList *sheet, const Face &face, float &pen, float base, ImU32 tone, const char *from, const char *to)
        {
            const ImTextureRef tex(static_cast<ImTextureID>(face.view.handle));
            for (const char *walk = from; (to == nullptr || walk < to) && *walk != '\0';)
            {
                const int wide = Step(walk);
                const unsigned sign = Sign(walk, wide);
                walk += wide;

                stbtt_aligned_quad quad{};
                stbtt_GetPackedQuad(face.chars.data(), face.wide, face.tall, Slot(sign), &pen, &base, &quad, face.over == 1);
                if (sign == ' ' || sign == 0xA0 || quad.x1 <= quad.x0 || quad.y1 <= quad.y0)
                {
                    continue;
                }

                sheet->AddImage(tex, ImVec2(quad.x0, quad.y0), ImVec2(quad.x1, quad.y1),
                                ImVec2(quad.s0, quad.t0), ImVec2(quad.s1, quad.t1), tone);
            }
        }

        float Reach(const Face &face, const char *from, const char *to)
        {
            float pen = 0.0f;
            for (const char *walk = from; (to == nullptr || walk < to) && *walk != '\0';)
            {
                const int wide = Step(walk);
                pen += face.chars[Slot(Sign(walk, wide))].xadvance;
                walk += wide;
            }
            return pen;
        }

        float Borrowed(float size, const char *from, const char *to)
        {
            const float base = ImGui::GetFontSize();
            return base > 0.0f ? ImGui::CalcTextSize(from, to).x * size / base : 0.0f;
        }
    }

    void Serve(reshade::api::device *live)
    {
        if (device != live)
        {
            Wipe(device);
            crippled = false;
            device = live;
        }

        ++frame;
        for (size_t i = 0; i < graves.size();)
        {
            if (frame >= graves[i].due)
            {
                Burn(device, graves[i].resource, graves[i].view);
                graves.erase(graves.begin() + static_cast<ptrdiff_t>(i));
            }
            else
            {
                ++i;
            }
        }
    }

    void Drop(reshade::api::device *gone)
    {
        if (gone == nullptr || gone != device)
        {
            return;
        }
        Wipe(gone);
        device = nullptr;
    }

    bool Cyrillic()
    {
        if (!overlay::Inside())
        {
            Off();
            return false;
        }
        if (Own())
        {
            return true;
        }

        static const ImWchar probe[] = { 0x0410, 0x0430, 0x0404, 0x0407, 0x0456 };

        static ImFont *seen = nullptr;
        static bool kept = false;
        static DWORD checked = 0;

        ImFont *borrowed = ImGui::GetFont();
        if (borrowed == nullptr)
        {
            return false;
        }

        const DWORD now = GetTickCount();
        if (borrowed == seen && now - checked < 1000)
        {
            return kept;
        }

        kept = true;
        for (const ImWchar sign : probe)
        {
            if (!borrowed->IsGlyphInFont(sign))
            {
                kept = false;
                break;
            }
        }

        seen = borrowed;
        checked = now;
        return kept;
    }

    const Ink &ink()
    {
        static const Ink kept{
            IM_COL32(6, 7, 6, 240),
            IM_COL32(196, 184, 152, 72),
            IM_COL32(216, 208, 184, 28),
            IM_COL32(217, 162, 39, 255),
            IM_COL32(217, 162, 39, 28),
            IM_COL32(239, 236, 228, 255),
            IM_COL32(228, 224, 214, 255),
            IM_COL32(246, 232, 196, 255),
            IM_COL32(154, 148, 132, 255),
            IM_COL32(118, 112, 98, 255),
            IM_COL32(12, 12, 11, 150),
            IM_COL32(18, 17, 14, 255),
            IM_COL32(224, 80, 60, 255),
            IM_COL32(224, 80, 60, 140),
            IM_COL32(159, 209, 74, 255),
            IM_COL32(12, 12, 10, 220),
            IM_COL32(217, 162, 39, 42),
            IM_COL32(8, 8, 7, 170)
        };

        return themed != nullptr ? *themed : kept;
    }

    const Ink &terminal()
    {
        static const Ink kept{
            IM_COL32(3, 11, 6, 246),
            IM_COL32(38, 104, 62, 255),
            IM_COL32(30, 80, 50, 255),
            IM_COL32(106, 236, 142, 255),
            IM_COL32(70, 230, 120, 20),
            IM_COL32(182, 255, 200, 255),
            IM_COL32(116, 236, 150, 255),
            IM_COL32(182, 255, 200, 255),
            IM_COL32(82, 182, 112, 255),
            IM_COL32(66, 144, 94, 255),
            IM_COL32(70, 230, 120, 16),
            IM_COL32(2, 14, 6, 255),
            IM_COL32(255, 102, 84, 255),
            IM_COL32(255, 102, 84, 140),
            IM_COL32(176, 255, 140, 255),
            IM_COL32(4, 18, 10, 236),
            IM_COL32(80, 240, 130, 34),
            IM_COL32(2, 10, 5, 200)
        };

        return kept;
    }

    void Theme(const Ink *palette)
    {
        themed = palette;
    }

    void Box(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, ImU32 tone)
    {
        if (!overlay::Inside() || !sheet)
        {
            Off();
            return;
        }
        sheet->AddRectFilled(a, b, tone);
    }

    void Frame(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, ImU32 tone, float thick)
    {
        if (!overlay::Inside() || !sheet)
        {
            Off();
            return;
        }
        sheet->AddRect(a, b, tone, 0.0f, 0, thick);
    }

    void Rule(ImDrawList *sheet, const ImVec2 &a, float span, ImU32 tone, float thick)
    {
        if (!overlay::Inside() || !sheet)
        {
            Off();
            return;
        }
        sheet->AddRectFilled(a, ImVec2(a.x + span, a.y + thick), tone);
    }

    void Grain(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, float step, ImU32 tone)
    {
        if (!overlay::Inside() || !sheet)
        {
            Off();
            return;
        }
        const float pitch = step < 2.0f ? 2.0f : step;
        for (float y = a.y; y < b.y; y += pitch)
        {
            sheet->AddRectFilled(ImVec2(a.x, y), ImVec2(b.x, y + 1.0f), tone);
        }
    }

    void Corners(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, ImU32 tone, float len, float thick)
    {
        if (!overlay::Inside() || !sheet)
        {
            Off();
            return;
        }
        Box(sheet, a, ImVec2(a.x + len, a.y + thick), tone);
        Box(sheet, a, ImVec2(a.x + thick, a.y + len), tone);
        Box(sheet, ImVec2(b.x - len, a.y), ImVec2(b.x, a.y + thick), tone);
        Box(sheet, ImVec2(b.x - thick, a.y), ImVec2(b.x, a.y + len), tone);
        Box(sheet, ImVec2(a.x, b.y - thick), ImVec2(a.x + len, b.y), tone);
        Box(sheet, ImVec2(a.x, b.y - len), ImVec2(a.x + thick, b.y), tone);
        Box(sheet, ImVec2(b.x - len, b.y - thick), b, tone);
        Box(sheet, ImVec2(b.x - thick, b.y - len), b, tone);
    }

    void Bracket(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, ImU32 tone, float len, float thick)
    {
        Corners(sheet, a, b, tone, len, thick);
    }

    float Span(float size, const char *line)
    {
        if (!overlay::Inside())
        {
            Off();
            return 0.0f;
        }
        if (const Face *face = Bake(size))
        {
            return Reach(*face, line, nullptr);
        }
        return Borrowed(size, line, nullptr);
    }

    void Probe(void (*sink)(float pixels))
    {
        fontSink.store(sink, std::memory_order_relaxed);
    }

    void Text(ImDrawList *sheet, const ImVec2 &at, float size, ImU32 tone, const char *line)
    {
        if (!overlay::Inside())
        {
            Off();
            return;
        }
        Measure(size, line);
        if (const Face *face = Bake(size))
        {
            float pen = std::floor(at.x);
            Put(sheet, *face, pen, std::floor(at.y) + face->ascent, tone, line, nullptr);
            return;
        }
        sheet->AddText(ImGui::GetFont(), size, at, tone, line);
    }

    void Mid(ImDrawList *sheet, const ImVec2 &at, float size, ImU32 tone, const char *line)
    {
        Text(sheet, ImVec2(at.x - Span(size, line) * 0.5f, at.y), size, tone, line);
    }

    float Caps(ImDrawList *sheet, const ImVec2 &at, float size, ImU32 tone, const char *line, float gap)
    {
        if (!overlay::Inside())
        {
            Off();
            return 0.0f;
        }
        Measure(size, line);
        const Face *face = Bake(size);
        const float base = face != nullptr ? std::floor(at.y) + face->ascent : at.y;

        float pen = face != nullptr ? std::floor(at.x) : at.x;
        for (const char *walk = line; *walk != '\0';)
        {
            const int wide = Step(walk);
            if (*walk == ' ')
            {
                pen += size * 0.32f + gap;
                walk += wide;
                continue;
            }

            if (face != nullptr)
            {
                Put(sheet, *face, pen, base, tone, walk, walk + wide);
            }
            else
            {
                sheet->AddText(ImGui::GetFont(), size, ImVec2(pen, at.y), tone, walk, walk + wide);
                pen += Borrowed(size, walk, walk + wide);
            }
            pen += gap;
            walk += wide;
        }

        return pen - at.x - gap;
    }

    float CapsSpan(float size, const char *line, float gap)
    {
        if (!overlay::Inside())
        {
            Off();
            return 0.0f;
        }
        const Face *face = Bake(size);

        float pen = 0.0f;
        for (const char *walk = line; *walk != '\0';)
        {
            const int wide = Step(walk);
            if (*walk == ' ')
            {
                pen += size * 0.32f + gap;
                walk += wide;
                continue;
            }

            pen += (face != nullptr ? Reach(*face, walk, walk + wide) : Borrowed(size, walk, walk + wide)) + gap;
            walk += wide;
        }

        return pen > gap ? pen - gap : 0.0f;
    }
}
