#pragma once

#include "census.h"
#include "lang.h"
#include "toast.h"
#include "world.h"

#include <cstdint>
#include <string>

namespace reshade::api
{
    struct device;
    struct command_list;
    struct pipeline_subobject;
}

namespace zsdk
{
    class Input;
    struct Overlay;

    struct Hooks
    {
        void (*boot)() = nullptr;
        void (*refresh)(bool again) = nullptr;
        void (*prime)() = nullptr;
        void (*advance)() = nullptr;
        void (*step)(uint64_t now) = nullptr;
        bool (*hot)() = nullptr;
        void (*publish)(uint64_t now) = nullptr;
        void (*turn)(const world::Turn &turn) = nullptr;
        void (*order)(int order) = nullptr;
        const char *(*orderName)(int order) = nullptr;
        void (*quit)() = nullptr;
        void (*census)(const census::Snapshot &snapshot) = nullptr;
        bool (*complete)(const census::Snapshot &snapshot) = nullptr;
        void (*frame)(const Input &input) = nullptr;
        void (*overlay)(Overlay &overlay) = nullptr;
        void (*page)(Overlay &overlay) = nullptr;
        void (*opened)(bool open) = nullptr;
        void (*stop)() = nullptr;
        std::string (*words)(const toast::Item &item, lang::Lang ui) = nullptr;
        uint32_t (*tone)(const toast::Item &item) = nullptr;
        void (*pipelineInit)(reshade::api::device *device, uint64_t layout, uint32_t count, const reshade::api::pipeline_subobject *subobjects, uint64_t pipeline) = nullptr;
        void (*pipelineBind)(reshade::api::command_list *list, uint32_t stages, uint64_t pipeline) = nullptr;
        void (*pipelineDestroy)(reshade::api::device *device, uint64_t pipeline) = nullptr;
        void (*deviceDestroy)(reshade::api::device *device) = nullptr;
        std::string (*report)() = nullptr;
    };

    namespace hooks
    {
        void Bind(const Hooks &hooks);
        const Hooks &Get();
    }
}
