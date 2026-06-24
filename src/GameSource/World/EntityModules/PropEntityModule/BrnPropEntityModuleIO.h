// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h
//
// Canonical (DWARF) home for the BrnWorld::PropEntityIO prop-entity IO types
// (BrnPropEntityModuleIO.h). This is a MINIMAL-COMPLETE slice: it currently homes only
// PropVFXLocatorEvent -- the element type of the EventQueue<PropVFXLocatorEvent,10>
// instantiation whose Construct the X360 emitted out-of-line (0x8228DB20). The many
// other PropEntityIO types/buffers in the DWARF (OutputBuffer_*, the other event
// structs, the queue typedefs) are intentionally NOT reproduced here yet; their own
// TUs grow this header ADDITIVELY when they land.
//
// Member names/types and the EEventType enumerators are from the DecFIGS DWARF
// (BrnPropEntityModuleIO.h:228/268..270), X360-gated. Layout (DWARF-faithful):
//   +0   Matrix44Affine mTransform     (rw::math::vpu::Matrix44Affine, 64B, 16-aligned)
//   +64  u32            muTypeId
//   +68  EEventType     meEventType    (s32)
// => sizeof 80 (16-aligned because Matrix44Affine forces alignas(16)); the event has
// no trailing pad beyond natural alignment. The owning EventQueue<...,10> places its
// inline maEvents[10] at the 16-aligned offset +16 inside the queue (mpEvents@0,
// miMaxLength@4, miLength@8, 4B pad, maEvents@16) -- matching X360 Construct's
// `lpEventBuffer = this + 0x10` and the stores mpEvents=&maEvents, miMaxLength=10,
// miLength=0.
#pragma once

#include "types.hpp"          // u32, s32
#include "BrnCommonTypes.h"   // Matrix44Affine (rw::math::vpu::Matrix44Affine)
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer

namespace BrnWorld
{
namespace PropEntityIO
{
    // BrnPropEntityModuleIO.h:66 -- the PropVFXLocator output queue capacity.
    const u32 KU_PROP_VFX_QUEUE_SIZE = 10;

    // BrnPropEntityModuleIO.h:225 -- a VFX-locator hit/smash event published per prop
    // impact: the prop's affine world transform, its type id, and which event fired.
    struct PropVFXLocatorEvent
    {
        // BrnPropEntityModuleIO.h:228
        enum EEventType : s32
        {
            E_EVENTTYPE_PROPHIT  = 0,
            E_EVENTTYPE_PROPSMASH = 1,
            E_EVENTTYPE_MAX      = 2
        };

        // BrnPropEntityModuleIO.h:242 -- own TU (declared-only here).
        void Construct(Matrix44Affine lTransform, u32 luTypeId, EEventType leEventType);
        // BrnPropEntityModuleIO.h:250 -- own TU.
        const Matrix44Affine& GetTransform() const;
        // BrnPropEntityModuleIO.h:256 -- own TU.
        u32 GetPropType() const;
        // BrnPropEntityModuleIO.h:262 -- own TU.
        EEventType GetEventType() const;

    private:
        Matrix44Affine mTransform;   // :268
        u32            muTypeId;     // :269
        EEventType     meEventType;  // :270
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::BrokenPropEvent -- a "this prop broke" notification
    // published by the prop entity module's post-physics output buffer and drained
    // by BrnWorld::PropEntityModule::ProcessBrokenProps.
    //
    // SIZE (X360, authoritative): sizeof == 1. Pinned by
    // BaseEventQueue<BrokenPropEvent>::AddEvent @ 0x822C9838, which copies the event
    // with `lbz r10, 0(src); stbx r10, miLength, mpEvents` -- a single byte at
    // stride 1 (no index scaling). EventQueue<BrokenPropEvent,50>::Construct
    // @ 0x822E5060 places the inline maEvents[50] at +0xC (base mpEvents@0,
    // miMaxLength@4, miLength@8, maEvents@12 -- no alignment pad, confirming the
    // element's alignment is 1), with miMaxLength = 50 (0x32).
    //
    // The single byte is the broken prop's index/id (the consumer's asserts speak of
    // "luPartIndex < (1U << KU_NUM_BITS_FOR_PART...)"). Named muPropIndex; this is the
    // only field and the type is exactly one byte wide as the asm attests.
    struct BrokenPropEvent
    {
        u8 muPropIndex;   // +0x00  broken prop index (single-byte payload)
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::TrafficLightKnockDownEvent -- a "this traffic light has been
    // knocked down" notification queued by the prop entity module's pre-physics input/output
    // buffers (the producers OutputBuffer_PrePhysics::Construct / BrnTrafficIO::
    // InputBuffer_PrePhysics::Construct build EventQueue<TrafficLightKnockDownEvent,32>).
    //
    // SIZE (X360, authoritative): sizeof == 4. Pinned by
    // BaseEventQueue<TrafficLightKnockDownEvent>::AddEvent @ 0x822C8D78, which copies the event
    // with `slwi r11,miLength,2; stwx r10,r11,mpEvents` -- a single 4-byte store at stride 4
    // (no two-half split, no index scaling beyond *4). EventQueue<...,32>::Construct
    // @ 0x822E4CE0 places the inline maEvents[32] at +0xC (base mpEvents@0, miMaxLength@4,
    // miLength@8, maEvents@12 -- no alignment pad, confirming 4-byte element alignment) with
    // miMaxLength = 32 (0x20).
    //
    // FLAG (opaque interior): the 4-byte payload's internal field layout is not recovered by
    // this slice -- every observed body (the queue Construct/AddEvent/Append) treats the
    // element only as a 4-byte blob (single stwx / 4*count XMemCpy). Modelled as exactly one
    // 4-byte word so the asm-attested stride and the +0xC inline-buffer offset are exact; the
    // interior (likely a traffic-light/junction id or packed index) is honestly opaque.
    struct TrafficLightKnockDownEvent
    {
        u32 muPayload;   // +0x00  4-byte payload (interior opaque -- see FLAG)
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::TrafficLightRestoreEvent -- the paired "restore this knocked-down
    // traffic light" notification, queued alongside TrafficLightKnockDownEvent by the same
    // producers (EventQueue<TrafficLightRestoreEvent,80>).
    //
    // SIZE (X360, authoritative): sizeof == 4. Pinned by
    // BaseEventQueue<TrafficLightRestoreEvent>::AddEvent (@ 0x822C8F.., `slwi r11,miLength,2;
    // stwx r10,r11,mpEvents`) and ::Append (XMemCpy with `slwi count,miLength,2` == 4*count) --
    // a single 4-byte element at stride 4. EventQueue<...,80>::Construct @ 0x822E4D50 places
    // the inline maEvents[80] at +0xC (base mpEvents@0, miMaxLength@4, miLength@8, maEvents@12 --
    // no alignment pad, confirming 4-byte element alignment) with miMaxLength = 80 (0x50).
    //
    // FLAG (opaque interior): as TrafficLightKnockDownEvent above -- the 4-byte payload's
    // internal field layout is not recovered by this slice; modelled as one 4-byte word so the
    // asm-attested stride and +0xC inline-buffer offset are exact.
    struct TrafficLightRestoreEvent
    {
        u32 muPayload;   // +0x00  4-byte payload (interior opaque -- see FLAG)
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::OutputBuffer_PreScene (DWARF BrnPropEntityModuleIO.h:676).
    // ADDITIVE GROW: this slice homes ONLY the IO-OutputBuffers group's X360-emitted
    // accessors of the pre-scene output buffer:
    //   GetResourceRequestInterface()    @ 0x822B9888  write-lock (bit 3) -> +4      (asm-line 648)
    //   GetPropInputInterface() const    @ 0x827A1970  read-lock  (bit 4) -> +819824 (asm-line 642)
    //   GetPropInputInterface()          @ 0x822B97E0  write-lock (bit 3) -> +819824 (asm-line 645)
    //
    // LAYOUT (DWARF :676 member order + X360 getter return-offsets, authoritative):
    //   base  CgsModule::IOBuffer                            (1-byte status; +1..+3 pad)
    //   +4       ResourceRequestInterface mResourceRequestInterface (RequestInterface<1024>) :647
    //   +...     SceneInputInterface      mSceneInputInterface      (InSceneUpdateInterface)  :648
    //   +819824  PropInputInterface       mPropInputInterface       (PropInputInterface)      :649
    //   +...     VisibleOverheadSignArray mVisibleOverheadSignArray                           :650
    //
    // FLAG (foreign types): the three interface members and the overhead-sign array have
    // their own owning homes elsewhere and are NOT reconstructed here; the region between
    // the +4 member start and the +819824 member start is modelled as correctly-sized
    // opaque storage so the two X360-pinned return offsets (+4, +819824) are exact. The
    // intervening mSceneInputInterface byte split is not separately recoverable from this
    // DWARF, so it is folded into the padding (named in the comment above). Adopt the
    // named interface types additively when their homes land.
    class OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storages (see FLAG above).
        struct ResourceRequestInterfaceStorage { unsigned char maBytes[1]; };
        struct PropInputInterfaceStorage       { unsigned char maBytes[1]; };

        // X360 0x822B9888: write-lock handle, returns this + 4 (mResourceRequestInterface).
        ResourceRequestInterfaceStorage* GetResourceRequestInterface();
        // X360 0x827A1970: read-lock handle, returns this + 819824 (mPropInputInterface).
        const PropInputInterfaceStorage* GetPropInputInterface() const;
        // X360 0x822B97E0: write-lock handle, returns this + 819824 (mPropInputInterface).
        PropInputInterfaceStorage* GetPropInputInterface();

        static void _AssertLayout();

    private:
        u8                              maStatusPad[3];             // +1..+3 (force +4 placement)
        ResourceRequestInterfaceStorage mResourceRequestInterface;  // +4      :647
        // mSceneInputInterface (:648) is folded into this padding (see FLAG): it spans from
        // the end of mResourceRequestInterface (+5) to the +819824 start of mPropInputInterface.
        unsigned char                   maSceneInputAndPad[819824 - 5]; // ...    :648
        PropInputInterfaceStorage       mPropInputInterface;        // +819824 :649
    };
}
}
