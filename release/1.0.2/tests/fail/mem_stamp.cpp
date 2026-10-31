#include <zsdk/present.h>

bool Probe(void *at)
{
    return zsdk::mem::Stamp(at, 0x40000000u);
}
