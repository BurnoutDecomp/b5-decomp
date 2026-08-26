#include "BrnRouteMapModule.h"

#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISectionsData (complete type)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnAI::RouteMapModule::RouteMapModule @0x827E23F0
//   BrnAI::RouteMapModule::Prepare        @0x8277FDE8
//
// The ctor constructs the two read/write mutexes (handled by member construction),
// clears the intrusive-list anchor and wires its node pointers back to the
// anchor (empty circular list), and installs the static dispatch table.

namespace BrnAI
{
RouteMapModule::RouteMapModule()
{
    mAnchorState = 0;
    mUnk6505     = 0;
    mUnk6506     = 0;

    mpListHead   = &mAnchorState;
    mpListTail   = &mAnchorState;
    mpListCursor = &mAnchorState;
    mUnk6510     = 0;

    // Guest static dispatch table at 0x820CDFC4.
    mpAllocatorIface = reinterpret_cast<void*>(0x820CDFC4);
}

// ---------------------------------------------------------------------------------------
// Prepare @0x8277FDE8 -- the AI module's stage 3 (its vtable slot 16; see the header).
//
//   0x8277FE04  bl  CgsResource::BaseResourcePtr::CreateFromHandle(this + 26016, &lHandle)
//   0x8277FE10  bl  CgsDev::DebugComponent::Register(this + 26052)
//   0x8277FE18  bl  BrnAI::ResourcePtr<AISectionsData>::GetMemoryResource(this + 26016)
//   0x8277FE24  bl  BrnAI::AStar::Construct(this + 552, <that>)
//   0x8277FE2C  b   CgsModule::ModuleSingleBuffered::Prepare(this)
//
// The CreateFromHandle call is spelled here as the ResourcePtr's own
// `operator=(const ResourceHandle&)`, which the committed CgsResourcePtr.h documents as
// compiling to exactly that ONE call at every attested assign site (FlaptManager::Construct,
// the ChallengeList/WheelList slot resets) -- no list unlink, no re-init around it.
//
// ⚠️ [FLAG PC boot gate] the `DebugComponent::Register(this + 26052)` step is PARKED: the
// route-map debug component that lives at that offset is not a member of this class yet (it
// sits inside mWorkingSetPad), and registering an object that was never constructed is the
// [[valid-pointer-invalid-object]] shape -- CgsDev::DebugComponent::Register links it into the
// global debug list, after which the debug UI walks it every frame. Nothing on the reset-on-track
// path needs it. Restore it WITH the component as a named member.
bool RouteMapModule::Prepare(CgsResource::ResourceHandle lHandle)
{
    mAISectionsData = lHandle;

    // [FLAG PC boot gate] CgsDev::DebugComponent::Register(this + 26052) -- see the banner.

    // ⚠️ [FLAG PC link gate] `AStar::Construct(this + 552, mAISectionsData.GetMemoryResource())`
    // is PARKED, and it is a LINK closure problem, not a reconstruction one: AStar::Construct
    // itself is fully bodied in BrnAStar.cpp, but that TU is not link-complete -- its Compute /
    // BuildRoute reference BrnAI::AStarNodePool::FindNode and BrnAI::AStar::IsBlockSection, which
    // have NO body anywhere in the tree, plus BrnAI::Route::AddNode and
    // BrnAI::Portal::GetLinkSectionIndex from the unmounted route TUs (measured: mounting it costs
    // 5 x LNK2019). Nothing on the reset-on-track path runs a search -- the manager reads the
    // section graph directly through mAISectionsData -- so the binding the pathfinder needs can
    // land with the route-request slice that first uses it. Un-park this WITH BrnAStar.cpp's
    // closure, not before: an AStar whose mpAISectionsData is never set is
    // [[valid-pointer-invalid-object]] waiting to happen.

    return CgsModule::ModuleSingleBuffered::Prepare();
}

// The route map's bound road network (X360 offset 0x65A0; RouteMapDebugComponent::OnActivate
// @0x8277FE50 reads it through the same ResourcePtr accessor).
AISectionsData* RouteMapModule::GetAISectionsData() const
{
    return const_cast<AISectionsData*>(mAISectionsData.GetMemoryResource());
}
}
