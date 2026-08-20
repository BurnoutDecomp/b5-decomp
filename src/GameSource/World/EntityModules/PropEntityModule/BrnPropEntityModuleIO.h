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
// ⭐ WAVE Q KEYSTONE (2026-08-18): the two halves of the retyped
// OutputBuffer_PreScene::VisibleOverheadSignArrayStorage (see that typedef's note).
#include "GameShared/GameClasses/Containers/CgsArray.h"   // ::Array<T,N>
#include "GameSource/Graphics/BrnCoronaManager.h"        // BrnCoronaManager::BrnSubmissionInterface
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"           // BrnGui::OverheadSignScore

// ---- ADDITIVE GROW 2026-08-18 (wave Q round 2, world-side prop IO-buffer pass) -----------
// The element/interface types OutputBuffer_PostPhysics and InputBuffer_PostScene hold BY
// VALUE, now that both classes carry real members instead of console-sized opaque storage.
#include "GameSource/GameState/BrnGameEvents.h"                                // BrnGameState::GameStateModuleIO::RecordPropHitEvent / HitOverheadSignEvent
#include "SharedIO/BrnPropBecamePhysicalEvent.h"                               // BrnWorld::PropEntityIO::PropBecamePhysicalEvent
#include "GameSource/Replays/BrnReplayRequestInterface.h"                      // BrnReplays::ReplayIO::RequestInterface (the X360-only post-physics member)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h" // BrnWorld::CrashIO::RaceCarCrashCompleteEvent (InputBuffer_PostScene)
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

        // ⭐ BODIED 2026-08-18 (wave Q round 2). All four were declared-only "own TU" here and
        // DEFINED NOWHERE in the tree -- four unresolved externals the moment ProcessContacts
        // lands (it calls Construct). There is no own TU: the X360 emits NO out-of-line symbol
        // for any of them (the only PropVFXLocatorEvent symbols in the whole image are the
        // queue instantiations EventQueue<...,10>::Construct @0x8228DB20,
        // BaseEventQueue<...>::AddEventSafe @0x822C9970 and ::Append @0x823C4058). They are
        // header inlines, and Construct's fold is MEASURED in ProcessContacts
        // @0x822FAF7C..0x822FAFCC: the console builds the event in a stack slot as four
        // `stvx128` rows at +0x00/+0x10/+0x20/+0x30 (mTransform), `stw` the prop type at +0x40
        // and `stw 1` at +0x44 (meEventType == E_EVENTTYPE_PROPSMASH), then hands the slot to
        // AddEventSafe. i.e. member-for-member assignment with no tripwire.
        // The three getters have no decompiled fold of their own; their bodies are the reads
        // their declarations force [INFERENCE, marked].
        void Construct(Matrix44Affine lTransform, u32 luTypeId, EEventType leEventType)   // :242
        {
            mTransform  = lTransform;
            muTypeId    = luTypeId;
            meEventType = leEventType;
        }
        const Matrix44Affine& GetTransform() const { return mTransform; }    // :250
        u32                   GetPropType() const  { return muTypeId; }      // :256
        EEventType            GetEventType() const { return meEventType; }   // :262

    private:
        Matrix44Affine mTransform;   // :268
        u32            muTypeId;     // :269
        EEventType     meEventType;  // :270
    };

    // ========================================================================
    // BrnWorld::PropEntityIO::BrokenPropEvent -- a "a prop just broke" notification published
    // into OutputBuffer_PostPhysics::mBrokenPropQueue by
    // BrnWorld::PropEntityModule::ProcessContacts (AddEvent call @0x822FAF78).
    //
    // ⚠️ CORRECTED 2026-08-18 (wave Q round 3): this banner used to say the queue is "drained
    // by BrnWorld::PropEntityModule::ProcessBrokenProps". It is NOT. ProcessBrokenProps
    // @0x822EEFA0 takes `(const InputBuffer_PrePhysics*, OutputBuffer_PrePhysics*)`
    // (BrnPropEntityModule.h:564) and never touches this buffer -- it retires the props/parts
    // the PHYSICS side reported broken. The consumer of THIS queue is not identified anywhere
    // in `b5-decomp/src` today (re-grepped: the only readers of the queue or the event type
    // are the producer, its explicit-instantiation TUs and the buffer's own accessor), so the
    // drain side is an open question, not a recovered fact.
    //
    // SIZE (X360, authoritative): sizeof == 1. Pinned by
    // BaseEventQueue<BrokenPropEvent>::AddEvent @ 0x822C9838, which copies the event
    // with `lbz r10, 0(src); stbx r10, miLength, mpEvents` -- a single byte at
    // stride 1 (no index scaling). EventQueue<BrokenPropEvent,50>::Construct
    // @ 0x822E5060 places the inline maEvents[50] at +0xC (base mpEvents@0,
    // miMaxLength@4, miLength@8, maEvents@12 -- no alignment pad, confirming the
    // element's alignment is 1), with miMaxLength = 50 (0x32).
    //
    // ⚠️ WHAT THE SINGLE BYTE ACTUALLY HOLDS -- CORRECTED 2026-08-18 (wave Q round 3).
    // The old text ("the broken prop's index/id") is contradicted by the only producer in the
    // image. MEASURED in ProcessContacts, immediately before the AddEvent:
    //       0x822FAF5C  lwz  r11, 4(r25)     ; r25 == the PropContact; +0 is mEntityIdA (the
    //                                        ; PROP, owner byte 3 == E_ENTITYTYPE_PROP, checked
    //                                        ; @0x822FAAA0) and +4 is mEntityIdB (the RACE CAR,
    //                                        ; owner byte 1, checked @0x822FAA90)
    //       0x822FAF64  srwi r11, r11, 10    ; KU_ENTITY_INDEX_BASE -- the entity-index field
    //       0x822FAF6C  stb  r11, <event>    ; low 8 bits only
    // So the payload is the low byte of the STRIKING CAR's entity index, not the prop's. The
    // prop's own identity is carried out of this function by the separate
    // maRecentlyBrokenProps insert @0x822FAF58, not by this event.
    //
    // ⚠️ THE FIELD NAME muPropIndex IS THEREFORE A MISNOMER and is knowingly left in place:
    // renaming it edits PropEntityModule_wQ2_03.cpp:422 (another lane's file) and, more to the
    // point, the consumer that would settle the name is not decoded anywhere in the tree (see
    // the class banner above). Rename to muCarIndex once a reader is recovered.
    //
    // This is the only field and the type is exactly one byte wide as the asm attests.
    struct BrokenPropEvent
    {
        u8 muPropIndex;   // +0x00  single-byte payload; MEASURED as (u8)car.GetEntityIndex()
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

        // :650 -- DWARF type GuiOverheadSignInfoEvent::VisibleOverheadSignArray.
        // ⭐ RETYPED 2026-08-18 (wave Q keystone; the output-buffer half of park P8 in
        // BrnPropEntityModule_PreScene.cpp). This was `{ unsigned char maSigns[32*0x20];
        // s32 miCount; }` -- correctly SIZED but opaque, on the belief that
        // BrnGui::OverheadSignScore "has no committed home". It does:
        // GameSource/Gui/BrnGuiEventTypeDefs.h:155, with its 0x20 stride static_assert'd, and
        // DWARF BrnGuiEventTypeDefs.h:1001 spells the typedef
        //     typedef Array<BrnGui::OverheadSignScore,32u> VisibleOverheadSignArray;
        // The opacity was the whole of what blocked P8: with both ends opaque there was no
        // typed array for PropEntityModule::PreSceneUpdate's
        // `Array<OverheadSignScore,32>::AppendArray<32>` (X360 @0x822E5348) to append to.
        // The previous shape's facts all survive: the element run is still 32 * 0x20 bytes and
        // the count word Construct/Clear zero is still ::Array's miCount immediately after it.
        typedef ::Array<BrnGui::OverheadSignScore, 32> VisibleOverheadSignArrayStorage;

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
    //
    // ⭐⭐ RETYPED 2026-08-18 (wave Q round 2, world-side prop IO-buffer pass). Everything in
    // this class was CONSOLE-SIZED OPAQUE STORAGE: 1-byte placeholders separated by byte pads
    // computed from X360 offsets. That shape carried four defects, all fixed here.
    //
    //  (1) THREE ACCESSORS WERE MISIDENTIFIED. Construct @0x822EFE08 is a 44-instruction
    //      roll-call of every member -- each nested Construct is an out-of-line call whose IDA
    //      symbol carries the instantiation -- so it gives the complete map, and it disagrees
    //      with the offsets the old getters were labelled with. MEASURED, instruction by
    //      instruction (44 instructions, 0x822EFE08..0x822EFEB4, counted). ⚠️ Each row cites
    //      BOTH addresses -- the address-forming `addi`/`addis` FIRST, then the `bl` it feeds.
    //      An earlier revision of this table printed the `bl` address beside the `addi` text on
    //      five of these rows (corrected 2026-08-18, round 3):
    //        addi 0x822EFE1C / bl 0x822EFE24     r3 = this+0x860   -> InSceneUpdateInterface::Construct
    //        addis+addi 0x822EFE28..2C           r30 = this+0xC86B0 (== 0xD0000-0x7950) -> the four
    //                    PropInputInterface queues: +0 AddPhysicalPropEvent<50> (bl 0x822EFE34),
    //                    +0x1F60 RemovePhysicalPropEvent<300> (bl 0x822EFE3C), +0x28CC
    //                    RemovePhysicalPartEvent<100> (bl 0x822EFE44), +0xFB0
    //                    AddPhysicalPartEvent<50> (bl 0x822EFE4C), then `stb 0, 0x2C00(r30)`
    //                    @0x822EFE58 == mbRemoveAllPropsAndParts
    //        addi 0x822EFE54 / bl 0x822EFE5C     r3 = this+0x820   -> BrokenPropEvent<50>::Construct
    //        addi 0x822EFE60 / bl 0x822EFE64     r3 = this+0x7B0   -> HitOverheadSignEvent<100>::Construct
    //        addis+addi 0x822EFE68..6C / bl 0x822EFE70  +0xCB2C0   -> PropVFXLocatorEvent<10>::Construct
    //        addi 0x822EFE74 / bl 0x822EFE78     r3 = this+0x160   -> RecordPropHitEvent<50>::Construct
    //        addi 0x822EFE7C / bl 0x822EFE80     r3 = this+0x10    -> PropBecamePhysicalEvent<20>::Construct
    //        addis+addi 0x822EFE84..8C           +0xCB5F0, then the `li r10,0xB; mtctr; stw 0`
    //                    loop @0x822EFE90..0x822EFEA0 -> 11 pointer slots cleared
    //        0x822EFEAC  stbx 0 at +0xCB61C      -> mbShouldRequestProgression = false
    //      i.e. the console layout is
    //        +0x000   IOBuffer status byte
    //        +0x010   EventQueue<PropBecamePhysicalEvent,20>                        :748
    //        +0x160   EventQueue<BrnGameState::..::RecordPropHitEvent,50>           :749
    //        +0x7B0   EventQueue<BrnGameState::..::HitOverheadSignEvent,100>        :750
    //        +0x820   EventQueue<BrokenPropEvent,50>                                :751
    //        +0x860   InSceneUpdateInterface  mSceneInputInterface                  :752
    //        +0xC86B0 PropInputInterface      mPropInputInterface                   :753
    //        +0xCB2C0 EventQueue<PropVFXLocatorEvent,10>                            :754
    //        +0xCB5F0 BrnReplays::ReplayIO::RequestInterface  (X360-ONLY, no DWARF member)
    //        +0xCB61C bool mbShouldRequestProgression                               :755
    //      -- exactly the DWARF :748..:755 member ORDER with one X360-only member inserted
    //      before the trailing bool. Console sizeof == 0xCB620 == 833056, from
    //      DestroyIOBuffer<OutputBuffer_PostPhysics> @0x827B9C98 (`lis r5,0xC; ori r5,0xB620`).
    //      The getters therefore re-bind as below -- each one verified by DUMPING it (lock bit,
    //      baked BrnPropEntityModuleIO.h line, return offset):
    //        0x822B9B28  W  line 741  -> +0x860    GetSceneInputInterface()
    //        0x822B9BD0  W  line 742  -> +0xC86B0  GetPropInputInterface()      (IDA "…::GetProp")
    //        0x822B9C78  W  line 743  -> +0x7B0    GetHitOverheadSignQueue()
    //        0x822B9D20  W  line 744  -> +0x820    GetBrokenPropQueue()
    //        0x822B9DC8  W  line 745  -> +0xCB2C0  GetPropVFXLocatorQueue()
    //        0x822B9E70  W  line 746  -> +0x160    GetRecordHitPropQueue()
    //        0x822B9F18  W  line 747  -> +0x10     GetPropBecamePhysicalEventQueue()
    //        0x822B9FC0  W  line 748  -> +0xCB5F0  GetReplayRequestInterface()  (IDA "…::GetRep")
    //      and the READ-lock twins, all IDA-UNNAMED and therefore invisible to a name-keyed grep
    //      of the JSON export set -- MEASURED 2026-08-20 (gateui wave) by enumerating and
    //      decompiling every function start in 0x827A1600..0x827A2100 on a private .i64 copy:
    //        0x827A1CB8  R  line 732  -> +0x860    GetSceneInputInterface() const
    //        0x827A1D60  R  line 734  -> +0x7B0    GetHitOverheadSignQueue() const
    //        0x827A1E08  R  line 736  -> +0xCB2C0  GetPropVFXLocatorQueue() const
    //        0x827A1EB0  R  line 737  -> +0x160    GetRecordHitPropQueue() const
    //        0x827A1F58  R  line 738  -> +0x10     GetPropBecamePhysicalEventQueue() const
    //        0x827A2000  R  line 739  -> +0xCB5F0  GetReplayRequestInterface() const
    //      (733 and 735 are NOT emitted -- the const twins of GetPropInputInterface and
    //      GetBrokenPropQueue have no call site anywhere in the image. Every emitted const twin's
    //      baked line is exactly its non-const twin's minus 9, so the class body is a contiguous
    //      const block 732..739 then a contiguous non-const block 741..748 -- an INDEPENDENT
    //      corroboration of the 741..748 re-binding recorded below.)
    //      The old header bound 0x822B9B28 to GetHitOverheadSignQueue, 0x822B9BD0 to
    //      GetSceneInputInterface and 0x822B9FC0 to GetPropInputInterface -- each one member out
    //      of step. No committed .cpp called any of the three, so nothing shipped broken.
    //      ProcessContacts @0x822FA944 independently confirms 0x822B9BD0 IS the prop-input
    //      getter: it immediately zeroes that interface's four queue lengths (+8 / +0x1F68 /
    //      +0x28D4 / +0xFB8) and its +0x2C00 flag.
    //  (2) The X360 asserts number the seven getters that HAVE a DWARF counterpart 741..747,
    //      where the DWARF numbers those same seven 733..739 (+8) -- one extra member plus its
    //      const/non-const getter pair ahead of them. The EIGHTH baked line, 748
    //      (0x822B9FC0 -> +0xCB5F0), is the replay RequestInterface getter and has NO DWARF
    //      counterpart at all; that is exactly what corroborates +0xCB5F0 as an X360-only
    //      addition. (Corrected 2026-08-18, round 3: this used to read "these seven ...
    //      741..748", which spans eight lines.)
    //  (3) mbShouldRequestProgression (:755) had no member at all, and the DWARF's
    //      ShouldRequestPropProgression() / RequestPropProgression() (:742/:745) no declaration.
    //  (4) THE PADS WERE A LOADED GUN (AGENTS.md gotcha 1). Each pad was sized from a console
    //      span on the assumption the member it bracketed was one byte; a real host
    //      EventQueue<T,N> is up to 8 bytes WIDER than its console twin (mpEvents widens 4->8),
    //      so re-typing any placeholder in place would have overrun the following pad -- the
    //      wave-Q keystone measured the BrokenPropQueue case going NEGATIVE. The pads are GONE:
    //      every member is now the real host type and the host compiler lays the class out. The
    //      console offsets above are PROVENANCE ONLY and appear in no expression.
    class OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
    public:
        // ---- DWARF member typedefs (:99 / :91 / :89 / :93 / :95) + the X360-only replay one --
        typedef CgsModule::EventQueue<PropBecamePhysicalEvent, 20>                                 PropBecamePhysicalEventQueue; // :99
        typedef CgsModule::EventQueue<BrnGameState::GameStateModuleIO::RecordPropHitEvent, 50>     RecordHitPropQueue;           // :91
        typedef CgsModule::EventQueue<BrnGameState::GameStateModuleIO::HitOverheadSignEvent, 100>  HitOverheadSignQueue;         // :89
        typedef CgsModule::EventQueue<BrokenPropEvent, 50>                                         BrokenPropQueue;              // :93
        typedef CgsModule::EventQueue<PropVFXLocatorEvent, 10>                                     PropVFXLocatorQueue;          // :95
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface                            SceneInputInterface;          // :752 (DWARF typedef :75)
        typedef BrnPhysics::Props::PropInputInterface                                              PropInputInterface;           // :753 (DWARF typedef :76)
        // X360-ONLY member (no DWARF counterpart -- a merge-window addition). The TYPE is
        // MEASURED: 11 pointer slots zeroed by Construct, and PostPhysicsUpdate @0x823032A0
        // calls this getter then BrnReplays::ReplayIO::RequestInterface::RegisterSerialiser on
        // the result with r4 == &mPropEntitySerialiser. The NAME is [INFERENCE]: IDA truncates
        // the getter symbol to "…::GetRep", and the house spelling for this member everywhere
        // else is `typedef RequestInterface ReplayRequestInterface; ... mReplayRequestInterface;
        // ... GetReplayRequestInterface()` (DWARF EffectsModuleIO.h:299/302/330,
        // BrnRootSoundModuleIo.h:287/290/309, BrnWorldModuleIO.h:482/485/700; the committed
        // BrnWorldModuleIO.h:533 already uses it verbatim).
        typedef BrnReplays::ReplayIO::RequestInterface                                             ReplayRequestInterface;

        // ---- X360-emitted accessors (write-lock unless marked) -------------------------------
        // X360 0x822B9B28 (line 741): write-lock; return &mSceneInputInterface (console +0x860).
        SceneInputInterface* GetSceneInputInterface();
        // X360 0x822B9BD0 (line 742): write-lock; return &mPropInputInterface (console +0xC86B0).
        PropInputInterface* GetPropInputInterface();
        // X360 0x822B9C78 (line 743): write-lock; return &mHitOverheadSignQueue (console +0x7B0).
        HitOverheadSignQueue* GetHitOverheadSignQueue();
        // X360 0x822B9D20 (line 744): write-lock; return &mBrokenPropQueue (console +0x820).
        BrokenPropQueue* GetBrokenPropQueue();
        // X360 0x822B9DC8 (line 745): write-lock; return &mPropVFXLocatorQueue (console +0xCB2C0).
        PropVFXLocatorQueue* GetPropVFXLocatorQueue();
        // X360 0x822B9E70 (line 746): write-lock; return &mRecordHitPropQueue (console +0x160).
        RecordHitPropQueue* GetRecordHitPropQueue();
        // X360 0x822B9F18 (line 747): write-lock; return &mPropBecamePhysicalEventQueue (console +0x10).
        PropBecamePhysicalEventQueue* GetPropBecamePhysicalEventQueue();
        // X360 0x822B9FC0 (line 748): write-lock; return &mReplayRequestInterface (console +0xCB5F0).
        ReplayRequestInterface* GetReplayRequestInterface();

        // ====================================================================================
        // ⭐ THE READ-LOCK (const) TWINS -- ADDED 2026-08-20 (gateui wave, owner `wire`).
        //
        // ⚠️ THIS CORRECTS A RECORDED "THERE IS NO SUCH SYMBOL" CONCLUSION. The gateui scout
        // report (scratch/gateui_wave/scout.md §E) states that the ONLY read-lock twin on this
        // buffer is 0x827A2000 (+0xCB5F0) and that a host-invented FLAGged accessor would be
        // needed for +0x160. That is WRONG: SIX const twins are emitted, they were simply
        // UNNAMED in the JSON export set (`sub_827A1CB8` … `sub_827A1F58`), so a name-keyed grep
        // finds nothing. MEASURED on a private BURNOUT_X360_ARTIST.XEX.i64 copy by enumerating
        // every function start in 0x827A1600..0x827A2100 and decompiling each: every one opens
        // `lbz r11,0(this) ; extrwi r11,r11,1,27` (the READ bit -- IOBuffer::
        // IsBufferLockedForReading), fires "Not locked for reading\n" against
        // ..\..\..\GameSource\World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h,
        // and tail-returns `this + <offset>`:
        //
        //     0x827A1CB8  R  baked line 732  -> +0x860    mSceneInputInterface
        //   [ 733 not emitted: the const GetPropInputInterface twin has no caller ]
        //     0x827A1D60  R  baked line 734  -> +0x7B0    mHitOverheadSignQueue
        //   [ 735 not emitted: the const GetBrokenPropQueue twin has no caller ]
        //     0x827A1E08  R  baked line 736  -> +0xCB2C0  mPropVFXLocatorQueue
        //     0x827A1EB0  R  baked line 737  -> +0x160    mRecordHitPropQueue
        //     0x827A1F58  R  baked line 738  -> +0x10     mPropBecamePhysicalEventQueue
        //     0x827A2000  R  baked line 739  -> +0xCB5F0  mReplayRequestInterface
        //
        // ⭐ THE BAKED LINES INDEPENDENTLY CORROBORATE THE NON-CONST TABLE ABOVE. Every emitted
        // const twin sits at exactly (its non-const line - 9): 732/741 scene, 734/743 sign,
        // 736/745 vfx, 737/746 record-hit, 738/747 became-physical, 739/748 replay. The two
        // gaps (733, 735) fall precisely on the two members whose const twin has no call site,
        // so the class body is a contiguous const block (732..739) then a contiguous non-const
        // block (741..748). That is a second, independent proof of the 2026-08-18 re-binding.
        //
        // WHY THEY MATTER: WorldModule::BridgeEntityModulesToOutput_PostPhysics @0x827AEEB0
        // runs with this buffer READ-locked (BrnWorldModule.cpp :: WorldModule::Update brackets
        // it with LockBuffersForIO(dest, …sources) -- dest write, sources read), so the four
        // prop legs there call these, not the write getters. Calling a non-const getter under a
        // read lock is the "Not locked for writing" tripwire.
        // ====================================================================================
        // X360 0x827A1CB8 (line 732): read-lock; return &mSceneInputInterface (console +0x860).
        const SceneInputInterface* GetSceneInputInterface() const;
        // X360 0x827A1D60 (line 734): read-lock; return &mHitOverheadSignQueue (console +0x7B0).
        const HitOverheadSignQueue* GetHitOverheadSignQueue() const;
        // X360 0x827A1E08 (line 736): read-lock; return &mPropVFXLocatorQueue (console +0xCB2C0).
        const PropVFXLocatorQueue* GetPropVFXLocatorQueue() const;
        // X360 0x827A1EB0 (line 737): read-lock; return &mRecordHitPropQueue (console +0x160).
        // ⭐ THE gateui SEAM: the source the world->output bridge appends into the world
        // update-output GameEventQueue as game event 111 (E_EVENT_RECORD_PROP_HIT).
        const RecordHitPropQueue* GetRecordHitPropQueue() const;
        // X360 0x827A1F58 (line 738): read-lock; return &mPropBecamePhysicalEventQueue (+0x10).
        const PropBecamePhysicalEventQueue* GetPropBecamePhysicalEventQueue() const;
        // X360 0x827A2000 (line 739): read-lock twin of GetReplayRequestInterface(); same member.
        const ReplayRequestInterface* GetReplayRequestInterface() const;

        // DWARF :719. X360 @0x822EFE08 -- bodied in this buffer's own TU.
        void Construct();
        // DWARF :723. No out-of-line symbol of its own: like every base-only Destruct in this
        // family it ICF-folded with PropEntityIO::OutputBuffer_PreScene::Destruct @0x822DC3D0,
        // which is the address DestroyIOBuffer<OutputBuffer_PostPhysics> @0x827B9C94 calls.
        void Destruct() { CgsModule::IOBuffer::Destruct(); }

        // DWARF :742 / :745. Both header-inline on the console. RequestPropProgression's fold is
        // MEASURED at PostPhysicsUpdate 0x8230321C..0x82303228 -- `lis r10,0xC ; ori r10,0xB61C ;
        // stbx r17,r31,r10` with r17 == 1 and NO lock-bit test inlined, unlike every out-of-line
        // accessor in this family (all of which open `lbz r11,0(this) ; extrwi r11,r11,1,28`).
        // So the setter carries NO tripwire; adding one would be a fabricated recovered fact.
        // ShouldRequestPropProgression's body is the read its declaration forces -- [INFERENCE],
        // no fold of the query side is decompiled yet.
        void RequestPropProgression()             { mbShouldRequestProgression = true; }
        bool ShouldRequestPropProgression() const { return mbShouldRequestProgression; }

        static void _AssertLayout();

    private:
        // Real host-laid-out members in the DWARF's (== the console's) order. NOTHING below
        // claims a host byte offset; the console offsets in the comments are provenance.
        PropBecamePhysicalEventQueue mPropBecamePhysicalEventQueue;  // console +0x010   :748
        RecordHitPropQueue           mRecordHitPropQueue;            // console +0x160   :749
        HitOverheadSignQueue         mHitOverheadSignQueue;          // console +0x7B0   :750
        BrokenPropQueue              mBrokenPropQueue;               // console +0x820   :751
        SceneInputInterface          mSceneInputInterface;           // console +0x860   :752
        PropInputInterface           mPropInputInterface;            // console +0xC86B0 :753
        PropVFXLocatorQueue          mPropVFXLocatorQueue;           // console +0xCB2C0 :754
        ReplayRequestInterface       mReplayRequestInterface;        // console +0xCB5F0 (X360-only)
        bool                         mbShouldRequestProgression;     // console +0xCB61C :755
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
        // ⭐ RETYPED 2026-08-18 (wave Q keystone): u32 -> the real interface pointer. See the
        // note on muCoronaSubmissionInterface below.
        BrnCoronaManager::BrnSubmissionInterface* GetCoronaSubmissionInterface() const;
        // X360 0x827A12D0: write-lock; set the corona-submission interface handle (this+0x801C).
        void SetCoronaSubmissionInterface(BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface);

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
        // ⭐ RETYPED 2026-08-18 (wave Q keystone). This was `u32 muCoronaSubmissionInterface`
        // -- a CONSOLE-WIDTH WORD STANDING IN FOR A POINTER, which is AGENTS.md gotcha 1's
        // exact shape and cannot hold an x64 address. The console really does `lwzx`/`stwx` a
        // 32-bit word here (0x822B8FF8 / 0x827A12D0), because on the X360 a pointer IS 32
        // bits; on the host it must widen. Three independent confirmations that the word is a
        // BrnCoronaManager::BrnSubmissionInterface*:
        //   * the SIBLING buffer already types it that way --
        //     RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists::
        //     SetCoronaSubmissionInterface(BrnCoronaManager::BrnSubmissionInterface*)
        //     (BrnRaceCarEntityModuleIO.h:852, X360 @0x8279EBC8);
        //   * so does the producer end -- BrnWorldIO::DispatchInputBuffer and
        //     RendererIO::OutputBuffer both carry the pointer type
        //     (BrnWorldModuleIO_DispatchInputBuffer.h:116, BrnRendererModuleIO.h:153);
        //   * the consumer dereferences it -- RenderPropAndCoronas' parked corona tail calls
        //     `lpCoronaSubmissionInterface->AddPropCorona(...)` (X360 @0x823FD138, now
        //     declared at BrnCoronaManager.h:237).
        // ⚠️ NOT a live corruption today: NOTHING in the tree calls this setter, so the field
        // has only ever been read as 0 (which is what makes GenerateDispatchLists log
        // "corona submission interface is NULL" and skip prop coronas). Retyped now so that
        // when the renderer->prop bridge leg IS written it cannot truncate.
        BrnCoronaManager::BrnSubmissionInterface* mpCoronaSubmissionInterface;  // console +0x801C (u32 there)
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
        // X360 0x822B92A0: READ-lock (`extrwi r11,r11,1,27`, baked line 0x223 == 547); returns
        // this + 0x10 (&mPotentialContactQueue). ⭐ BODIED 2026-08-18 (wave Q round 2) -- it was
        // declared here and defined NOWHERE, while TWO wave-Q partfiles already call it
        // (PropEntityModule_wQ_05.cpp:176, PropEntityModule_wQ_07.cpp:628), i.e. a live
        // unresolved external that `cl /c` cannot see.
        const OutPotentialContactQueue* GetPotentialContactQueue() const;          // :540
        // X360 0x827AA170 (THIS batch): write-lock; Append onto mPotentialContactQueue (this+0x10).
        void AppendPotentialContactQueue(const OutPotentialContactQueue* lpQueue);  // :541
        const ResetOnTrackResultQueue* GetResetOnTrackResultQueue() const;         // :543
        // X360 0x827AA220 -- NO per-address JSON export exists for it, so this was dumped
        // directly out of the IDB with headless idat: write-lock assert (baked line 0x227 ==
        // 551), then `addis r3,this,3 ; addi r3,r3,-0x7FE0` == this + 0x28020
        // (&mResetOnTrackResultQueue) and `bl BaseEventQueue<ResetOnTrackResult>::Append` with
        // r4 == lpQueue. Identity is independently pinned by the Append's own xrefs_to list
        // (.ida-exports/.../0x827A71B8.json names 0x827AA220 as
        // InputBuffer_PrePhysics::AppendResetOnTrackResultQueue). ⭐ BODIED 2026-08-18.
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
    // ------------------------------------------------------------------------

    // ⭐ GROWN 2026-08-18 (wave Q round 2). This was `struct { u8 maDeferredPayload[16]; }`
    // with a "size NOT X360-attested ... GROW when it lands" banner. The size IS attested,
    // and the buffer is a single member -- Construct @0x822EFC40 is the WHOLE class:
    //     mr r11,r3 ; li r10,1 ; addi r3,r11,8 ; stb r10,0(r11)
    //     b BrnWorld__CrashIO__RaceCarCrashCompleteEvent_10___Construct
    // i.e. IOBuffer::Construct() then the one member's Construct at console +8, with the
    // capacity 10 carried IN THE CALLEE SYMBOL. Destruct @0x822DC398 is
    // `bl CgsModule::IOBuffer::Destruct` then `stw 0, 0x10(this)` == that queue's miLength
    // (queue base +8), i.e. the queue's own Clear folded in.
    // The 16-byte placeholder could NOT be re-typed in place (gotcha 1): the queue is
    // 16 + 10*16 == 176 bytes on the host (12 + 10*16 == 172 on the console, mpEvents
    // widening 4->8), so a reinterpret_cast onto maDeferredPayload was an out-of-bounds
    // walk, not the project's opaque-storage idiom. The member is real now.
    struct InputBuffer_PostScene : public CgsModule::IOBuffer
    {
        // DWARF :519 spells the member type RaceCarOutputInterface::RaceCarCrashCompleteEventQueue;
        // that interface's own home (BrnCrashModuleRaceCarIOInterfaces.h) is still a reserved
        // blob, so the queue is spelled out here from the Construct callee symbol. Same type
        // either way.
        typedef CgsModule::EventQueue<BrnWorld::CrashIO::RaceCarCrashCompleteEvent, 10>
                RaceCarCrashCompleteEventQueue;

        void Construct();   // DWARF :505, X360 @0x822EFC40
        void Destruct();    // DWARF :509, X360 @0x822DC398

        // DWARF :516 -- the DWARF's own name for the const accessor. PostSceneUpdate
        // @0x822C4718 folds it as a bare `addi r28, r24, 8` (0x822C476C) with NO lock-bit
        // test inlined, so it is a plain header inline with no tripwire (the buffer's
        // LockForRead happens in the caller at 0x822C4734).
        const RaceCarCrashCompleteEventQueue* GetCrashEventQueue() const
        {
            return &mRaceCarCrashCompleteEventQueue;
        }

        // DWARF :513. DECLARATION ONLY -- no X360 out-of-line symbol and no decompiled fold
        // exists for it (its producer, WorldModule::BridgeCrashModuleToPropModule_PostScene,
        // is still the inert boot gate at WorldLinkStubs.cpp:2305), so writing a body would be
        // invention. Declared so the bridge can be spelled against the real name when it lands.
        void AppendRaceCarCrashQueue(const RaceCarCrashCompleteEventQueue* lpQueue);

        static void _AssertLayout();

    private:
        RaceCarCrashCompleteEventQueue mRaceCarCrashCompleteEventQueue;   // console +0x08 :519
    };

    // ⭐ CORRECTED 2026-08-18 (wave Q round 2). This carried the same
    // `u8 maDeferredPayload[16]` placeholder as its input twin under a "size NOT
    // X360-attested ... GROW when it lands" banner. It has NOTHING to grow: the buffer is
    // EMPTY, and the placeholder was actively misleading (it advertised a 16-byte payload
    // that does not exist).
    //   * CreateIOBuffer<PropEntityIO::OutputBuffer_PostScene> @0x827B6BA0 calls
    //     `IOBufferStack::Alloc(stack, 1, name)` -- `li r4, 1` at 0x827B6C3C. The console
    //     sizeof is ONE BYTE: the IOBuffer status byte and nothing else.
    //   * That confirms the DWARF, which gives this struct exactly two members and no data
    //     (BrnPropEntityModuleIO.h:661 -> :666 Construct, :670 Destruct).
    //   * Neither Construct nor Destruct has an out-of-line symbol under this class's name:
    //     the CreateIOBuffer above tail-calls WorldEntityIO::OutputBuffer_PrePhysics::
    //     Construct, i.e. both ICF-folded into the base-only representatives.
    //   * The only decompiled consumer, PropEntityModule::PostSceneUpdate @0x822C4718, does
    //     nothing with it but LockForWrite @0x822C473C / UnlockForWrite @0x822C4800.
    struct OutputBuffer_PostScene : public CgsModule::IOBuffer
    {
        void Construct() { CgsModule::IOBuffer::Construct(); }   // :666, ICF-folded
        void Destruct()  { CgsModule::IOBuffer::Destruct(); }    // :670, ICF-folded
    };
}
}
