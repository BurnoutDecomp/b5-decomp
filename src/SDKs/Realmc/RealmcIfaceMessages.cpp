#include "SDKs/Realmc/RealmcIfaceMessages.h"

// RealmcIface::MessageBootupDone -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, class:RealmcIface::MessageBootupDone):
//   MessageBootupDone::Apply @0x82B55220
//   MessageBootupDone::`scalar deleting destructor` @0x82B55240 (compiler-generated
//   from the virtual dtor + the class operator delete in the header)

namespace RealmcIface
{

// @ 0x82B55220 -- forward into the handler's vtable slot 15 (`lwz 0x3C(vtbl)`).
int MessageBootupDone::Apply(IRealmcIfaceHandler* lpHandler)
{
    return lpHandler->OnBootupDone(this);
}

}
