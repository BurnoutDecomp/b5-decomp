// Explicit-instantiation home for BrnWorld::NearMissData<A,B> -- the per-race-car near-miss /
// contact / crash / takedown / check bookkeeping. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Every method body is an inline template definition in BrnNearMissData.h (both instantiations
// share them). This TU forces the two X360-emitted instantiations so their out-of-line member
// copies are compiled by the gate:
//
//   NearMissData<4,7>  -- the race-car near-miss vein:
//     RememberNearMiss @0x822E4608  AddContacted @0x822E4780  AddCrashed @0x822E4868
//     AddTakenDown @0x822E4950      BackupNearArray @0x822E4BE8  UpdateTimers @0x822E4330
//     HasRecentlyCrashed @0x822E46F0  HasThereBeenARecentNearMiss @0x822C8988
//     IsRecentNearMiss @0x822E4AC8  IsRecentContacted @0x822E4B58  IsRecentlyTakeDown @0x822E4A38
//
//   NearMissData<4,8>  -- the 8-slot long-list sibling:
//     RememberNearMiss @0x822E3CC8  AddContacted @0x822E3E40  AddCrashed @0x822E3F28
//     AddChecked @0x822E4010        BackupNearArray @0x822E42A8  UpdateTimers @0x822E39F0
//     HasRecentlyCrashed @0x822E3DB0  IsRecentNearMiss @0x822E40F8  IsRecentContacted @0x822E4188
//     IsRecentTrafficCheck @0x822E4218
//
// The element-array accessors (Array<VehicleTimePair,{4,7,8}>::Append/Erase/operator[]) are the
// thin explicit instantiations already committed in the sibling Array_VehicleTimePair_{4,7,8}.cpp.
#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissData.h"

namespace BrnWorld
{
    template class NearMissData<4, 7>;
    template class NearMissData<4, 8>;
}
