#include <zsdk/present.h>

#include <string>

int Probe()
{
    const zsdk::ue::Lent view = zsdk::ue::Lend(std::wstring(L"gone after this line"));
    return view.num;
}
