// Tiny embed/compile check for the RealmcIface::LoadEntryInfo home: includes the header
// and touches both reconstructed entry points so the gate exercises the full TU.
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"

namespace
{
void RealmcLoadEntryInfoEmbedCheck()
{
    RealmcIface::LoadEntryInfo a;  // default ctor
    RealmcIface::LoadEntryInfo b;
    b = a;                         // operator=
    (void)b;
}
} // namespace
