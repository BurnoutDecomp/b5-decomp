// Tiny embed/compile check for the RealmcIface::TitleInfo home: includes the header
// and touches both reconstructed entry points so the gate exercises the full TU.
#include "SDKs/Realmc/RealmcTitleInfo.h"

namespace
{
void RealmcTitleInfoEmbedCheck()
{
    RealmcIface::TitleInfo& rEmpty = RealmcIface::TitleInfo::Empty();  // singleton
    RealmcIface::TitleInfo  copy(rEmpty);                              // copy ctor
    (void)copy;
}
} // namespace
