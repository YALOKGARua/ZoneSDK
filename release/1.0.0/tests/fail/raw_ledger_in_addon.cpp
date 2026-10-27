#include <zsdk/test/ledger_raw.h>

bool Probe()
{
    return zsdk::ledger::raw::Settle();
}