#pragma once

#if defined(ZSDK_PRESENT_UNIT)
#error zsdk overlay headers are not allowed in a present unit
#endif

#include <imgui.h>

namespace reshade::api
{
    struct device;
}

namespace zsdk::paint
{
    struct Ink
    {
        ImU32 glass;
        ImU32 edge;
        ImU32 hair;
        ImU32 amber;
        ImU32 wash;
        ImU32 paper;
        ImU32 text;
        ImU32 warm;
        ImU32 dim;
        ImU32 faint;
        ImU32 tile;
        ImU32 dark;
        ImU32 foe;
        ImU32 veiled;
        ImU32 life;
        ImU32 soot;
        ImU32 glow;
        ImU32 rail;
    };

    const Ink &ink();
    const Ink &terminal();
    void Theme(const Ink *palette);

    void Serve(reshade::api::device *device);
    void Drop(reshade::api::device *device);
    bool Cyrillic();

    void Box(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, ImU32 tone);
    void Frame(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, ImU32 tone, float thick = 1.0f);
    void Rule(ImDrawList *sheet, const ImVec2 &a, float span, ImU32 tone, float thick = 1.0f);
    void Grain(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, float step, ImU32 tone);
    void Corners(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, ImU32 tone, float len, float thick);
    void Bracket(ImDrawList *sheet, const ImVec2 &a, const ImVec2 &b, ImU32 tone, float len, float thick);

    float Span(float size, const char *line);
    void Text(ImDrawList *sheet, const ImVec2 &at, float size, ImU32 tone, const char *line);
    void Mid(ImDrawList *sheet, const ImVec2 &at, float size, ImU32 tone, const char *line);
    float Caps(ImDrawList *sheet, const ImVec2 &at, float size, ImU32 tone, const char *line, float gap);
    float CapsSpan(float size, const char *line, float gap);
    void Probe(void (*sink)(float pixels));
}
