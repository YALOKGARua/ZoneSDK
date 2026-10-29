#pragma once

#include <zsdk/present.h>

#include "../../src/detail/engine_core.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>

namespace fake
{
    inline constexpr int32_t kStride = 24;
    inline constexpr size_t kBytes = 0x400;
    inline constexpr int32_t kCap = 256;
    inline constexpr int32_t kPropertiesAt = 0x50;
    inline constexpr int32_t kPropertyNextAt = 0x20;
    inline constexpr int32_t kPropertyNameAt = 0x28;
    inline constexpr int32_t kPropertyOffsetAt = 0x4C;
    inline constexpr int32_t kInterfacesAt = 0x1F0;
    inline constexpr int32_t kItemSerialAt = 0x10;

    struct Memory
    {
        uint8_t objects[kCap][kBytes];
        uint8_t spare[kCap][kBytes];
        uint8_t pool[1 << 16];
        uint8_t items[kCap * kStride];
        uint8_t *blocks[1];
        uint8_t *slabs[1];
        uint8_t **chart;
        size_t used;
        int32_t count;
        int32_t spares;
    };

    class Game
    {
    public:
        Game() : m(new Memory{})
        {
            m->blocks[0] = m->pool;
            m->slabs[0] = m->items;
            m->chart = m->slabs;
            m->used = 2;
            klass = Make("Class", nullptr, nullptr);
            Put(klass, zsdk::ue::kClassAt, static_cast<const void *>(klass));
            object = Type("Object", nullptr);
            function = Type("Function", object);
            zsdk::detail::engine::Pose(m->blocks, reinterpret_cast<uint8_t *>(&m->chart), reinterpret_cast<uint8_t *>(&m->count), kStride);
        }

        Game(const Game &) = delete;
        Game &operator=(const Game &) = delete;

        uint32_t Name(const std::string &text)
        {
            const auto found = names.find(text);
            if (found != names.end()) return found->second;
            if (text.empty() || text.size() > 200 || m->used + 2 + text.size() + 1 > sizeof(m->pool)) throw std::runtime_error("fake name does not fit: " + text);
            const uint32_t slot = static_cast<uint32_t>(m->used / 2);
            const uint16_t header = static_cast<uint16_t>(text.size() << 6);
            std::memcpy(m->pool + m->used, &header, sizeof(header));
            std::memcpy(m->pool + m->used + 2, text.data(), text.size());
            m->used += 2 + text.size();
            m->used = (m->used + 1) & ~static_cast<size_t>(1);
            names.emplace(text, slot);
            return slot;
        }

        uint8_t *Make(const std::string &name, const void *type, const void *outer, uint32_t number = 0)
        {
            if (m->count >= kCap) throw std::runtime_error("the fake object array is full");
            const int32_t index = m->count;
            uint8_t *thing = m->objects[index];
            Put(thing, zsdk::ue::kIndexAt, index);
            Put(thing, zsdk::ue::kClassAt, type);
            const uint32_t pair[2] = { Name(name), number };
            std::memcpy(thing + zsdk::ue::kNameAt, pair, sizeof(pair));
            Put(thing, zsdk::ue::kOuterAt, outer);
            uint8_t *slot = m->items + static_cast<size_t>(index) * kStride;
            Put(slot, 0, static_cast<const void *>(thing));
            Put(slot, kItemSerialAt, index + 1);
            ++m->count;
            return thing;
        }

        uint8_t *Type(const std::string &name, const void *super)
        {
            uint8_t *type = Make(name, klass, nullptr);
            Put(type, zsdk::ue::kSuperAt, super);
            return type;
        }

        uint8_t *Loose()
        {
            if (m->spares >= kCap) throw std::runtime_error("the fake spare memory is full");
            return m->spare[m->spares++];
        }

        void Property(uint8_t *type, const std::string &name, int32_t at)
        {
            uint8_t *prop = Loose();
            const uint32_t pair[2] = { Name(name), 0 };
            std::memcpy(prop + kPropertyNameAt, pair, sizeof(pair));
            Put(prop, kPropertyOffsetAt, at);
            Put(prop, kPropertyNextAt, Get(type, kPropertiesAt));
            Put(type, kPropertiesAt, static_cast<const void *>(prop));
        }

        void Implements(uint8_t *type, const void *face, int32_t offset)
        {
            uint8_t *list = Loose();
            Put(list, 0, face);
            Put(list, 8, offset);
            Put(type, kInterfacesAt, static_cast<const void *>(list));
            Put(type, kInterfacesAt + 8, int32_t{ 1 });
            Put(type, kInterfacesAt + 12, int32_t{ 1 });
        }

        void Link(uint8_t *thing, int32_t at, const void *value)
        {
            Put(thing, at, value);
        }

        void ObjectFlags(uint8_t *thing, uint32_t flags)
        {
            Put(thing, zsdk::ue::kItemFlagsAt, flags);
        }

        void ItemFlags(const void *thing, uint32_t flags)
        {
            Put(Slot(thing), zsdk::ue::kItemFlagsAt, flags);
        }

        void Serial(const void *thing, int32_t serial)
        {
            Put(Slot(thing), kItemSerialAt, serial);
        }

        uint8_t *klass = nullptr;
        uint8_t *object = nullptr;
        uint8_t *function = nullptr;

    private:
        template<class T> static void Put(void *at, int32_t offset, const T &value)
        {
            std::memcpy(static_cast<uint8_t *>(at) + offset, &value, sizeof(T));
        }

        static const void *Get(const void *at, int32_t offset)
        {
            const void *value = nullptr;
            std::memcpy(&value, static_cast<const uint8_t *>(at) + offset, sizeof(value));
            return value;
        }

        uint8_t *Slot(const void *thing)
        {
            int32_t index = -1;
            std::memcpy(&index, static_cast<const uint8_t *>(thing) + zsdk::ue::kIndexAt, sizeof(index));
            if (index < 0 || index >= m->count) throw std::runtime_error("not a fake object");
            return m->items + static_cast<size_t>(index) * kStride;
        }

        Memory *m;
        std::map<std::string, uint32_t> names;
    };
}
