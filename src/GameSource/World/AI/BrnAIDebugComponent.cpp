#include "GameSource/World/AI/BrnAIDebugComponent.h"

#include "GameSource/World/AI/BrnAIModule.h"                                      // BrnAI::AIModule (member-pointer completeness)

// BrnAI::AIDebugComponent -- the in-game "Main AI" debug overlay. Reconstructed from the X360
// ARTIST build (function addresses cited per method) cross-checked against the DecFIGS DWARF
// (member names / virtual shape) and the recovered base CgsDev::DebugComponent.
//
// SCOPE NOTE: the per-car draw helpers (DrawAICarRoute / DrawAICarSection / DrawCarControlData /
// DrawAggressiveTargetPoints / DrawAggressiveWarning / DrawCarIndices / DrawChevrons /
// DrawPortalTargets / DrawDirectDestinationVector / DrawResetOnTrackAISection / DrawAISectionEdge /
// StateTableDrawEntry / DeactiveateDriversCallback) all walk the AIModule's per-car driver array
// by internal byte offset in the X360 build (the AIDriver/AICar member reads are inlined, e.g.
// driver-array stride 0x1D70 (7536) with the activity flag at +199609 and the AICar pointer at
// +199472). Those internal AIModule/AIDriver layouts are NOT yet reconstructed in the tree, so
// reconstructing those bodies by NAME (the project rule -- no raw-offset pointer hacks, no
// fabricated members) is blocked until the AIModule driver array + AIDriver layout land. They are
// declared in the header (vtable/declaration surface) and bodied in their own follow-on once the
// layouts exist.
//
// RenderWorld is ALSO held back here: the X360 dispatch (@0x82796A08) gates each draw helper on a
// member read by raw word offset (*(v2+44)..*(v2+55)). Mapping each of those offsets to a specific
// named bool would require this class's exact member-offset table relative to the base
// DebugComponent size; that mapping is not yet pinned (the toggle->member correspondence cannot be
// grounded without the asm offset table for THIS object), so baking in a guessed mapping would
// fabricate it. RenderWorld is therefore declared-only until the offset table is pinned. This TU
// bodies the functions that need no un-reconstructed layout and no offset guesswork: GetName and
// GetPath.

namespace BrnAI
{

// @0x827671B8  The component's display name in the debug menu.
const char* AIDebugComponent::GetName() const
{
    return "Main AI";
}

// @0x82767678  The component's debug-menu group path.
const char* AIDebugComponent::GetPath() const
{
    return "AI";
}

}
