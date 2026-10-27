#include <zsdk/present.h>

#include "../detail/native_bindings.h"
#include "../detail/native_bindings_detail.h"
#include "../detail/trace.h"

#include <windows.h>
#include <atomic>
#include <mutex>

namespace zsdk::detail::native_bindings::detail
{
#include "../detail/native_bindings_catalog.inc"
}

namespace zsdk::detail::native_bindings
{
    namespace
    {
        std::once_flag once;
        std::atomic<Status> state{Status::Pending};
        std::uintptr_t base = 0;
        std::size_t imageSize = 0;
        detail::Result resolved;

        bool Sections(std::uintptr_t module, std::size_t &size, std::vector<detail::Section> &sections)
        {
            __try
            {
                const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module);
                if (!module || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < sizeof(IMAGE_DOS_HEADER) ||
                    dos->e_lfanew > 0x10000) return false;
                const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(module + dos->e_lfanew);
                if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
                    nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
                    nt->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER64) ||
                    !nt->FileHeader.NumberOfSections || nt->FileHeader.NumberOfSections > 96 ||
                    nt->OptionalHeader.SizeOfImage < 0x10000 || nt->OptionalHeader.SizeOfImage > 0x80000000u ||
                    nt->OptionalHeader.SizeOfHeaders > nt->OptionalHeader.SizeOfImage) return false;
                size = nt->OptionalHeader.SizeOfImage;
                const auto *first = IMAGE_FIRST_SECTION(nt);
                if (!detail::Contains(nt->OptionalHeader.SizeOfHeaders,
                    reinterpret_cast<std::uintptr_t>(first) - module,
                    std::uint64_t{nt->FileHeader.NumberOfSections} * sizeof(IMAGE_SECTION_HEADER))) return false;
                for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i)
                {
                    const auto &section = first[i];
                    if (!section.Misc.VirtualSize) continue;
                    if (section.VirtualAddress < nt->OptionalHeader.SizeOfHeaders ||
                        !detail::Contains(size, section.VirtualAddress, section.Misc.VirtualSize)) return false;
                    sections.push_back({section.VirtualAddress, section.Misc.VirtualSize,
                        (section.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0,
                        (section.Characteristics & IMAGE_SCN_MEM_READ) != 0});
                }
                return !sections.empty();
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        bool CopyCode(void *out, const void *source, std::size_t size)
        {
            __try
            {
                std::memcpy(out, source, size);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        bool Scan(std::uintptr_t module, std::size_t size, std::vector<detail::Section> &sections)
        {
            std::vector<std::vector<std::uint8_t>> copies(sections.size());
            for (std::size_t i = 0; i < sections.size(); ++i)
            {
                auto &section = sections[i];
                if (!section.executable) continue;
                if (!section.readable) return false;
                copies[i].resize(section.size);
                if (!CopyCode(copies[i].data(), reinterpret_cast<const void *>(module + section.rva), section.size)) return false;
                section.bytes = copies[i].data();
            }
            return detail::Resolve(reinterpret_cast<const std::uint8_t *>(module), size, sections,
                detail::kPatterns, resolved, detail::kLinks);
        }

        void InitializeOnce()
        {
            state.store(Status::Scanning, std::memory_order_release);
            const auto started = GetTickCount64();
            const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
            std::vector<detail::Section> sections;
            std::size_t size = 0;
            const bool valid = Sections(module, size, sections) && Scan(module, size, sections);
            if (valid)
            {
                base = module;
                imageSize = size;
                zsdk::detail::Trace("native bindings: ready; %zu addresses, %zu of %zu patterns matched, %llu ms; image size 0x%zX",
                    resolved.bindings.size(), resolved.matched, resolved.patterns, GetTickCount64() - started, imageSize);
                state.store(Status::Ready, std::memory_order_release);
            }
            else
            {
                resolved.bindings.clear();
                state.store(Status::Rejected, std::memory_order_release);
                zsdk::detail::Trace("native bindings: incompatible; reason %u, key 0x%X, %llu ms; no native addresses published",
                    static_cast<unsigned>(resolved.failure), resolved.key, GetTickCount64() - started);
            }
        }
    }

    bool Initialize() noexcept
    {
        try
        {
            std::call_once(once, []() noexcept {
                try { InitializeOnce(); }
                catch (...)
                {
                    resolved.bindings.clear();
                    state.store(Status::Rejected, std::memory_order_release);
                }
            });
        }
        catch (...)
        {
            state.store(Status::Rejected, std::memory_order_release);
            return false;
        }
        return Ready();
    }

    bool Ready() noexcept { return state.load(std::memory_order_acquire) == Status::Ready; }
    Status State() noexcept { return state.load(std::memory_order_acquire); }
    std::uintptr_t Base() noexcept { return Ready() ? base : 0; }
    std::size_t ImageSize() noexcept { return Ready() ? imageSize : 0; }

    std::uintptr_t Address(std::uint32_t baselineRva) noexcept
    {
        if (!Ready()) return 0;
        const auto &bindings = resolved.bindings;
        const auto found = std::lower_bound(bindings.begin(), bindings.end(), baselineRva,
            [](const detail::Binding &binding, std::uint32_t key) { return binding.key < key; });
        return found != bindings.end() && found->key == baselineRva ? base + found->rva : 0;
    }
}
