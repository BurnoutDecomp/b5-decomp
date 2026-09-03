#ifndef BRN_ROUTE_MAP_MODULE_H
#define BRN_ROUTE_MAP_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // the module base
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"     // ResourcePtr<AISectionsData>
#include "GameSource/World/AI/Route/BrnAStar.h"                        // BrnAI::AStar (embedded)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"             // the "Route" IO buffer pair
#include <eathread/eathread_rwmutex.h>

namespace CgsModule { struct IOBufferStack; }

namespace BrnAI
{
struct AISectionsData;

// Reconstructed from DWARF (BrnRouteMapModule.h:48-52). An 8-byte (section,portal) index pair;
// the element type of the RacingLineGenerator's ExtrapolatedIndexArray
// (CgsContainers::Array<BrnAI::SectionAndPortalIndices,16u>, typedef ExtrapolatedIndexArray).
// Two u32 words -> stride 8, matching the X360 Array<...,16>::operator[] `index*8 + base`
// accessor @0x8276AA08 (slwi r11,r28,3; add r3,r29) and the live-count word at byte +0x80
// (== 16*8).
struct SectionAndPortalIndices
{
    u32 muSection; // DWARF BrnRouteMapModule.h:50
    u32 muPortal;  // DWARF BrnRouteMapModule.h:51
};

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E23F0.
// The X360 layout places a large embedded working set between the two
// read/write mutexes and the trailing intrusive-list anchor; the unknown
// span is preserved with an explicit padding buffer so every recovered
// member is accessed by name (no raw offset casts).
//
// ⭐ 2026-08-25 (aimodule wave): this IS a CgsModule module. AIModule::Construct
// @0x82794D08 calls `ModuleSingleBuffered::Construct(this + 295896)` on it, AIModule::Release
// calls its vtable slot 2 and AIModule::Destruct its slot 3, and AIModule::Prepare stage 3
// calls SLOT 16 -- which is one past ModuleSingleBuffered's last virtual (0..9 public, 10..13
// the four protected DataStructure hooks, 14/15 CreateInput/CreateOutputDataStructure), i.e.
// RouteMapModule's OWN first virtual. That slot is RouteMapModule::Prepare @0x8277FDE8. The
// arithmetic landing exactly on 16 is what pins the whole module-vtable model.
//
// ⭐ 2026-09-03 (aiwave A5): SLOT 17 (vtbl+68, `lwz r11,0x44(vtbl); mtctr; bctrl` at
// AIModule::Update 0x8279B7C4) is the module's own Update(IOBufferStack*, IOBufferStack*,
// const RouteMapModuleIO::InputBuffer*, RouteMapModuleIO::OutputBuffer*) @0x82793ED8 -- the
// DWARF declares Prepare, Release, Destruct, Update in that order and Release/Destruct
// override base slots, so Update is the second NEW virtual == slot 17. Written by name.
class RouteMapModule : public CgsModule::ModuleSingleBuffered
{
public:
    RouteMapModule();

    // X360 0x8277FDE8 (its own virtual, slot 16 -- see the banner). FIVE lines:
    //   ResourcePtr<AISectionsData>::CreateFromHandle(this + 26016, &lHandle)
    //   CgsDev::DebugComponent::Register(this + 26052)
    //   AStar::Construct(this + 552, ResourcePtr<AISectionsData>::GetMemoryResource())
    //   return ModuleSingleBuffered::Prepare(this)
    // The console takes the handle BY VALUE in one 64-bit GPR (`ld r4, 0(...)` at the call
    // site) -- two 32-bit pointers packed; kept by value here for the same reason.
    bool Prepare(CgsResource::ResourceHandle lHandle);

    // The base's no-arg Update() stays reachable by name (the console hides it the same way).
    using CgsModule::ModuleSingleBuffered::Update;

    // X360 0x82793ED8 (slot 17, vtbl+68; DWARF BrnRouteMapModule.cpp:90). The per-frame route
    // service: read-lock the input / write-lock the output, service AT MOST ONE race route per
    // frame (the RaceRouteRequest queue has capacity 1 and a multi-frame A* keeps going with a
    // NULL request while mAStar.IsInProgress()), then every extrapolated request, then unlock.
    // Called by AIModule::Update on the transient "Route" buffer pair it creates for the frame.
    virtual void Update(CgsModule::IOBufferStack* lpInputBufferStack,
                        CgsModule::IOBufferStack* lpOutputBufferStack,
                        const RouteMapModuleIO::InputBuffer* lpInputBuffer,
                        RouteMapModuleIO::OutputBuffer* lpOutputBuffer);

    // The route map owns a CgsResourcePtr to the loaded AISectionsData;
    // RouteMapDebugComponent::OnActivate (@0x8277FE50) resolves it via
    // ResourcePtr<AISectionsData>::GetMemoryResource(this + 0x65A0). X360 offset 0x65A0 (26016).
    AISectionsData* GetAISectionsData() const;

    // Has Prepare bound a road network yet? (Raw test, no assert -- the same
    // load-and-branch idiom the X360 uses wherever an unloaded resource is legal.)
    bool HasAISectionsData() const { return mAISectionsData.HasMemoryResource(); }

    // X360 AIModule::Construct's `*(this + 295900) = 1` -- 295900 == this module's base + 4 ==
    // Module::mbIsNewModule, stored right after the ModuleSingleBuffered::Construct(this+295896)
    // that cleared it. mbIsNewModule is `protected` on CgsModule::Module, so the owner needs this
    // one-line hatch to set it from outside. (Measured cost of omitting it: the base Prepare takes
    // its CreateInputDataStructure arm and asserts "This is a new module type" every frame,
    // for ever.)
    void MarkAsNewModule() { mbIsNewModule = true; }

private:
    // X360 0x8278C2E0 (DWARF BrnRouteMapModule.cpp:171). One A* step for the race route: on a
    // fresh request Prepare the search + push the block list, then Compute one iteration; when
    // the search is no longer in progress build the Route into a RouteResponse and post it.
    // lpRouteRequest is NULL on the continuation frames of a multi-frame search.
    void ProcessRaceRoute(const RouteMapModuleIO::RaceRouteRequest* lpRouteRequest,
                          RouteMapModuleIO::RouteResponseQueue* lpRouteResponseQueue);

    // X360 0x8278C4C8 (DWARF BrnRouteMapModule.cpp:231). Backwards (6) + forwards/twisty (8)
    // section extrapolation through the RacingLineGenerator, one portal-position node per
    // section, posted as a COMPLETE RouteResponse. See the .cpp for the PARK.
    void ProcessExtrapolatedRoute(const RouteMapModuleIO::ExtrapolatedRouteRequest* lpRouteRequest,
                                  RouteMapModuleIO::RouteResponseQueue* lpRouteResponseQueue);

    EA::Thread::RWMutex mReadWriteMutexA;   // guest index 4
    EA::Thread::RWMutex mReadWriteMutexB;   // guest index 70

    // X360 +552 (0x228). The route pathfinder AStar::Construct binds the section graph into.
    AStar               mAStar;

    // X360 +26016 (0x65A0). The loaded AI road network.
    CgsResource::ResourcePtr<AISectionsData> mAISectionsData;

    u8                  mWorkingSetPad[25472]; // unknown embedded span up to the anchor

    // Trailing intrusive-list anchor (circular list head: the three node
    // pointers are initialised to the anchor itself when empty).
    u32   mAnchorState;     // guest index 6504
    u32   mUnk6505;
    u32   mUnk6506;
    void* mpListHead;       // guest index 6507
    void* mpListTail;       // guest index 6508
    void* mpListCursor;     // guest index 6509
    u32   mUnk6510;
    u32   mUnk6511;
    u32   mUnk6512;
    void* mpAllocatorIface; // guest index 6513 -> static dispatch table

    // ---- ADDITIVE (aiwave A5, 2026-09-03; DWARF BrnRouteMapModule.h:111/:112) ------------
    // The ids of the race route currently being searched, latched by ProcessRaceRoute on the
    // frame the request arrives (`sth 0x6A(req), 0x65C0(this)` / `sth 0x68(req), 0x65C2(this)`)
    // and copied into the RouteResponse on the frame the search completes -- the request itself
    // is gone by then (the transient "Route" input buffer is rebuilt every frame). X360 +26048 /
    // +26050; on this host they live here, past the pad spine, and are reached by name only.
    u16   muEventId;        // X360 +0x65C0 (26048)
    u16   muOwnerId;        // X360 +0x65C2 (26050)
};
}

#endif
