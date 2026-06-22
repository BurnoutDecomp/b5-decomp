// Tiny embed/compile check for the RealmcIface::CardData home: includes the
// header and touches each reconstructed entry point so the gate exercises the
// full TU (all six member functions).
#include "SDKs/Realmc/RealmcCardData.h"

namespace
{
void RealmcCardDataEmbedCheck()
{
    using namespace RealmcIface;

    CardData a;
    CardData b;

    b = a;                          // operator=
    (void)(a == b);                 // operator==
    (void)(a != b);                 // operator!=
    a.PreviousState(b);             // PreviousState
    CardData& e = CardData::Empty();// Empty (static singleton)
    (void)e;
}
} // namespace
