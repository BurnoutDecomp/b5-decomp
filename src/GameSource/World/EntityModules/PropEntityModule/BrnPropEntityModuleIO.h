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
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h" // BrnPhysics::Props::PropInputInterface (OutputBuffer_PrePhysics::mPropInputInterface, retyped 2026-08-10)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"  // CgsSceneManager::SceneManagerIO::PotentialContact (InputBuffer_PrePhysics::mPotentialContactQueue element)
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"           // BrnAI::AIModuleIO::AIModuleResultInterface::ResetOnTrackResultQueue (InputBuffer_PrePhysics::mResetOnTrackResultQueue)

// The replay-status payload InputBuffer_PreScene::SetReplayStatusInterface latches.
// RETYPED 2026-08-11 (WorldBridgeInputToEntityModules mount): the forward declaration that
// stood here was enough while the setter was declaration-only, but the setter is bodied now
// and the member is held BY VALUE (see the note on the member), so the real home has to be
// included.
#include "GameSource/Replays/BrnReplayStatusInterface.h"   // BrnReplays::ReplayIO::StatusInterface

// ---- ADDITIVE GROW 2026-08-12 (prop-spawn wave, agent B5) --------------------------------
// InputBuffer_PreScene's real interior: the three streaming notification queues, the player
// position / race-car velocity block and the game-action-derived scalars.
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<300000>
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT (GetHitPropsBitArray tripwire)
#include "SharedIO/BrnPropGraphicsAndZoneEvents.h"           // PropGraphicsLoaded/Unloaded/InstancesNeededForZoneEvent
#include "BrnCommonTypes.h"                                  // Vector3

// ---- ADDITIVE GROW 2026-08-12 (prop-BOOT wave, agent B8) ---------------------------------
// The three nested interfaces the prepare/pre-scene/pre-physics OUTPUT buffers hold BY VALUE.
// They used to be opaque byte spans here, which is exactly why nothing could Construct them
// (see the banner on OutputBuffer_Prepare). Their real homes:
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"              // BrnResource::GameDataIO::RequestInterface<1024>
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // CgsSceneManager::SceneManagerIO::InSceneUpdateInterface
#include "SharedIO/BrnPropToTrafficInterface.h"                                // PropToTrafficInterface + the two TrafficLight*Event structs
                                                                               // (RE-HOMED there 2026-08-12; this include replaces their old
                                                                               //  definitions below and breaks the former A<->B cycle)

// Pointer-only uses in InputBuffer_Dispatch (AGENTS.md forward-declaration exception (b)):
// including CgsDispatcher.h / BrnShadowMap.h here would pull the whole renderer + shadow
// cascade tail into every includer of this IO header.
namespace CgsGraphics { class DispatchFrame; }

namespace BrnWorld
{
    struct ShadowMap;

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

    // (TrafficLightKnockDownEvent / TrafficLightRestoreEvent were RE-HOMED 2026-08-12 to
    //  SharedIO/BrnPropToTrafficInterface.h -- their DWARF home, and the same file as the
    //  interface whose queues hold them. Included at the top of this header; nothing else
    //  changed about them.)

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
        // ⭐⭐ RETYPED 2026-08-12 (prop-BOOT wave, agent B8) -- see the identical note on
        // OutputBuffer_Prepare below for the full argument. Short form: the three interfaces
        // were CONSOLE-SIZED OPAQUE SPANS that every consumer reinterpret_cast to the real
        // host type, and nothing could Construct them because a span has no Construct. Both
        // halves of that are fixed by naming the DWARF types (:647/:648/:649).
        typedef BrnResource::GameDataIO::RequestInterface<1024>         ResourceRequestInterfaceStorage; // :647 (DWARF typedef :74)
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterfaceStorage;      // :648 (DWARF typedef :75)
        typedef BrnPhysics::Props::PropInputInterface                   PropInputInterfaceStorage;       // :649 (DWARF typedef :76)

        // :650 -- DWARF type GuiOverheadSignInfoEvent::VisibleOverheadSignArray
        // (== Array<BrnGui::OverheadSignScore,32>), which has NO committed home.
        // ⭐ GROWN 2026-08-12 from `maBytes[1]` to the console extent, because Construct
        // @0x822EFB98 zeroes exactly one word at buffer+832128 == &mVisibleOverheadSignArray
        // + 1024 -- twice, once in the Construct pass and once in the Clear pass -- and a
        // 1-byte member had nothing to zero. 1024 == 32 * 0x20; the trailing word IS the
        // array's count (BridgePropToOutput_PreScene's console body materialises the same
        // aggregate on the stack and zeroes the word at sp+0x400 == +1024 before
        // AppendArray). FLAG: only the count is named -- the 32 elements stay opaque until
        // BrnGui::OverheadSignScore gets a home.
        struct VisibleOverheadSignArrayStorage
        {
            unsigned char maSigns[32 * 0x20];   // +0    FLAG opaque element interior
            s32           miCount;              // +1024 the count Construct/Clear zero

            void Clear() { miCount = 0; }
        };

        // X360 0x827A1A18 (:640, THIS batch): read-lock handle, returns this + 4 (mResourceRequestInterface).
        const ResourceRequestInterfaceStorage* GetResourceRequestInterface() const;
        // X360 0x822B9888: write-lock handle, returns this + 4 (mResourceRequestInterface).
        ResourceRequestInterfaceStorage* GetResourceRequestInterface();
        // X360 0x827A1970: read-lock handle, returns this + 819824 (mPropInputInterface).
        const PropInputInterfaceStorage* GetPropInputInterface() const;
        // X360 0x822B97E0: write-lock handle, returns this + 819824 (mPropInputInterface).
        PropInputInterfaceStorage* GetPropInputInterface();
        // ADDITIVE GROW (prop-spawn wave, 2026-08-12). X360 0x822B9738 (IDA `sub_822B9738`,
        // unnamed): write-lock handle, returns this + 1056 (mSceneInputInterface); its
        // "Not locked for writing" tripwire bakes BrnPropEntityModuleIO.h:645. It is the
        // scene interface PropCellManager::DeactivateCell / PropEntityModule::
        // ReplayUpdate*InScene / PropZoneManager::RemoveAllPropsAndParts push through.
        SceneInputInterfaceStorage* GetSceneInputInterface();
        // ADDITIVE GROW (prop-BOOT wave, 2026-08-12, agent B8). X360 0x827A18C8 (IDA
        // `sub_827A18C8`, unnamed): read-lock handle (bit 4, `extrwi r11,r11,1,27`),
        // returns this + 1056 (`addi r3, r28, 0x420`) -- the CONST twin of the getter
        // above, DWARF :634. It is the one the per-frame scene merge uses:
        // WorldModule::BridgeEntityModulesToSceneModule_PreScene @0x827AB490 calls it as
        // its FOURTH leg (`mr r3,r27 ; bl sub_827A18C8` at 0x827AB5C4) and Appends the
        // result into the scene manager's update input. Without it that leg had no const
        // source to read and was dropped, so no prop ever reached the broad phase.
        const SceneInputInterfaceStorage* GetSceneInputInterface() const;
        // X360 0x827A1AC0 (:643, THIS batch): read-lock handle, returns this + 831104 (mVisibleOverheadSignArray).
        const VisibleOverheadSignArrayStorage* GetVisibleOverheadSignArray() const;
        // X360 0x822B9930 (:644, THIS batch): write-lock handle, returns this + 831104 (mVisibleOverheadSignArray).
        VisibleOverheadSignArrayStorage* GetVisibleOverheadSignArray();

        // DWARF :628 / :632. X360 Construct @0x822EFB98 (bodied in this buffer's own TU).
        void Construct();
        // BODIED 2026-08-15 (IO-buffer zero-fill removal audit): DestroyIOBuffer<T> is the
        // console's mirror now and calls T::Destruct, so this could no longer stay
        // declaration-only. X360 0x822DC3D0 IS this function and it is a single instruction --
        // `b CgsModule::IOBuffer::Destruct` -- i.e. the whole body is the base call. (It is also
        // the ICF representative every other base-only Destruct in this family folds into: the
        // DestroyIOBuffer instantiations for OutputBuffer_Prepare @0x827B5E20, OutputBuffer_PreScene
        // @0x827B6918, InputBuffer_PreScene @0x827B89A0, OutputBuffer_PrePhysics @0x827B9470 and
        // InputBuffer_PostPhysics @0x827B9E70 all `bl` this address.)
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

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
        // ⭐⭐⭐ RETYPED 2026-08-12 (prop-BOOT wave, agent B8) -- THE FIX FOR THE 21-ASSERT
        // BOOT STORM + the access violation in InSceneUpdateInterface::SetCullingGroupPair.
        //
        // These three were `unsigned char maBytes[...]` spans sized to the CONSOLE offsets,
        // and mPropInputInterface was a 1-byte trailing placeholder. Two independent things
        // were wrong with that, and both bit at once on the first boot that reached
        // WorldModule::Prepare -> PropEntityModule::Prepare:
        //
        //   (1) NOTHING COULD CONSTRUCT THEM. A byte span has no Construct, so the X360's
        //       OutputBuffer_Prepare::Construct @0x822EFC58 -- which is nothing BUT the three
        //       nested Construct/Clear calls -- had no C++ to be written as, and the buffer's
        //       `Construct()` silently resolved to the 2-line CgsModule::IOBuffer base. The
        //       embedded VariableEventQueue<1024,16> therefore kept mbIsConstructed == false
        //       and every request fired "Not Constructed" (CgsVariableEventQueue.h:688/454/
        //       728/983/1000/348). This is the FOURTH instance of the project's recurring
        //       "promised-but-never-written Construct" disease (cf. PhysicsSimulationIO's
        //       InputBuffer/OutputBuffer/InputBuffer_PostPhysics).
        //   (2) THE SPANS WERE THE WRONG SIZE ON THE HOST -- the recurring console-offset bug.
        //       Every consumer already reinterpret_cast them to the real type
        //       (BrnPropEntityModule.cpp's GetSceneInterface/GetGameDataRequestInterface/
        //       GetPropInputInterface helpers), and the real host types are WIDER than their
        //       console spans (8-byte mpEvents per embedded queue, two-pointer ResourceHandle).
        //       Writing an 11 KB host PropInputInterface through a 1-byte trailing member ran
        //       off the end of the IOBufferStack allocation.
        //
        // Naming the DWARF types (:609/:610/:611, typedefs :74/:75/:76) fixes both at once:
        // the members can Construct themselves, the casts become identities, and the host
        // compiler -- not a console constant -- decides where each member sits. This is the
        // same move OutputBuffer_PrePhysics::PropInputInterfaceStorage already made on
        // 2026-08-10 (the "seventh use of the storage-typedef pattern"); the *Storage names
        // are kept so no call site has to change.
        typedef BrnResource::GameDataIO::RequestInterface<1024>         ResourceRequestInterfaceStorage; // :609 (DWARF typedef :74)
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterfaceStorage;      // :610 (DWARF typedef :75)
        typedef BrnPhysics::Props::PropInputInterface                   PropInputInterfaceStorage;       // :611 (DWARF typedef :76)

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

        // DWARF :593 / :597. X360 Construct @0x822EFC58 (bodied in this buffer's own TU).
        void Construct();
        // BODIED 2026-08-15 (IO-buffer zero-fill removal audit). No out-of-line X360 symbol of
        // its own because it ICF-folded with OutputBuffer_PreScene::Destruct @0x822DC3D0
        // (`b CgsModule::IOBuffer::Destruct`) -- which is exactly what DestroyIOBuffer<
        // OutputBuffer_Prepare> @0x827B5E20 calls. Base-only, no member teardown.
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

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
        // ⭐ RETYPED 2026-08-10 (pre-physics bridge wave; SEVENTH use of the storage-typedef
        // pattern). mPropInputInterface was a correctly-CONSOLE-sized opaque span, and
        // WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics @0x827AAEC0 hands it
        // straight to Props::PropInputInterface::Append. On the host that type is LARGER than
        // its console span (four embedded event queues with an 8-byte mpEvents, plus a
        // two-pointer ResourceHandle -- the physics input buffer already carries a
        // KU_PROP_DRIFT for exactly this), so reading a host-shaped PropInputInterface out of
        // an 11292-byte console-sized span walks off the end of the member and reads whatever
        // follows as a queue length. Naming the real type makes both ends the same type: no
        // cast, no overrun, and mPropToTrafficInterface simply moves down by the host growth
        // (the layout gate below computes that drift from sizeof, so it keeps its teeth).
        typedef BrnPhysics::Props::PropInputInterface PropInputInterfaceStorage;
        // ⭐ RETYPED 2026-08-12 (prop-BOOT wave, agent B8) -- EIGHTH use of the pattern, and
        // the same two faults the Prepare buffer had: a 1-byte trailing span that (a) nothing
        // could Construct, so its two embedded EventQueues kept mpEvents == NULL, and (b) is
        // ~2.2 KB smaller than the host type BrnPropZoneManager.cpp:676 already
        // reinterpret_casts it to (SendTrafficLightRestoreEvents) -- an off-the-end write.
        // PropToTrafficInterface's DWARF home now also homes the two TrafficLight*Event
        // structs, so including it here costs no cycle. DWARF :703.
        typedef PropToTrafficInterface PropToTrafficInterfaceStorage;               // trailing +11296

        // X360 0x827A1B68 (own TU): read-lock handle, returns this + 4 (mPropInputInterface).
        const PropInputInterfaceStorage* GetPropInputInterface() const;
        // X360 0x822B99D8 (own TU): write-lock handle, returns this + 4 (mPropInputInterface).
        PropInputInterfaceStorage* GetPropInputInterface();
        // X360 0x827A1C10 (:698, THIS batch): read-lock handle, returns this + 11296 (mPropToTrafficInterface).
        const PropToTrafficInterfaceStorage* GetPropToTrafficInterface() const;
        // X360 0x822B9A80 (:699, THIS batch): write-lock handle, returns this + 11296 (mPropToTrafficInterface).
        PropToTrafficInterfaceStorage* GetPropToTrafficInterface();

        // DWARF :688 / :692. X360 Construct @0x822EFCF0 (bodied in this buffer's own TU).
        void Construct();
        // BODIED 2026-08-15 (IO-buffer zero-fill removal audit). ICF-folded with
        // OutputBuffer_PreScene::Destruct @0x822DC3D0 -- DestroyIOBuffer<OutputBuffer_PrePhysics>
        // @0x827B9470 calls that address. Base-only, no member teardown.
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

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
    // ⭐ SUPERSEDED 2026-08-12 (prop-spawn wave, agent B5): the old FLAG here said
    // "muHitPropsBitArray is a 32-bit console word ... resolve when the prop TU lands",
    // and the interior up to +0x780 was one opaque pad. The prop TU has now landed
    // (PropEntityModule::PreSceneUpdate @0x82309A40, the buffer's only consumer), so the
    // word is resolved to the pointer it is, the three streaming queues and the
    // position/velocity block are named, and the three "bool" flags are resolved to the
    // tri-state latches they are. See the big banner inside the class.
    class InputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        // ====================================================================
        // ADDITIVE GROW 2026-08-12 (prop-spawn wave, agent B5) -- the FULL DWARF member
        // set (BrnPropEntityModuleIO.h:474-:490) plus the DWARF accessor surface
        // (:344-:463). Nothing below claims a HOST byte offset: every member is named and
        // the host compiler lays the class out. The console offsets in the comments are
        // PROVENANCE, recovered from the two ends that pin them --
        //   * InputBuffer_PreScene::Construct   @0x822EFAA0  (the initial values), and
        //   * PropEntityModule::PreSceneUpdate  @0x82309A40  (the reads),
        //   * WorldModule::BridgeInputToEntityModules @0x827ADF88 (the inlined setters).
        //
        // THE THREE QUEUES. The X360 emits the const getters out of line; their `this+N`
        // returns pin the member order exactly:
        //     0x822B91F8 -> this + 0x004   GetPropInstancesNeededForZoneQueue() const  (:458)
        //     0x822B90A8 -> this + 0x04C   GetPropGraphicsLoadedQueue() const          (:452)
        //     0x822B9150 -> this + 0x08C   GetPropGraphicsUnloadedQueue() const        (:455)
        // (the baked assert lines are the X360 file's; the DecFIGS dump is 3 lines older).
        // 0x04C-0x004 == 72 == 12 + 30*2 and 0x08C-0x04C == 64 == round4(12 + 25*2), which
        // is exactly EventQueue<...,30> / EventQueue<...,25> over the 2-byte events
        // BrnPropGraphicsAndZoneEvents.h homes -- and matches Construct's three calls
        // (PropGraphicsLoadedEvent_25 on this+0x4C, PropInstancesNeededForZoneEvent_30 on
        // this+4, PropGraphicsUnloadedEvent_25 on this+0x8C).
        //
        // ⚠ meOnlineStatus / meEasySmashPropsStatus / mePropProgressionStatus ARE NOT
        // BOOLS. Construct stores **2** in all three (`li r11,2 ; stb r11,0x78D/E/F`)
        // while the bridge only ever stores 0 or 1 -- a THREE-state latch whose third
        // state means "no change this frame". That is what the DWARF's six
        // HasJustChangedTo* / HasPropProgressionBeen* predicates read, and it is why
        // PreSceneUpdate is a PAIR of tests per flag (`if (==1) m = true; if (==0) m =
        // false;`) instead of a plain copy: state 2 leaves the module's flag alone. The
        // previous `bool mbIsOnline` model silently collapsed that third state.
        //
        // ⚠ mpabHitPropBitArray IS A POINTER, not the u32 the previous slice modelled.
        // PreSceneUpdate does `memcpy(&mZoneManager.maPreviouslyHitProps,
        // *(lpInput+0x780), 37504)` -- it DEREFERENCES the word -- and guards it with the
        // buffer's own baked tripwire "mpabHitPropBitArray != NULL"
        // (BrnPropEntityModuleIO.h:848). The bridge's PropSmashReport case reads the game
        // action's first word (`lwz r11,0(r31)`) and stores it straight here, i.e. the
        // action carries `const Profile::HitPropsBitArray*` (BrnGameActions.h:5156).
        //
        // ⚠ mbSendingPropProgression / mbReloadingProfile ARE SWAPPED vs the DecFIGS DIE
        // order (:486 mbReloadingProfile then :487 mbSendingPropProgression). The X360 asm
        // is unambiguous: the bridge's SEND_PROP_PROGRESSION case writes +0x790 and its
        // PROP_SMASH_REPORT case writes +0x791 next to the +0x780 pointer store; and
        // PreSceneUpdate reads +0x790 -> meStreamingMode = E_RESET_UNLOADING_FOR_PROFILE
        // ("we are about to hand the profile our progression, drop everything") and +0x791
        // -> install the profile's hit-props and resume E_STREAM ("the profile came
        // back"). Declaration order here follows the ASM.
        // ====================================================================

        // BrnPropEntityModuleIO.h:81 / :83 / :85 -- the buffer's own queue typedefs.
        typedef CgsModule::EventQueue<PropInstancesNeededForZoneEvent, 30> PropInstancesNeededForZoneQueue;
        typedef CgsModule::EventQueue<PropGraphicsLoadedEvent, 25>         PropGraphicsLoadedQueue;
        typedef CgsModule::EventQueue<PropGraphicsUnloadedEvent, 25>       PropGraphicsUnloadedQueue;
        // DWARF `Profile::HitPropsBitArray` (BrnProfile.h:555). Same instantiation the
        // committed PropZoneManager::HitPropsBitArray names -- a typedef, not a new type,
        // so the two spell one type and no header cycle is introduced.
        typedef CgsContainers::BitArray<300000u> HitPropsBitArray;

        // :344 / :348 -- X360 Construct @0x822EFAA0 (bodied in this buffer's own TU).
        void Construct();
        // BODIED 2026-08-15 (IO-buffer zero-fill removal audit). No symbol of its own in ARTIST
        // because it ICF-folded with OutputBuffer_PreScene::Destruct @0x822DC3D0 -- the address
        // DestroyIOBuffer<InputBuffer_PreScene> @0x827B89A0 calls. Base-only.
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

        void SetReplayStatusInterface(const BrnReplays::ReplayIO::StatusInterface* lpReplayStatusInterface);

        // ---- the three streaming queues (X360 out-of-line; bodied in this buffer's TU) ----
        const PropGraphicsLoadedQueue*         GetPropGraphicsLoadedQueue() const;          // :449 / X360 0x822B90A8
        PropGraphicsLoadedQueue*               GetPropGraphicsLoadedQueue();                // :450
        const PropGraphicsUnloadedQueue*       GetPropGraphicsUnloadedQueue() const;        // :452 / X360 0x822B9150
        PropGraphicsUnloadedQueue*             GetPropGraphicsUnloadedQueue();              // :453
        const PropInstancesNeededForZoneQueue* GetPropInstancesNeededForZoneQueue() const;  // :455 / X360 0x822B91F8
        PropInstancesNeededForZoneQueue*       GetPropInstancesNeededForZoneQueue();        // :456

        // ---- X360 header-inlines (the consumer/producer both open-code these) ----------
        // Producer side (inlined into BridgeInputToEntityModules @0x827ADF88).
        void SetHitPropsBitArray(const HitPropsBitArray& lrHitProps)
        {
            mbReloadingProfile   = true;                 // stb 1, 0x791  (store order: asm)
            mpabHitPropBitArray  = &lrHitProps;          // stw   , 0x780
        }
        void SetCurrentTimestep(f32 lfTimeStep)                   { mfCurrentTimeStep = lfTimeStep; }        // :426
        void SetIsOnline(bool lbIsOnline)                         { meOnlineStatus = lbIsOnline ? E_CHANGESTATUS_ON : E_CHANGESTATUS_OFF; }            // :414
        void SetEasySmashProps(bool lbEasySmashProps)             { meEasySmashPropsStatus = lbEasySmashProps ? E_CHANGESTATUS_ON : E_CHANGESTATUS_OFF; } // :418
        void SetPropProgressionEnabled(bool lbEnabled)            { mePropProgressionStatus = lbEnabled ? E_CHANGESTATUS_ON : E_CHANGESTATUS_OFF; }      // :422
        void SendingPropProgression()                             { mbSendingPropProgression = true; }       // :367
        void ReloadingProfile()                                   { mbReloadingProfile = true; }             // :361
        void ResetProps()                                         { mbResetProps = true; }                   // :407
        void SetPlayerCarIndex(u8 lu8Index)                       { mu8PlayerCarIndex = lu8Index; }          // :352
        void SetPlayerPosition(Vector3 lPosition)                 { mPlayerPos = lPosition; }                // :374
        void SetPlayerCarCrashing(bool lbCrashing)                { mbPlayerCrashing = lbCrashing; }         // :378
        void SetPlayerWrecked(bool lbWrecked)                     { mbPlayerWrecked = lbWrecked; }           // :382
        void SetPlayerZoneNumber(s32 liZoneNumber)                { miPlayerZoneNumber = liZoneNumber; }     // :386
        void SetRaceCarVelocity(s32 liIndex, Vector3 lVelocity)   { maRaceCarVelocity[liIndex] = lVelocity; }// :400

        // Consumer side (inlined into PreSceneUpdate @0x82309A40 as bare lbz/lwz/lvx).
        Vector3 GetPlayerPosition() const                   { return mPlayerPos; }             // :355
        u8      GetPlayerCarIndex() const                   { return mu8PlayerCarIndex; }      // :358
        Vector3 GetRaceCarVelocity(s32 liIndex) const       { return maRaceCarVelocity[liIndex]; } // :404
        s32     GetPlayerZoneNumber() const                 { return miPlayerZoneNumber; }     // :389
        f32     GetCurrentTimestep() const                  { return mfCurrentTimeStep; }      // :429
        bool    IsPlayerWrecked() const                     { return mbPlayerWrecked; }        // :392
        bool    IsPlayerCrashing() const                    { return mbPlayerCrashing; }       // :395
        bool    ShouldResetProps() const                    { return mbResetProps; }           // :410
        bool    IsReloadingProfile() const                  { return mbReloadingProfile; }     // :364
        bool    IsSendingPropProgression() const            { return mbSendingPropProgression; } // :370
        const HitPropsBitArray& GetHitPropsBitArray() const                                    // :459
        {
            // The buffer's own baked tripwire, BrnPropEntityModuleIO.h:848, which
            // PreSceneUpdate fires before the copy.
            CGS_ASSERT(mpabHitPropBitArray != 0, "mpabHitPropBitArray != NULL");
            return *mpabHitPropBitArray;
        }

        // The six tri-state predicates (:432-:447). "Just changed" == the status byte is
        // no longer the E_CHANGESTATUS_NO_CHANGE Construct seeded.
        bool HasJustChangedToEasySmashOn() const   { return meEasySmashPropsStatus  == E_CHANGESTATUS_ON;  }  // :432
        bool HasJustChangedToEasySmashOff() const  { return meEasySmashPropsStatus  == E_CHANGESTATUS_OFF; }  // :435
        bool HasJustChangedToOnline() const        { return meOnlineStatus          == E_CHANGESTATUS_ON;  }  // :438
        bool HasJustChangedToOffline() const       { return meOnlineStatus          == E_CHANGESTATUS_OFF; }  // :441
        bool HasPropProgressionBeenEnabled() const { return mePropProgressionStatus == E_CHANGESTATUS_ON;  }  // :444
        bool HasPropProgressionBeenDisabled() const{ return mePropProgressionStatus == E_CHANGESTATUS_OFF; }  // :447

        // The tri-state the three `uint8_t me*Status` members carry. The DWARF types them
        // uint8_t (no enum DIE survives), so the enumerator NAMES are descriptive; the
        // VALUES are X360 facts: Construct seeds 2, the bridge writes 0 / 1.
        enum EChangeStatus
        {
            E_CHANGESTATUS_OFF       = 0,
            E_CHANGESTATUS_ON        = 1,
            E_CHANGESTATUS_NO_CHANGE = 2
        };

        static void _AssertLayout();

    private:
        PropInstancesNeededForZoneQueue mPropInstancesNeededForZoneQueue;  // :474  console +0x004
        PropGraphicsLoadedQueue         mPropGraphicsLoadedQueue;          // :475  console +0x04C
        PropGraphicsUnloadedQueue       mPropGraphicsUnloadedQueue;        // :476  console +0x08C

        // FLAG (unrecovered members): the console span +0x0CC..+0x6EF holds members this
        // slice does not recover. Construct touches three of them -- a word at +0x0CC, six
        // byte flags at +0x1D0 + i*0x101, and a {-1, -1, float} triple at +0x6D8/+0x6DC/
        // +0x6E0 -- but nothing in the prop module reads them, so they are left as one
        // opaque span rather than guessed at. THE SIZE IS THE CONSOLE SPAN, NOT A HOST
        // OFFSET CLAIM: it only reserves "at least this much unknown payload"; every
        // member around it is reached by name and the host lays them out wherever it
        // likes. GROW it into named members when their producer lands.
        u8 maUnrecoveredPayload[0x6F0 - 0x0CC];

        Vector3 mPlayerPos;                          // :477  console +0x6F0
        // :478. Console +0x700..+0x77F; Construct zeroes all eight with `stvx128` at
        // 0x700/0x710/.../0x770, which is what pins the 16-byte stride and the count.
        Vector3 maRaceCarVelocity[8];

        const HitPropsBitArray* mpabHitPropBitArray; // :479  console +0x780 (see banner)
        f32  mfCurrentTimeStep;                      // :480  console +0x784
        s32  miPlayerZoneNumber;                     // :481  console +0x788 (-1 == none)
        u8   mu8PlayerCarIndex;                      // :482  console +0x78C
        u8   meOnlineStatus;                         // :483  console +0x78D (EChangeStatus)
        u8   meEasySmashPropsStatus;                 // :484  console +0x78E (EChangeStatus)
        u8   mePropProgressionStatus;                // :485  console +0x78F (EChangeStatus)
        bool mbSendingPropProgression;               // :487  console +0x790 (asm order)
        bool mbReloadingProfile;                     // :486  console +0x791 (asm order)
        bool mbResetProps;                           // :488  console +0x792
        bool mbPlayerCrashing;                       // :489  console +0x793
        bool mbPlayerWrecked;                        // :490  console +0x794

        // ---- ADDITIVE GROW 2026-08-11 (WorldBridgeInputToEntityModules mount) ----------
        // The frame's replay status, latched by SetReplayStatusInterface @0x827A1578.
        // [FLAG unrecovered console offset] 0x827A1578 is a HOLE in the .ida-exports dump
        // (the bridge's xref table names it, but no per-function json was emitted), so the
        // member's console byte offset inside this buffer is NOT recovered -- it is placed
        // at the tail here so the +0x780..+0x792 block above, whose offsets the bridge's
        // own inlined stores DO pin, is left undisturbed. It is reached only by name.
        // Held BY VALUE with an operator= copy, matching both of this setter's siblings:
        // the RaceCar twin (InputBuffer_PreScene::SetReplayStatusInterface @0x8279D258,
        // called by the bridge on the line immediately before this one, with the SAME
        // source pointer) and the producing side (BrnWorldIO::UpdateInputBuffer::
        // SetReplayStatusInterface @0x823B4DF0). The PS3 DWARF for this buffer has neither
        // the member nor the method -- it is a merge-window delta, X360-only.
        BrnReplays::ReplayIO::StatusInterface mReplayStatusInterface;
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

        // ⚠️ POINTER WIDTH CORRECTED 2026-08-12 (prop-spawn wave, phase 2 -- render slice).
        // The first two slots were modelled as `u32` handles because the X360 accessors are a
        // bare `lwz r3,4(this)` / `lwz r3,8(this)`. They are not handles: the console words ARE
        // the 32-bit pointers (WorldModule seeds the sibling world/race-car dispatch inputs with
        // `SetDispatchFrame(lpDispatchFrame)` / `SetShadowMap(&mShadowMap)`, and
        // WorldEntityIO::InputBuffer_GenerateDispatchLists already carries them as real pointers).
        // Keeping them u32 on x64 truncates a host pointer -- the recurring project bug. The
        // console offsets below are PROVENANCE ONLY; every access goes through the named member.
        // The corona-submission slot stays u32 for now: its only consumer (the parked corona
        // loop in BrnPropEntityModule_Render.cpp) needs no dereference, and widening it would
        // drag BrnCoronaManager's whole header into this one for a nested class that cannot be
        // forward-declared.
        //
        // X360 0x822B8EA8: read-lock; return the dispatch frame (console word @this+4).
        CgsGraphics::DispatchFrame* GetDispatchFrame() const;
        // X360 0x827A1180: write-lock; set the dispatch frame (console word @this+4).
        void SetDispatchFrame(CgsGraphics::DispatchFrame* lpDispatchFrame);
        // X360 0x822B8F50: read-lock; return the shadow map (console word @this+8).
        ShadowMap* GetShadowMap() const;
        // X360 0x827A1228: write-lock; set the shadow map (console word @this+8).
        void SetShadowMap(ShadowMap* lpShadowMap);
        // :295 (own TU): read-lock; return the scene-query-results queue (this+0xC).
        const SceneResultQueueStorage* GetSceneResultQueue() const;
        // X360 0x827BB1E0 (:296, THIS batch): write-lock; return the scene-query-results queue (this+0xC).
        SceneResultQueueStorage* GetSceneResultQueue();
        // X360 0x822B8FF8: read-lock; return the corona-submission interface handle (this+0x801C).
        u32  GetCoronaSubmissionInterface() const;
        // X360 0x827A12D0: write-lock; set the corona-submission interface handle (this+0x801C).
        void SetCoronaSubmissionInterface(u32 luCoronaSubmissionInterface);

        // X360 Construct @0x822DC300 / Destruct @0x822DC358 (both real ledger symbols).
        // ADDED 2026-08-12 (prop-BOOT wave, agent B8): Construct was never written, so
        // `Construct()` on this buffer resolved to the CgsModule::IOBuffer base and the
        // embedded VariableEventQueue<32768,16> stayed un-Constructed. Bodied in this
        // buffer's own TU.
        void Construct();
        // BODIED 2026-08-15 (IO-buffer zero-fill removal audit) -- DestroyIOBuffer<T> is the
        // console's mirror now and calls T::Destruct. Unlike its base-only siblings this one has
        // a REAL body, X360 0x822DC358, and it is three statements in this order:
        //   bl  CgsModule::IOBuffer::Destruct(this)
        //   stw 0, 4(this)                                  -> mpDispatchFrame = 0
        //   bl  VariableEventQueue<32768,16>::Clear(this+12) -> mSceneResultQueue.Clear()
        // (the console does NOT null mpShadowMap here, only mpDispatchFrame -- Construct nulls
        //  both; reproduced exactly. Both console words are host pointers, so they are touched by
        //  name, per this header's POINTER WIDTH note.)
        void Destruct()
        {
            CgsModule::IOBuffer::Destruct();
            mpDispatchFrame = 0;
            mSceneResultQueue.Clear();
        }

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 places the dispatch-frame word at
        // this+4, so pad bytes +1..+3 explicitly. On the host the two widened pointers push the
        // queue and the corona word past their console offsets -- deliberately: this buffer is a
        // pure runtime object (nothing serialises it), so host width is correct and the console
        // offsets are provenance.
        u8                          maStatusPad[3];                    // +1..+3 (console: force +4)
        CgsGraphics::DispatchFrame* mpDispatchFrame;                   // console +4
        ShadowMap*                  mpShadowMap;                       // console +8
        // Scene-query-results queue (:307) at console +0xC (opaque; see FLAG).
        SceneResultQueueStorage     mSceneResultQueue;                 // console +0xC   :307
        u32                         muCoronaSubmissionInterface;       // console +0x801C
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
        // BODIED 2026-08-15 (IO-buffer zero-fill removal audit). No out-of-line symbol of its own
        // in ARTIST: like every base-only Destruct in this family it ICF-folded with
        // PropEntityIO::OutputBuffer_PreScene::Destruct @0x822DC3D0, a bare
        // `b CgsModule::IOBuffer::Destruct`. Base-only, no member teardown.        // :538
        void Destruct() { CgsModule::IOBuffer::Destruct(); }
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

        // The remaining DWARF-attested members (:569-:573) live in their own (not-yet-
        // reconstructed) TUs; declared here for declaration-shape fidelity, bodied elsewhere.
        // BODIED 2026-08-15 (IO-buffer zero-fill removal audit): DestroyIOBuffer<
        // InputBuffer_PostPhysics> @0x827B9E70 calls PropEntityIO::OutputBuffer_PreScene::Destruct
        // @0x822DC3D0 -- the ICF representative, a bare `b CgsModule::IOBuffer::Destruct`.
        void Destruct() { CgsModule::IOBuffer::Destruct(); }                // :567
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
