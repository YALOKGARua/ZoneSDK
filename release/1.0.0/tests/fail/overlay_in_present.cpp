#include <zsdk/overlay.h>

int Probe()
{
    return zsdk::tab::Size() > 0.0f ? 1 : 0;
}
