// SharedClasses/Trigger/BrnKillzone.cpp
//
// [gateui] NEW TU, 2026-08-20. One body: `BrnTrigger::Killzone::GetTrigger`.
//
// WHY IT EXISTS AS A .cpp RATHER THAN A HEADER INLINE: the accessor returns a
// `const GenericRegion*` out of the killzone's pointer table, and BrnKillzone.h
// deliberately only FORWARD-DECLARES GenericRegion (its own banner says
// "body needs complete GenericRegion -> declaration only"). Reading the element is
// fine with an incomplete type, so this could have been inlined by including
// BrnGenericRegion.h from BrnKillzone.h -- that is not done, to keep the header's
// include graph (which BrnTriggerData.h and the whole trigger family sit on)
// untouched.
//
// LINK EVIDENCE: `?GetTrigger@Killzone@BrnTrigger@@QEBAPEBUGenericRegion@2@H@Z`
// is a measured UNDEF external in BrnTriggerQueryManager.obj, and it is one of the
// thirteen unresolved externals build_game_exe.bat:2369-2375 lists as the reason
// that TU is not mounted.
//
// SHAPE: DWARF references/DecFIGS/dwarfdump/SharedClasses/Trigger/BrnKillzone.h:33
//   `const GenericRegion * GetTrigger(int32_t) const;`
// -- reproduced verbatim by the committed declaration.
//
// NO ASSERT IS ADDED. The accessor is header-inlined on the console (there is no
// `Killzone::GetTrigger` entry in progress/identity.json), so no assert text or
// line number is recoverable, and its bodied sibling GetRegionId() in the same
// header carries none either. Inventing one would be a fabrication.

#include "SharedClasses/Trigger/BrnKillzone.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"   // completes the returned type

namespace BrnTrigger
{

const GenericRegion*
Killzone::GetTrigger( int32_t liIndex ) const
{
    return mppTriggers[liIndex];
}

} // namespace BrnTrigger
