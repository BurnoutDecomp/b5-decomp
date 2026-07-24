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

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "types.hpp"          // u32, s32
#include "BrnCommonTypes.h"   // Matrix44Affine (rw::math::vpu::Matrix44Affine)
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsEventQueue.h"  // CgsModule::EventQueue (InputBuffer_PostPhysics::mUpdatedPropQueue)
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"  // BrnPhysics::ContactSpy::ContactSpyInterface (mContactSpyInterface, by value)
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"   // BrnPhysics::Props::UpdatePropEvent (queue element)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"  // CgsSceneManager::SceneManagerIO::PotentialContact (InputBuffer_PrePhysics::mPotentialContactQueue element)
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"           // BrnAI::AIModuleIO::AIModuleResultInterface::ResetOnTrackResultQueue (InputBuffer_PrePhysics::mResetOnTrackResultQueue)

// The replay-status payload InputBuffer_PreScene::SetReplayStatusInterface latches (pointer-only
// use; home GameSource/Replays/BrnReplayStatusInterface.h).
namespace BrnReplays { namespace ReplayIO { struct StatusInterface; } }

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
        // Opaque foreign-type storages (see FLAG above). Sized so each X360-pinned return offset
        // is exact: mResourceRequestInterface@+4, mSceneInputInterface@+1056,
        // mPropInputInterface@+819824, mVisibleOverheadSignArray@+831104 (LAST member).
        struct ResourceRequestInterfaceStorage { unsigned char maBytes[1056 - 4]; };        // +4..+1055
        struct SceneInputInterfaceStorage      { unsigned char maBytes[819824 - 1056]; };    // +1056..+819823
        struct PropInputInterfaceStorage       { unsigned char maBytes[831104 - 819824]; };  // +819824..+831103
        struct VisibleOverheadSignArrayStorage { unsigned char maBytes[1]; };                // trailing +831104

        // X360 0x827A1A18 (:640, THIS batch): read-lock handle, returns this + 4 (mResourceRequestInterface).
        const ResourceRequestInterfaceStorage* GetResourceRequestInterface() const;
        // X360 0x822B9888: write-lock handle, returns this + 4 (mResourceRequestInterface).
        ResourceRequestInterfaceStorage* GetResourceRequestInterface();
        // X360 0x827A1970: read-lock handle, returns this + 819824 (mPropInputInterface).
        const PropInputInterfaceStorage* GetPropInputInterface() const;
        // X360 0x822B97E0: write-lock handle, returns this + 819824 (mPropInputInterface).
        PropInputInterfaceStorage* GetPropInputInterface();
        // X360 0x827A1AC0 (:643, THIS batch): read-lock handle, returns this + 831104 (mVisibleOverheadSignArray).
        const VisibleOverheadSignArrayStorage* GetVisibleOverheadSignArray() const;
        // X360 0x822B9930 (:644, THIS batch): write-lock handle, returns this + 831104 (mVisibleOverheadSignArray).
        VisibleOverheadSignArrayStorage* GetVisibleOverheadSignArray();

        static void _AssertLayout();

    private:
        u8                              maStatusPad[3];             // +1..+3 (force +4 placement)
        ResourceRequestInterfaceStorage mResourceRequestInterface;  // +4      :647
        SceneInputInterfaceStorage      mSceneInputInterface;       // +1056   :648
        PropInputInterfaceStorage       mPropInputInterface;        // +819824 :649
        VisibleOverheadSignArrayStorage mVisibleOverheadSignArray;  // +831104 :650 (LAST)
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::OutputBuffer_Prepare (DWARF BrnPropEntityModuleIO.h:588).
    // ADDITIVE GROW: homes the IO-OutputBuffers group's X360-emitted accessors of the
    // prepare-phase output buffer. The three the X360 emitted out-of-line for this slice:
    //   GetResourceRequestInterface()  @ 0x822B9690  write-lock (bit 3) -> +4      (asm-line 613)
    //   GetPropInputInterface() const  @ 0x827A1778  read-lock  (bit 4) -> +819824 (asm-line 607)
    //   GetPropInputInterface()        @ 0x822B95E8  write-lock (bit 3) -> +819824 (asm-line 610)
    //
    // LAYOUT (DWARF :588 member order + X360 getter return-offsets, authoritative; identical
    // shape to OutputBuffer_PreScene):
    //   base     CgsModule::IOBuffer                                       (1-byte status; +1..+3 pad)
    //   +4       ResourceRequestInterface mResourceRequestInterface (RequestInterface<1024>) :609
    //   +...     SceneInputInterface      mSceneInputInterface      (InSceneUpdateInterface)  :610
    //   +819824  PropInputInterface       mPropInputInterface       (PropInputInterface)      :611
    //
    // FLAG (foreign types): the three interface members have their own owning homes elsewhere
    // and are NOT reconstructed here; the region between the +4 member start and the +819824
    // member start is modelled as correctly-sized opaque storage so the two X360-pinned return
    // offsets (+4, +819824) are exact (same folding as OutputBuffer_PreScene). Adopt the named
    // interface types additively when their homes land.
    class OutputBuffer_Prepare : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storages (see FLAG above). Sized so each X360-pinned return offset
        // is exact: mResourceRequestInterface@+4, mSceneInputInterface@+1056, mPropInputInterface@+819824.
        struct ResourceRequestInterfaceStorage { unsigned char maBytes[1056 - 4]; };     // +4..+1055
        struct SceneInputInterfaceStorage      { unsigned char maBytes[819824 - 1056]; }; // +1056..+819823
        struct PropInputInterfaceStorage       { unsigned char maBytes[1]; };             // trailing +819824

        // X360 0x827A1820 (:605, THIS batch): read-lock handle, returns this + 4 (mResourceRequestInterface).
        const ResourceRequestInterfaceStorage* GetResourceRequestInterface() const;
        // X360 0x822B9690: write-lock handle, returns this + 4 (mResourceRequestInterface).
        ResourceRequestInterfaceStorage* GetResourceRequestInterface();
        // X360 0x827A16D0 (:599, THIS batch): read-lock handle, returns this + 1056 (mSceneInputInterface).
        const SceneInputInterfaceStorage* GetSceneInputInterface() const;
        // X360 0x822B9540 (:602, THIS batch): write-lock handle, returns this + 1056 (mSceneInputInterface).
        SceneInputInterfaceStorage* GetSceneInputInterface();
        // X360 0x827A1778: read-lock handle, returns this + 819824 (mPropInputInterface).
        const PropInputInterfaceStorage* GetPropInputInterface() const;
        // X360 0x822B95E8: write-lock handle, returns this + 819824 (mPropInputInterface).
        PropInputInterfaceStorage* GetPropInputInterface();

        static void _AssertLayout();

    private:
        u8                              maStatusPad[3];             // +1..+3 (force +4 placement)
        ResourceRequestInterfaceStorage mResourceRequestInterface;  // +4      :609
        SceneInputInterfaceStorage      mSceneInputInterface;       // +1056   :610
        PropInputInterfaceStorage       mPropInputInterface;        // +819824 :611
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::OutputBuffer_PostPhysics (DWARF BrnPropEntityModuleIO.h:714).
    // ADDITIVE GROW: homes the IO-OutputBuffers group's X360-emitted accessors of the
    // post-physics output buffer. The three the X360 emitted out-of-line for this slice:
    //   GetPropInputInterface() const  @ 0x827A2000  read-lock  (bit 4) -> +833008 (asm-line 739)
    //   GetSceneInputInterface()       @ 0x822B9BD0  write-lock (bit 3) -> +820912 (asm-line 742)
    //   GetPropInputInterface()        @ 0x822B9FC0  write-lock (bit 3) -> +833008 (asm-line 748)
    // (The const+non-const GetPropInputInterface pair both return the same +833008 member; the
    // write-lock GetSceneInputInterface returns the lower +820912 member -- DWARF :752/:753 place
    // mSceneInputInterface before mPropInputInterface, so Scene is the lower offset. The Hex-Rays
    // recovered names were truncated; the lock bit + return offset + DWARF member order pin them.)
    //
    // LAYOUT (DWARF :714 member order + X360 getter return-offsets, authoritative):
    //   base     CgsModule::IOBuffer  (1-byte status)
    //   ...      mPropBecamePhysicalEventQueue / mRecordHitPropQueue / mHitOverheadSignQueue /
    //            mBrokenPropQueue  (the leading event-queue members; :748-:751)
    //   +820912  SceneInputInterface mSceneInputInterface  (InSceneUpdateInterface)  :752
    //   +833008  PropInputInterface  mPropInputInterface   (PropInputInterface)      :753
    //   ...      mPropVFXLocatorQueue (:754) / mbShouldRequestProgression (:755)
    //
    // FLAG (foreign types / opaque interior): only the two interface members are pinned by this
    // slice's X360 getters (return offsets +820912 / +833008). The leading event queues, the
    // trailing VFX queue, and the trailing bool are NOT reconstructed here; the storage up to the
    // +820912 SceneInputInterface start and the SceneInputInterface span up to +833008 are modelled
    // as correctly-sized opaque storage so the two pinned return offsets are exact. The interface
    // members and queues have their own homes; adopt them additively when those land.
    class OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storages (see FLAG above).
        struct HitOverheadSignQueueStorage { unsigned char maBytes[1]; }; // +2144 (span carved by maToScene)
        struct SceneInputInterfaceStorage  { unsigned char maBytes[1]; };
        struct PropInputInterfaceStorage   { unsigned char maBytes[1]; };
        // Leading event-queue members now carved to their X360-attested offsets (:748-:751).
        struct PropBecamePhysicalEventQueueStorage { unsigned char maBytes[1]; };  // +16      :747
        struct RecordHitPropQueueStorage           { unsigned char maBytes[1]; };  // +0x160   :748 (placeholder name)
        struct BrokenPropQueueStorage              { unsigned char maBytes[1]; };  // +2080    :744
        struct PropVFXLocatorQueueStorage          { unsigned char maBytes[1]; };  // +832192  :745

        // X360 0x827A2000: read-lock handle, returns this + 833008 (mPropInputInterface).
        const PropInputInterfaceStorage* GetPropInputInterface() const;
        // X360 0x822B9B28 (:735, THIS batch): write-lock handle, returns this + 2144 (mHitOverheadSignQueue).
        HitOverheadSignQueueStorage* GetHitOverheadSignQueue();
        // X360 0x822B9BD0: write-lock handle, returns this + 820912 (mSceneInputInterface).
        SceneInputInterfaceStorage* GetSceneInputInterface();
        // X360 0x822B9FC0: write-lock handle, returns this + 833008 (mPropInputInterface).
        PropInputInterfaceStorage* GetPropInputInterface();
        // X360 0x822B9F18 (:747): write-lock; return &mPropBecamePhysicalEventQueue (this + 16).
        PropBecamePhysicalEventQueueStorage* GetPropBecamePhysicalEventQueue();
        // X360 0x822B9E70 (:746): write-lock; return &mRecordHitPropQueue (this + 0x160). (placeholder name)
        RecordHitPropQueueStorage* GetRecordHitPropQueue();
        // X360 0x822B9D20 (:744): write-lock; return &mBrokenPropQueue (this + 2080).
        BrokenPropQueueStorage* GetBrokenPropQueue();
        // X360 0x822B9DC8 (:745): write-lock; return &mPropVFXLocatorQueue (this + 832192).
        PropVFXLocatorQueueStorage* GetPropVFXLocatorQueue();

        static void _AssertLayout();

    private:
        // Leading event-queue members carved to their X360-attested offsets (all opaque
        // foreign-type storage -> the byte offsets are safe to pin). IOBuffer status byte at +0.
        unsigned char                       maStatusPad[16 - 1];               // +1..+15
        PropBecamePhysicalEventQueueStorage mPropBecamePhysicalEventQueue;     // +16      :747
        unsigned char                       maToRecordHit[0x160 - 16 - 1];     // +17..+0x15F
        RecordHitPropQueueStorage           mRecordHitPropQueue;               // +0x160   :748 (placeholder)
        unsigned char                       maToBrokenProp[2080 - 0x160 - 1];  // +0x161..+2079
        BrokenPropQueueStorage              mBrokenPropQueue;                  // +2080    :744
        unsigned char                       maToHitOverhead[2144 - 2080 - 1];  // +2081..+2143
        // mHitOverheadSignQueue (:750, EventQueue<HitOverheadSignEvent,100>) at +2144 (opaque).
        HitOverheadSignQueueStorage         mHitOverheadSignQueue;             // +2144   :750
        unsigned char                       maToScene[820912 - 2144 - 1];      // +2145..+820911
        SceneInputInterfaceStorage          mSceneInputInterface;              // +820912 :752
        unsigned char                       maSceneToVFX[832192 - 820912 - 1]; // +820913..+832191
        PropVFXLocatorQueueStorage          mPropVFXLocatorQueue;              // +832192 :745
        unsigned char                       maVFXToProp[833008 - 832192 - 1];  // +832193..+833007
        PropInputInterfaceStorage           mPropInputInterface;               // +833008 :753
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::OutputBuffer_PrePhysics (DWARF BrnPropEntityModuleIO.h:683).
    // ADDITIVE GROW: the pre-physics OUTPUT buffer the prop module fills for the traffic bridge.
    // DWARF: mPropInputInterface(:702)@+4, mPropToTrafficInterface(:703)@+11296
    // (PropToTrafficInterface, foreign/opaque). X360 accessors emitted for this slice:
    //   GetPropInputInterface() const   @ 0x827A1B68  read-lock  (bit 4) -> +4      (own TU)
    //   GetPropInputInterface()         @ 0x822B99D8  write-lock (bit 3) -> +4      (own TU)
    //   GetPropToTrafficInterface() const @ 0x827A1C10 read-lock (bit 4) -> +11296  (THIS batch)
    //   GetPropToTrafficInterface()     @ 0x822B9A80  write-lock (bit 3) -> +11296  (THIS batch)
    //
    // FLAG (foreign types / opaque interior): both members have their own owning homes elsewhere;
    // modelled as correctly-sized opaque storage so the two X360-pinned return offsets (+4, +11296)
    // are exact. Adopt PropInputInterface / PropToTrafficInterface named types additively when they land.
    class OutputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storages (see FLAG above).
        struct PropInputInterfaceStorage     { unsigned char maBytes[11296 - 4]; }; // +4..+11295
        struct PropToTrafficInterfaceStorage { unsigned char maBytes[1]; };         // trailing +11296

        // X360 0x827A1B68 (own TU): read-lock handle, returns this + 4 (mPropInputInterface).
        const PropInputInterfaceStorage* GetPropInputInterface() const;
        // X360 0x822B99D8 (own TU): write-lock handle, returns this + 4 (mPropInputInterface).
        PropInputInterfaceStorage* GetPropInputInterface();
        // X360 0x827A1C10 (:698, THIS batch): read-lock handle, returns this + 11296 (mPropToTrafficInterface).
        const PropToTrafficInterfaceStorage* GetPropToTrafficInterface() const;
        // X360 0x822B9A80 (:699, THIS batch): write-lock handle, returns this + 11296 (mPropToTrafficInterface).
        PropToTrafficInterfaceStorage* GetPropToTrafficInterface();

        static void _AssertLayout();

    private:
        u8                            maStatusPad[3];            // +1..+3 (force +4 placement)
        PropInputInterfaceStorage     mPropInputInterface;       // +4      :702
        PropToTrafficInterfaceStorage mPropToTrafficInterface;   // +11296  :703
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::InputBuffer_PreScene
    // (ADDITIVE GROW: WorldBridgeInputToEntityModules TU)
    // ========================================================================
    // MINIMAL SLICE for the pre-scene INPUT buffer WorldModule::BridgeInputToEntityModules
    // @0x827ADF88 fills:
    //   * SetReplayStatusInterface -- real out-of-line X360 symbol (its own ledger function;
    //     declaration-only). The payload type is the committed BrnReplays::ReplayIO::
    //     StatusInterface (pointer-only use here).
    //   * the game-action-derived block @ +0x780..+0x792. Setter NAMES + semantics are the
    //     PS3 DecFIGS DWARF's (BrnWorldBridgesUnity.cpp dump: SetIsOnline / SetEasySmashProps /
    //     SetPropProgressionEnabled / SendingPropProgression / SetHitPropsBitArray /
    //     SetCurrentTimestep / ResetProps -- real PS3 symbols); the X360 bridge INLINES them
    //     (direct stb/stw/stfs), which pins the member offsets below. Modelled as X360-style
    //     header-inlines over offset-pinned members.
    // FLAG: muHitPropsBitArray is a 32-bit console word (likely a BitArray handle/pointer on
    // the 32-bit targets); kept as the raw copied word -- resolve when the prop TU lands.
    // The interior between the IOBuffer status byte and +0x780 (the replay-status payload
    // etc.) is opaque pad owned by the buffer's own TUs.
    class InputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        void SetReplayStatusInterface(const BrnReplays::ReplayIO::StatusInterface* lpReplayStatusInterface);

        // X360 header-inlines (inlined into BridgeInputToEntityModules @0x827ADF88).
        void SetHitPropsBitArray(u32 luHitPropsBitArray)          { muHitPropsBitArray = luHitPropsBitArray; mbHitPropsBitArrayValid = true; }
        void SetCurrentTimestep(f32 lfTimeStep)                   { mfCurrentTimestep = lfTimeStep; }
        void SetIsOnline(bool lbIsOnline)                         { mbIsOnline = lbIsOnline; }
        void SetEasySmashProps(bool lbEasySmashProps)             { mbEasySmashProps = lbEasySmashProps; }
        void SetPropProgressionEnabled(bool lbEnabled)            { mbPropProgressionEnabled = lbEnabled; }
        void SendingPropProgression()                             { mbSendingPropProgression = true; }
        void ResetProps()                                         { mbResetProps = true; }

    private:
        u8   maPad[0x77F];                     // +0x001..+0x77F (payload owned by own TUs)
        u32  muHitPropsBitArray;               // +0x780  (SetHitPropsBitArray; see FLAG)
        f32  mfCurrentTimestep;                // +0x784  (SetCurrentTimestep: sim timestep * multiplier)
        u8   maPad788[5];                      // +0x788..+0x78C
        bool mbIsOnline;                       // +0x78D  (SetIsOnline: game mode is an online mode)
        bool mbEasySmashProps;                 // +0x78E  (SetEasySmashProps)
        bool mbPropProgressionEnabled;         // +0x78F  (SetPropProgressionEnabled)
        bool mbSendingPropProgression;         // +0x790  (SendingPropProgression latch)
        bool mbHitPropsBitArrayValid;          // +0x791  (set alongside muHitPropsBitArray)
        bool mbResetProps;                     // +0x792  (ResetProps latch)
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::InputBuffer_Dispatch (DWARF BrnPropEntityModuleIO.h:~290).
    // ADDITIVE GROW: homes the six scalar accessors the X360 emitted out-of-line for the
    // prop-entity module's generate-dispatch-lists input buffer (the buffer the world module's
    // BridgeWorldModuleToEntityModules_Render fills and BrnWorld::PropEntityModule::
    // GenerateDispatchLists drains):
    //   GetDispatchFrame() const          @ 0x822B8EA8  read-lock  (bit 4) -> u32 *(this+4)      (asm-line 295)
    //   SetDispatchFrame(u32)             @ 0x827A1180  write-lock (bit 3) ->     *(this+4) = v   (asm-line 296)
    //   GetShadowMap() const              @ 0x822B8F50  read-lock  (bit 4) -> u32 *(this+8)      (asm-line 301)
    //   SetShadowMap(u32)                 @ 0x827A1228  write-lock (bit 3) ->     *(this+8) = v   (asm-line 302)
    //   GetCoronaSubmissionInterface()    @ 0x822B8FF8  read-lock  (bit 4) -> u32 *(this+0x801C) (asm-line 304)
    //   SetCoronaSubmissionInterface(u32) @ 0x827A12D0  write-lock (bit 3) ->     *(this+0x801C)=v(asm-line 305)
    //
    // The getters test the read-lock bit (`lbz r11,0(this); extrwi r11,r11,1,27` == bit 4 ==
    // IsBufferLockedForReading()) and fire "Not locked for reading\n"; the setters test the
    // write-lock bit (`extrwi r11,r11,1,28` == bit 3 == IsBufferLockedForWriting()) and fire
    // "Not locked for writing\n". All three accessed fields are 32-bit words: GetDispatchFrame
    // `lwz r3,4(this)`, GetShadowMap `lwz r3,8(this)`, GetCoronaSubmissionInterface
    // `ori r11,0,0x801C; lwzx r3,this,r11`; the setters mirror them (`stw 4`, `stw 8`,
    // `stwx ...,0x801C`).
    //
    // LAYOUT (X360 accessor offsets, authoritative):
    //   base    CgsModule::IOBuffer       (1-byte status; +1..+3 pad)
    //   +4      u32 muDispatchFrame       dispatch-frame index
    //   +8      u32 muShadowMap           shadow-map handle
    //   +...    (intervening dispatch-list payload region; not recovered by this slice)
    //   +0x801C u32 muCoronaSubmissionInterface   corona-submission interface handle
    //
    // FLAG (foreign type / opaque interior): the region between muShadowMap (+8) and
    // muCoronaSubmissionInterface (+0x801C) is the buffer's other dispatch-list payload (its
    // own members are not recovered by this slice) and is modelled as correctly-sized opaque
    // storage so the three X360-pinned accessor offsets (+4, +8, +0x801C) are exact. The
    // corona-submission handle is the address/handle of a foreign corona-submission interface
    // whose own home lands elsewhere; the asm treats this slot as a single 32-bit word.
    class InputBuffer_Dispatch : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storage (see FLAG above): the DWARF :307 scene-query-results queue
        // (OutSmSceneQueryResultsQueue) occupies +0xC..+0x801B; its own home lands elsewhere. Sized
        // exactly so muCoronaSubmissionInterface stays @+0x801C.
        // RETYPED 2026-07-24: the storage span (0x801C-0xC == 32784 == 32768+16)
        // is exactly sizeof(VariableEventQueue<32768,16>), and WorldModule::
        // GenerateDispatchLists @0x827D1CE8 drives Clear/AddEvent through it.
        typedef CgsModule::VariableEventQueue<32768, 16> SceneResultQueueStorage;

        // X360 0x822B8EA8: read-lock; return the dispatch-frame index (this+4).
        u32  GetDispatchFrame() const;
        // X360 0x827A1180: write-lock; set the dispatch-frame index (this+4).
        void SetDispatchFrame(u32 luDispatchFrame);
        // X360 0x822B8F50: read-lock; return the shadow-map handle (this+8).
        u32  GetShadowMap() const;
        // X360 0x827A1228: write-lock; set the shadow-map handle (this+8).
        void SetShadowMap(u32 luShadowMap);
        // :295 (own TU): read-lock; return the scene-query-results queue (this+0xC).
        const SceneResultQueueStorage* GetSceneResultQueue() const;
        // X360 0x827BB1E0 (:296, THIS batch): write-lock; return the scene-query-results queue (this+0xC).
        SceneResultQueueStorage* GetSceneResultQueue();
        // X360 0x822B8FF8: read-lock; return the corona-submission interface handle (this+0x801C).
        u32  GetCoronaSubmissionInterface() const;
        // X360 0x827A12D0: write-lock; set the corona-submission interface handle (this+0x801C).
        void SetCoronaSubmissionInterface(u32 luCoronaSubmissionInterface);

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 places muDispatchFrame at this+4,
        // so pad bytes +1..+3 explicitly.
        u8                     maStatusPad[3];                    // +1..+3 (force +4)
        u32                    muDispatchFrame;                   // +4
        u32                    muShadowMap;                       // +8
        // Scene-query-results queue (:307) at +0xC (opaque; see FLAG). Spans up to the +0x801C
        // start of muCoronaSubmissionInterface.
        SceneResultQueueStorage mSceneResultQueue;                // +0xC   :307
        u32                    muCoronaSubmissionInterface;       // +0x801C
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::InputBuffer_PrePhysics (DWARF BrnPropEntityModuleIO.h:529 /
    // members :547-:548).
    // The prop-entity module's pre-physics INPUT buffer: the scene/AI bridges fill it with the
    // per-frame potential-contact queue and the reset-on-track result queue.
    // LAYOUT (DWARF :547/:548 member order + X360 Append offset, authoritative):
    //   base  CgsModule::IOBuffer                                   (1-byte status; +1..+0xF pad)
    //   +0x10 OutPotentialContactQueue mPotentialContactQueue       (EventQueue<PotentialContact,2048>) :547
    //   +...  ResetOnTrackResultQueue  mResetOnTrackResultQueue                                         :548
    // mPotentialContactQueue lands at +0x10 because PotentialContact is alignas(16); X360
    // AppendPotentialContactQueue @ 0x827AA170 pins it with `addi r3, this, 0x10`.
    class InputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
    public:
        // DWARF :547 typedef -- OutputBuffer::OutPotentialContactQueue (capacity 2048; committed
        // element CgsSceneManager::SceneManagerIO::PotentialContact).
        typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>
                OutPotentialContactQueue;
        // DWARF :548 typedef -- AIModuleResultInterface::ResetOnTrackResultQueue (committed home).
        typedef BrnAI::AIModuleIO::AIModuleResultInterface::ResetOnTrackResultQueue
                ResetOnTrackResultQueue;

        void Construct();                                                          // :534
        void Destruct();                                                           // :538
        const OutPotentialContactQueue* GetPotentialContactQueue() const;          // :540
        // X360 0x827AA170 (THIS batch): write-lock; Append onto mPotentialContactQueue (this+0x10).
        void AppendPotentialContactQueue(const OutPotentialContactQueue* lpQueue);  // :541
        const ResetOnTrackResultQueue* GetResetOnTrackResultQueue() const;         // :543
        void AppendResetOnTrackResultQueue(const ResetOnTrackResultQueue* lpQueue); // :544

        static void _AssertLayout();

    private:
        OutPotentialContactQueue mPotentialContactQueue;   // +0x10 :547
        ResetOnTrackResultQueue  mResetOnTrackResultQueue;  //       :548
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::InputBuffer_PostPhysics (DWARF BrnPropEntityModuleIO.h:558).
    // The prop-entity module's post-physics INPUT buffer: the physics side fills it with the
    // contact-spy results and the per-prop post-physics update events, and
    // BrnWorld::PropEntityModule's post-physics step drains it.
    //
    // This slice homes the real, fully-named members (both element types are already
    // reconstructed in-tree) so Construct can build them BY NAME -- no opaque storage is
    // needed here. The X360 Construct (0x822EFDC8) does exactly three things:
    //   *this = 1;                       -> IOBuffer base: mark constructed (status byte = 1)
    //   mUpdatedPropQueue.Construct();   -> EventQueue<UpdatePropEvent,200>::Construct (this+0x10)
    //   mContactSpyInterface.Construct();-> ContactSpyInterface::Construct          (this+0x04)
    //
    // LAYOUT (X360 Construct member offsets + DWARF :558 member order, authoritative):
    //   base  CgsModule::IOBuffer            (1-byte status; +1..+3 natural pad)
    //   +0x04 ContactSpyInterface mContactSpyInterface   (sizeof 4)                       :576
    //   +0x10 UpdatePropEventQueue mUpdatedPropQueue     (EventQueue<UpdatePropEvent,200>):577
    // The queue starts at +0x10 because EventQueue<UpdatePropEvent,200> inherits a 16-byte
    // alignment from its inline maEvents[] (UpdatePropEvent is alignas(16)); the asm confirms
    // it with `addi r3, r31, 0x10`, and mContactSpyInterface at `addi r3, r31, 4`. The
    // DWARF lays mContactSpyInterface (:576) before mUpdatedPropQueue (:577), matching the
    // ascending offsets; the Construct body just happens to build the queue first.
    class InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
    public:
        // DWARF :77 -- the buffer spells its embedded contact-spy interface via a member
        // typedef; :87/99-style typedefs name the post-physics update-event queue.
        typedef BrnPhysics::ContactSpy::ContactSpyInterface              ContactSpyInterface;
        typedef CgsModule::EventQueue<BrnPhysics::Props::UpdatePropEvent, 200> UpdatePropEventQueue;

        // X360 0x822EFDC8 (the only function this TU bodies): mark the IOBuffer base
        // constructed, then construct the update-prop queue and the contact-spy interface.
        void Construct();

        // The remaining DWARF-attested members (:567-:573) live in their own (not-yet-
        // reconstructed) TUs; declared here for declaration-shape fidelity, bodied elsewhere.
        void Destruct();                                                    // :567
        const ContactSpyInterface* GetContactSpyInterface() const;          // :569
        const UpdatePropEventQueue* GetUpdatedPropQueue() const;            // :570
        ContactSpyInterface* GetContactSpyInterface();                      // :572
        void AppendUpdatedPropQueue(const UpdatePropEventQueue* lpQueue);   // :573

        static void _AssertLayout();

    private:
        ContactSpyInterface  mContactSpyInterface;   // +0x04 :576
        UpdatePropEventQueue mUpdatedPropQueue;       // +0x10 :577
    };

    // ------------------------------------------------------------------------
    // PropEntityIO post-scene buffers (callers: WorldModule::
    // EntityModulePostSceneUpdate @0x827C3C58 -- crash bridge in, prop post-scene
    // update out).
    //
    // FLAG (minimal-complete slice, size NOT X360-attested): the real aggregates
    // are this module's post-scene event queues; that layout belongs to the prop
    // IO TU and is NOT recovered here. GROW when it lands.
    // ------------------------------------------------------------------------
    struct InputBuffer_PostScene : public CgsModule::IOBuffer
    {
        u8 maDeferredPayload[16];
    };

    struct OutputBuffer_PostScene : public CgsModule::IOBuffer
    {
        u8 maDeferredPayload[16];
    };
}
}
