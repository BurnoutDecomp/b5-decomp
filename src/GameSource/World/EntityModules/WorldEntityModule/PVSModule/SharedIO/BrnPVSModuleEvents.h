#ifndef BRN_PVS_MODULE_EVENTS_H
#define BRN_PVS_MODULE_EVENTS_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (== rw::math::vpu::Vector4)
#include "GameShared/GameClasses/Module/CgsDataStructure.h"  // CgsModule::DataStructure
#include "GameShared/GameClasses/Module/CgsEventQueue.h"     // CgsModule::EventQueue<T,N>
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h" // RequestInterface<512>

// ============================================================================
// GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h
//
// The PVS (Potentially Visible Set) module's shared-IO request/response events.
// This slice covers BrnWorld::PVSIO::GetZoneRequest, the "which zone is the
// listener in" request the PVS module's Update processes.
//
// LAYOUT (from GetVelocity @ 0x822A3A48): the request carries a use-velocity flag
// (byte @ +0x24) and a velocity vector (Vector4 @ +0x30). The leading 0x24 bytes
// (the event header / position fields) are opaque in this slice.
//
// FLAG: only the two fields GetVelocity reads are named (mbUseVelocity @+0x24,
// mVelocity @+0x30); the preceding 0x24-byte header and the 11 bytes between the
// flag and the vector are modelled as opaque byte spans at the asm-observed
// offsets. Member names are inferred from the getter's behaviour and its assert
// text ("lpbOutUseVelocity is NULL"); there is no DWARF for this type. Grown
// additively when a later PVS-module TU attests the header fields.
// ============================================================================

namespace BrnWorld
{
namespace PVSIO
{
struct GetZoneRequest
{
    // GetVelocity @ 0x822A3A48. Reads back the request's velocity hint: copies the
    // velocity vector into *lpOutVelocity and the use-velocity flag into
    // *lpbOutUseVelocity. const getter.
    //
    // X360-faithful: asserts lpbOutUseVelocity != NULL (baked
    // BrnPVSModuleEvents.h:208, msg "lpbOutUseVelocity is NULL"; the baked
    // file/line is not reproduced -- CGS_ASSERT injects __FILE__/__LINE__), then
    // copies mVelocity (one lvx128/stvx128 16-byte vector move, a plain copy --
    // NOT a VMX compute pipeline) and the mbUseVelocity byte.
    void GetVelocity(Vector4* lpOutVelocity, bool* lpbOutUseVelocity) const;

    // --- layout (asm-observed offsets; header fields attested by
    //     WorldEntityModule::PreSceneUpdate @ 0x82302A08, which builds the
    //     request on the stack: player index 0, the PVS position @+0x10, the
    //     response's lookup index @+0x20, the use-velocity flag @+0x24 and the
    //     race car's velocity @+0x30) --------------------------------------
    s32     miPlayerIndex;    // +0x00  (PreSceneUpdate passes 0)
    s32     miPad04;          // +0x04  (zeroed by the builder)
    u8      maPad08[0x08];    // +0x08  opaque (deferred)
    Vector4 mPosition;        // +0x10  the position to resolve a zone for
    s32     miLookupIndex;    // +0x20  previous response's lookup hint
    u8      mbUseVelocity;    // +0x24  use-velocity flag (lbz 0x24)
    u8      maPad25[0x0B];    // +0x25  opaque padding up to the vector
    Vector4 mVelocity;        // +0x30  velocity hint (lvx128 0x30)
};

// ============================================================================
// BrnWorld::PVSIO::GetZoneResponse -- the PVS module's per-zone visibility reply.
//
// SIZE (X360, authoritative): sizeof == 736 (0x2E0). Pinned two independent ways:
//   * BaseEventQueue<GetZoneResponse>::AddEvent @ 0x822C9E28 block-copies the
//     event with `memcpy(736 * miLength + mpEvents, src, 736)` -- element stride
//     736.
//   * OutputBuffer::Construct @ 0x822EE658 places the next member (a
//     VariableEventQueue<512,16>) at +0x1718, after the
//     EventQueue<GetZoneResponse,8> sub-object: base 16 + 8*736 = 0x1710,
//     followed by the total-zone word and the aligned OutputInterface tail.
// EventQueue<...,8>::Construct @ 0x822E51B0 lays the inline maEvents at +0x10.
// DecFIGS declares u64 zone IDs and no vector payload; its natural X360 alignment is 8.
//
// INTERIOR (X360 offsets, now FULLY attested -- the whole 736-byte body is written
// field by field by BrnWorld::PVSModule::Update @0x822EE050, which settled the three
// byte arrays and the true type of the zone entries):
//   +0x000 miPlayerIndex          (echo of GetZoneRequest::miPlayerIndex)
//   +0x004 miNumZones
//   +0x008 maZoneIds[48]          (u64 -- a verbatim copy of CgsSceneManager::Zone::GetId())
//   +0x188 mabZoneSafe[48]        (1 = centre zone or reached over a SAFE edge)
//   +0x1B8 mabZoneRender[48]      (Neighbour::IsFlagSet(E_NEIGHBOURFLAG_RENDER))
//   +0x1E8 mabZoneRequired[48]    (Neighbour::IsFlagSet(E_NEIGHBOURFLAG_IMMEDIATE))
//   +0x218 mafZoneWeights[48]
//   +0x2D8 miLookupIndex          (the centre zone's INDEX -- next frame's search hint)
//
// *** CORRECTIONS this brought (all X360-proven, previous slice was inferred) ***
//  - maZones was modelled as {const u8* mpZoneData; u32 muZoneNumber}. The X360 fills
//    each 8-byte slot with ONE instruction, `ld r25,0x10(zone)` / `std r25,0(entry)` --
//    i.e. the Zone's 64-bit muZoneId, copied whole. Nothing in the PVS module ever
//    produces a pointer into "PVS data". The id's LOW 32 bits are the zone NUMBER
//    (UpdateStream @0x822F9740 does `clrlwi r3,r28,0` before MakeTrackUnitId) and the
//    whole 64-bit value is what the world graphics streamer stores as its user id
//    (AddEntry r6 = r28; IsAssetLoaded r5 = r31, PostPhysicsUpdate @0x823080F0).
//  - IsZoneSafe was modelled as `*(u32*)(mpZoneData - 96)`. That -96 is the byte
//    distance between the two ARRAYS: UpdateStream walks mabZoneRequired (r27 =
//    response+0x1E8) and reads the safe flag as `lbz r29,-0x60(r27)` == mabZoneSafe.
//  - ZoneHasPropInstances was modelled as `*(u32*)mpZoneData`. UpdateStream's prop
//    gate is `lbz r11,0(r27)` == mabZoneRequired[i]; the accessor is retired.
//  - +0x000 / +0x2D8: Update stores GetZoneRequest::miPlayerIndex into +0x000 and the
//    centre zone's index ((zone - zoneList->GetZones())) into +0x2D8, and
//    WorldEntityModule::PreSceneUpdate @0x82302A08 feeds +0x2D8 (`lwz r27,0x28F8(r31)`
//    == mPlayerZoneResponse + 0x2D8) back into the next request's miLookupIndex. So
//    +0x2D8 is the lookup index and +0x000 is the player index -- the two names were
//    swapped in the previous slice.
struct GetZoneResponse
{
    static const s32 KI_MAX_ZONES = 48;

    void Construct( s32 liPlayerIndex, s32 liNumZones, s32 liLookupIndex )
    {
        miPlayerIndex = liPlayerIndex;
        miNumZones    = liNumZones;
        miLookupIndex = liLookupIndex;
    }

    s32  GetPlayerIndex() const               { return miPlayerIndex; }
    s32  GetLookupIndex() const               { return miLookupIndex; }
    s32  GetNumZones() const                  { return miNumZones; }

    // The whole 64-bit Zone id -- the world graphics streamer's per-entry user id.
    u64  GetZoneId( u32 luZone ) const
    {
        CGS_ASSERT( luZone < static_cast<u32>( KI_MAX_ZONES ), "zone index out of range" );
        return maZoneIds[ luZone ];
    }
    // Its low half: the zone NUMBER (the TRK_UNIT index).
    s32  GetZoneNumber( u32 luZone ) const
    {
        CGS_ASSERT( luZone < static_cast<u32>( KI_MAX_ZONES ), "zone index out of range" );
        return static_cast<s32>( static_cast<u32>( maZoneIds[ luZone ] ) );
    }
    f32  GetZoneWeight( u32 luZone ) const
    {
        CGS_ASSERT( luZone < static_cast<u32>( KI_MAX_ZONES ), "zone index out of range" );
        return mafZoneWeights[ luZone ];
    }
    bool IsZoneRequired( u32 luZone ) const
    {
        CGS_ASSERT( luZone < static_cast<u32>( KI_MAX_ZONES ), "zone index out of range" );
        return mabZoneRequired[ luZone ] != 0;
    }
    bool IsZoneRender( u32 luZone ) const
    {
        CGS_ASSERT( luZone < static_cast<u32>( KI_MAX_ZONES ), "zone index out of range" );
        return mabZoneRender[ luZone ] != 0;
    }
    bool IsZoneSafe( u32 luZone ) const
    {
        CGS_ASSERT( luZone < static_cast<u32>( KI_MAX_ZONES ), "zone index out of range" );
        return mabZoneSafe[ luZone ] != 0;
    }

    s32       miPlayerIndex;                  // X360 +0x000
    s32       miNumZones;                     // X360 +0x004
    u64       maZoneIds[KI_MAX_ZONES];        // X360 +0x008
    u8        mabZoneSafe[KI_MAX_ZONES];      // X360 +0x188
    u8        mabZoneRender[KI_MAX_ZONES];    // X360 +0x1B8
    u8        mabZoneRequired[KI_MAX_ZONES];  // X360 +0x1E8
    f32       mafZoneWeights[KI_MAX_ZONES];   // X360 +0x218
    s32       miLookupIndex;                  // X360 +0x2D8
};

// ============================================================================
// BrnWorld::PVSIO::InputBuffer -- the PVS module's single-buffered INPUT data
// structure (the TInputBuffer template argument of the module's
// CgsModule::ModuleSingleBufferedTemplate<InputBuffer, OutputBuffer> base).
//
// The X360 EventQueue_GetZoneRequest_8 instance TU attests that
// BrnWorld::PVSIO::InputBuffer::Construct drives
// CgsModule::EventQueue<GetZoneRequest, 8>::Construct @ 0x822E5140 -- i.e. the
// input buffer owns one fixed-capacity GetZoneRequest queue (the "which zone is
// the listener in" requests WorldEntityModule::PreSceneUpdate enqueues and the
// PVS module's Update drains). DataStructure is the empty single-buffered base
// (CgsDataStructure.h); the queue is the only member, at offset 0.
//
// FLAG (slice): only the GetZoneRequest queue is modelled -- the only member this
// SharedIO slice has X360 evidence for. Grow ADDITIVELY if a later PVS-module TU
// attests further input-buffer members. Construct/Prepare/Release/Destruct are
// the DataStructure lifecycle hooks the ModuleSingleBufferedTemplate calls.
struct InputBuffer : public CgsModule::DataStructure
{
    void Construct() { mZoneRequestQueue.Construct(); }
    // X360 0x822D8C20 -- real body (clears the zone-request queue length, returns true).
    // De-inlined; body in BrnPVSModuleIO.cpp.
    bool Prepare();
    bool Release()   { return true; }
    // X360 0x822D8C38 -- real body (clears the zone-request queue length). De-inlined;
    // body in BrnPVSModuleIO.cpp.
    void Destruct();
    void Clear()     {}

    CgsModule::EventQueue<GetZoneRequest, 8> mZoneRequestQueue;   // +0x00
};

// DecFIGS BrnPVSModuleOutputInterface.h:74-75. The u64 zone IDs give
// the response queue eight-byte alignment on X360. Its 0x1710 bytes and the
// total-zone word make this interface 0x1718 bytes after aggregate alignment.
struct OutputInterface
{
    typedef CgsModule::EventQueue<GetZoneResponse, 8> ZoneResponseQueue;
    void Construct() { mGetZoneResponseQueue.Construct(); mGetZoneResponseQueue.Clear(); }
    void Clear() { mGetZoneResponseQueue.Clear(); }
    ZoneResponseQueue* GetZoneResponseQueue() { return &mGetZoneResponseQueue; }
    const ZoneResponseQueue* GetZoneResponseQueue() const { return &mGetZoneResponseQueue; }
    u32 GetTotalZones() const { return muTotalZones; }
    void SetTotalZones(u32 luTotalZones) { muTotalZones = luTotalZones; }

    ZoneResponseQueue mGetZoneResponseQueue;
    u32 muTotalZones;
};

// ARTIST 0x822EE658 and 0x822BB068: the output interface starts at zero,
// followed by the game-data request interface at +0x1718. The outer buffer
// remains 16-aligned; its 0x1930-byte extent anchors the following PVS stages.
struct CGS_ALIGN(16) OutputBuffer : public CgsModule::DataStructure
{
    void Construct()
    {
        mOutputInterface.Construct();
        mGameDataRequestInterface.Construct();
        Clear();
    }
    bool Prepare();
    bool Release();
    void Destruct();
    void Clear() { mOutputInterface.Clear(); mGameDataRequestInterface.Clear(); }

    OutputInterface* GetOutputInterface() { return &mOutputInterface; }
    const OutputInterface* GetOutputInterface() const { return &mOutputInterface; }
    BrnResource::GameDataIO::RequestInterface<512>* GetGameDataRequestInterface()
        { return &mGameDataRequestInterface; }
    const BrnResource::GameDataIO::RequestInterface<512>* GetGameDataRequestInterface() const
        { return &mGameDataRequestInterface; }

    OutputInterface mOutputInterface;
    BrnResource::GameDataIO::RequestInterface<512> mGameDataRequestInterface;
    static void _AssertLayout();
};
}
}

#endif // BRN_PVS_MODULE_EVENTS_H
