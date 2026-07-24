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
//     VariableEventQueue<512,16>) at +0x1718, immediately after the
//     EventQueue<GetZoneResponse,8> sub-object: base 16 + 8*736 = 0x1710,
//     rounded up to the 16-aligned 0x1718.
// 736 == 46*16, and EventQueue<...,8>::Construct @ 0x822E51B0 lays the inline
// maEvents[8] at the 16-aligned +0x10 -- so GetZoneResponse is 16-aligned (it
// carries a 16-aligned vector/matrix payload).
//
// FLAG (opaque payload): no getter for GetZoneResponse was recovered in this
// slice and there is no DWARF for the type, so its 736 internal bytes are NOT
// individually named. They are modelled as one correctly-sized, 16-aligned
// opaque byte span so the queue element stride (736) and the owning buffer's
// member offsets are byte-exact. Grow this ADDITIVELY into named fields when a
// later PVS-module TU (a GetZoneResponse accessor) attests them. Nothing here is
// fabricated as X360 fact -- only the size/alignment are pinned by asm.
struct alignas(16) GetZoneResponse
{
    // INTERIOR (X360 offsets; attested 2026-07-24 by the WorldEntityModule TU:
    // UpdateStream @0x822F9740, QueryWorldGraphicsLoad @0x822A87B0,
    // PostPhysicsUpdate @0x823080F0, Prepare @0x823027D0 mPlayerZoneResponse
    // .Construct(-1,0,-1)):
    //   +0x000 miLookupIndex   +0x004 miNumZones   +0x008 maZones[48]{ptr,zone}
    //   +0x1E8 mabZoneRequired[48]   +0x218 mafZoneWeights[48]   +0x2D8 miPlayerZone
    // The zone-record pointer is a host pointer on PC (the PVS module resolves it
    // into the loaded PVS data), so the x64 layout departs from the 736-byte X360
    // form; the queue element stride follows sizeof (semantic-by-NAME, x64 gate).
    static const s32 KI_MAX_ZONES = 48;

    // One visible zone: the PVS zone record + its zone number. The record fields
    // this slice reads are serialised PVS data (external format): the safe flag
    // at record-96 and the has-prop-instances count at record+0.
    struct ZoneEntry
    {
        const u8* mpZoneData;     // X360 +0: 32-bit pointer into the PVS data
        u32       muZoneNumber;   // X360 +4
    };

    void Construct( s32 liLookupIndex, s32 liNumZones, s32 liPlayerZone )
    {
        miLookupIndex = liLookupIndex;
        miNumZones = liNumZones;
        miPlayerZone = liPlayerZone;
    }

    s32  GetLookupIndex() const               { return miLookupIndex; }
    s32  GetNumZones() const                  { return miNumZones; }
    s32  GetZoneNumber( u32 luZone ) const
    {
        CGS_ASSERT( luZone < static_cast<u32>( KI_MAX_ZONES ), "zone index out of range" );
        return static_cast<s32>( maZones[ luZone ].muZoneNumber );
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
    // Serialised-PVS-record reads (raw offsets into external data; see above).
    bool IsZoneSafe( u32 luZone ) const
    {
        CGS_ASSERT( luZone < static_cast<u32>( KI_MAX_ZONES ), "zone index out of range" );
        return *reinterpret_cast<const u32*>( maZones[ luZone ].mpZoneData - 96 ) != 0;
    }
    bool ZoneHasPropInstances( u32 luZone ) const
    {
        CGS_ASSERT( luZone < static_cast<u32>( KI_MAX_ZONES ), "zone index out of range" );
        return *reinterpret_cast<const u32*>( maZones[ luZone ].mpZoneData ) != 0;
    }

    s32       miLookupIndex;                  // X360 +0x000
    s32       miNumZones;                     // X360 +0x004
    ZoneEntry maZones[KI_MAX_ZONES];          // X360 +0x008 (8B stride on X360)
    u8        maPad188[0x60];                 // X360 +0x188 (unattested span)
    u8        mabZoneRequired[KI_MAX_ZONES];  // X360 +0x1E8
    f32       mafZoneWeights[KI_MAX_ZONES];   // X360 +0x218
    s32       miPlayerZone;                   // X360 +0x2D8
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

// ============================================================================
// BrnWorld::PVSIO::OutputBuffer -- the PVS module's single-buffered OUTPUT data
// structure (the TOutputBuffer template argument of the module's base).
//
// LAYOUT (X360 authoritative):
//   +0x0000  EventQueue<GetZoneResponse, 8> mZoneResponseQueue
//            The per-zone visibility replies the PVS Update produces. The
//            EventQueue_GetZoneResponse_8 instance TU pins maEvents at the
//            16-aligned +0x10, so this sub-object spans 0x10 + 8*736 == 0x1710,
//            rounded up to the 16-aligned 0x1718.
//   +0x1718  RequestInterface<512> mGameDataRequestInterface
//            The game-data request interface the module fills to stream PVS data
//            (LoadPVS, type 26). BrnWorld::PVSModule::GetGameDataRequestInt
//            @ 0x822BB068 returns its address (asm: `addi r3, OutputStructure,
//            0x1718`). sizeof(RequestInterface<512>) == 512 + 16 == 528.
//
// The +0x1718 placement is the SAME constant the EventQueue_GetZoneResponse_8 TU
// already documents (0x10 + 8*736 == 0x1710, 16-aligned to 0x1718); pinned by
// OutputBuffer::Construct @ 0x822EE658 and re-attested by GetGameDataRequestInt.
//
// FLAG (slice): only these two X360-attested members are modelled. The 16-byte
// EventQueue base (mpEvents/miMaxLength/miLength, 12 bytes) is alignment-padded to
// 16 because GetZoneResponse is 16-aligned, so mGameDataRequestInterface naturally
// lands at +0x1718. Grow ADDITIVELY if a later PVS-module TU attests further
// output-buffer members.
struct alignas(16) OutputBuffer : public CgsModule::DataStructure
{
    void Construct()
    {
        mZoneResponseQueue.Construct();
        // mGameDataRequestInterface (a VariableEventQueue<512,16>) Constructs via its
        // own per-instance hook; OutputBuffer::Construct drives both.
    }
    // X360 0x822EE6A0 -- real body (clears the zone-response queue + the game-data request
    // interface, returns true). De-inlined; body in BrnPVSModuleIO.cpp.
    bool Prepare();
    // X360 0x822EE6D8 -- real body (clears the zone-response queue + the game-data request
    // interface, returns true). De-inlined; body in BrnPVSModuleIO.cpp.
    bool Release();
    // X360 0x822EE710 -- real body (clears the zone-response queue + the game-data request
    // interface; tail-calls VariableEventQueue<512,16>::Clear). De-inlined; body in BrnPVSModuleIO.cpp.
    void Destruct();
    void Clear()    {}

    CgsModule::EventQueue<GetZoneResponse, 8>*    GetZoneResponseQueue() { return &mZoneResponseQueue; }
    const CgsModule::EventQueue<GetZoneResponse, 8>* GetZoneResponseQueue() const { return &mZoneResponseQueue; }

    // WorldEntityModule::GetTotalZones @ 0x822BB0E0 reads this under the module's
    // output lock (X360: output-structure + 0x1710, between the queue and the
    // game-data request interface).
    u32 GetTotalZones() const { return muTotalZones; }

    CgsModule::EventQueue<GetZoneResponse, 8>     mZoneResponseQueue;          // X360 +0x0000
    u32                                           muTotalZones;                // X360 +0x1710
    BrnResource::GameDataIO::RequestInterface<512> mGameDataRequestInterface;  // X360 +0x1718

    // Pin the X360-attested layout the two accessors depend on.
    static void _AssertLayout();
};
}
}

#endif // BRN_PVS_MODULE_EVENTS_H
