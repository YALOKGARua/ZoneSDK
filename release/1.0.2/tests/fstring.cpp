#include <zsdk/present.h>
#include <zsdk/test/check.h>

#include <cstddef>
#include <cstring>
#include <string>

int main()
{
    using check::That;
    namespace ue = zsdk::ue;
    static_assert(sizeof(ue::Lent) == 16 && offsetof(ue::Lent, data) == 0 && offsetof(ue::Lent, num) == 8 && offsetof(ue::Lent, max) == 12);
    const std::wstring sid = L"ZOF_Main_Open";
    alignas(16) uint8_t block[64]{};
    const ue::Lent lent = ue::Lend(sid);
    std::memcpy(block + 16, &lent, sizeof(lent));
    const wchar_t *data = nullptr;
    int32_t num = 0;
    int32_t max = 0;
    std::memcpy(&data, block + 16, sizeof(data));
    std::memcpy(&num, block + 24, sizeof(num));
    std::memcpy(&max, block + 28, sizeof(max));
    That(data == sid.c_str() && num == static_cast<int32_t>(sid.size() + 1) && max == num, "an FString parameter is the borrowed buffer, the length with the zero and the same capacity");
    That(ue::CopyText(block + 16) == sid, "CopyText reads the FString back from the parameter block");
    const std::wstring empty;
    const ue::Lent none = ue::Lend(empty);
    That(none.data == empty.c_str() && none.num == 1 && none.max == 1 && ue::CopyText(&none).empty(), "an empty string is a valid FString of one zero");
    const ue::Lent literal = ue::Lend(L"XDestroyModel");
    That(literal.num == 14 && literal.max == 14 && ue::CopyText(&literal) == L"XDestroyModel", "a string literal can be lent");
    const ue::Lent missing = ue::Lend(static_cast<const wchar_t *>(nullptr));
    That(!missing.data && missing.num == 0 && missing.max == 0, "a null pointer lends nothing");
    const std::wstring longest(ue::kLendCap - 1, L'x');
    const ue::Lent fits = ue::Lend(longest);
    That(fits.data == longest.c_str() && fits.num == static_cast<int32_t>(ue::kLendCap), "the longest allowed string is lent");
    const std::wstring tooLong(ue::kLendCap, L'x');
    const ue::Lent refused = ue::Lend(tooLong);
    That(!refused.data && refused.num == 0 && refused.max == 0, "an over-long string is refused as an empty view");
    That(!ue::Lend(tooLong.c_str()).data, "an over-long literal is refused too");
    ue::Lent broken{ sid.c_str(), 20, 10 };
    That(ue::CopyText(&broken).empty(), "a count above the capacity is not read");
    broken = ue::Lent{ nullptr, 4, 4 };
    That(ue::CopyText(&broken).empty() && ue::CopyText(nullptr).empty() && ue::CopyText(reinterpret_cast<const void *>(uintptr_t{ 0x10 })).empty(), "null data and unreadable addresses give an empty text");
    broken = ue::Lent{ reinterpret_cast<const wchar_t *>(uintptr_t{ 0x20 }), 4, 4 };
    That(ue::CopyText(&broken).empty(), "unreadable characters give an empty text without a crash");
    const wchar_t inner[] = { L'a', L'b', 0, L'c', 0 };
    broken = ue::Lent{ inner, 5, 5 };
    That(ue::CopyText(&broken) == L"ab", "the text ends at the first zero");
    return check::Finish("fstring");
}
