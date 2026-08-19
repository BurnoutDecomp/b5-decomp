#pragma once

// The six RaceCarEntityModuleIO payload types (DWARF home BrnRaceCarEntityModuleIO.h; the real
// element/generic types live in their respective subsystem headers -- see per-type notes
// below).
//
// ⚠ THE HEADER'S ORIGINAL PREMISE IS RETIRED. It opened life as a "minimal slice" of
// reserved-byte blobs, on the theory that byte-exact layout was not required because every
// member is only ever handed out by name. That was WRONG FIVE TIMES: each of the five queues
// has since been GROWN to its real instantiation after a console producer was found writing
// THROUGH the member (Clear()+Append() / memcpy), which a 256-byte blob would have overrun into
// the next member. There are NO reserved-byte blobs left in this file, and a new payload must
// be the real type from the start -- see each type's own ⛔ note for the measurement that
// forced it.
//
// These six payloads are embedded BY VALUE in the BrnRaceCarEntityModuleIO IO buffers and are
// only ever returned by named pointer (Get*Queue()/Get*Interface() -> &member), so a sized
// complete type is sufficient for the IO header + its 33 accessors to compile with correct
// semantics. Byte-exact buffer layout is NOT required (all access is named).
//
// Namespace + names are exactly what BrnRaceCarEntityModuleIO.h forward-declares/uses
// (BrnWorld::RaceCarEntityModuleIO::<Type>) so the assemble step can replace the IO header's
// local forward-decls with one #include of this file.
//
// REAL underlying instantiations (per DecFIGS DWARF; do NOT pull the generic + element
// cascade in here -- STUB RULE 5):
//   GameActionQueue        -> InputBuffer::GameActionQueue (BaseGameActionQueue<N>; AIModuleIO
//                             shows BaseGameActionQueue<13312>).  RaceCar TU :63/:257.
//   TakedownEventQueue     -> CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>
//                             (BrnAIModuleIO.h:54; BrnNetworkManagerUnity.cpp:1582).  :403/:444.
//   GameEventQueue         -> World-level EventQueue<BrnGameState::GameEvent, N>
//                             (typedef GameEventQueue GameEventQueue).  :520/:491/:602.
//   PotentialContactQueue  -> OutputBuffer::OutPotentialContactQueue =
//                             CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>
//                             (BrnTrafficEntityModuleIO.cpp:66).  :381/:441.
//   SceneResultQueue       -> OutputBuffer::OutSmSceneQueryResultsQueue =
//                             CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<...>
//                             (BrnWorldBridgesUnity.cpp:10044).  :389/:442/:648.
//   ResourceRequestInterface = BrnResource::GameDataIO::RequestInterface<8192>
//                             (BrnGameDataEvents.h:240).  RaceCar TU :70/:131.
//
// Size sources:
//   ⚠ CORRECTED 2026-08-19 (wave Q6 C5). This block used to read "the five queue blobs use the
//   documented nominal 256 (NOT byte-verified) ... byte-exact layout is not required, so nominal
//   is intentional per STUB RULE 2". Every word of that is now false: all five carry their real
//   instantiation, each pinned by a named console symbol or a console Construct/memcpy offset
//   span (per-type notes below). MEASURED on the host, 2026-08-19:
//     PotentialContactQueue   163856 / align 16   (== 16 + 2048*80, console-attested span)
//     InputBuffer_PrePhysics  212160 / align 16
//   (scratchpad/waveQ6/probe_rcfork/measure_sizes.cpp -- the sizes MSVC itself prints.)
//   ResourceRequestInterface = 8208 = 8192 + 16: derived from the committed in-tree evidence in
//   BrnGameStateModuleIO.h, where RequestInterface<3072> occupies 0x4024-0x3414 = 3088 = 3072+16
//   (a RequestQueue<N> carries a 16-byte BaseEventQueue-style header: T* + s32 maxLen + s32 len,
//   see CgsBaseEventQueue.h:138-140). Same +16 header => RequestInterface<8192> = 8208.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"           // CgsModule::EventQueue<T,N> (PotentialContactQueue IS this; TakedownEventQueue base)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // VariableEventQueue<32768,16> (SceneResultQueue base)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h" // CgsSceneManager::SceneManagerIO::PotentialContact (PotentialContactQueue element)
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"  // BrnResource::GameDataIO::RequestInterface<8192>
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h" // BrnGameState::TakedownEvent (TakedownEventQueue element)

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    // ---- GameAction queue (InputBuffer_PreScene :257) -------------------------------------
    // ⛔⛔ GROWN 2026-08-01 (reset-player-car wave), and it was a LIVE MEMORY-CORRUPTION HAZARD
    // of exactly the family the physics module's one-byte queue was: this member was a
    // `unsigned char maReserved[256]` NOMINAL blob, while the console's own producer
    //     WorldModule::BridgeActionsToRaceCarModule @0x827ABF40
    //         CgsModule::VariableEventQueue<13312,16>::Append<13312,16>(
    //             InputBuffer_PreScene::GetGameActionQueue(), UpdateInputBuffer::GetGameActionQueue())
    // copies a WHOLE 13328-byte queue into it -- 13072 bytes past the end, straight over
    // meActivePaybackType / meActivePaybackAggressor / mReplayStatusInterface /
    // mAudioCarLoadedDataQueue (this member is in the MIDDLE of the buffer, not at the end).
    // It was invisible for one reason only: that bridge was an inert link stub, so nothing had
    // ever put a game action into the race-car input buffer.
    // The type is pinned by the mangled Append symbol at 0x827ABFC4 (13312,16), not inferred.
    // Derives (rather than typedefs) so existing `struct GameActionQueue` forward references
    // stay valid -- the SceneResultQueue precedent below.
    // ⚠ SAME FORK CLASS AS PotentialContactQueue (collapsed 2026-08-19, see below): the DWARF
    // spells this one a typedef too (BrnRaceCarEntityModuleIO.h:63 ->
    // `typedef InputBuffer::GameActionQueue GameActionQueue`, chaining to
    // BaseGameActionQueue<13312>). It was NOT collapsed here because no consumer in this tree
    // has yet been measured to need the base type by pointer; the day one does, the symptom is
    // a hard C2664, not a silent divergence.
    struct alignas(16) GameActionQueue : public CgsModule::VariableEventQueue<13312, 16>
    {
    };

    // ---- Takedown event queue (InputBuffer_PrePhysics :444) -------------------------------
    // EventQueue<BrnGameState::TakedownEvent, 8> -> alignas(16).
    // ⛔ GROWN 2026-08-11 (WorldBridgeInputToEntityModules mount) -- THE FIFTH
    // 256-BYTE-BLOB-VS-REAL-QUEUE FINDING in this file, and it was about to become live.
    //   InputBuffer_PrePhysics::SetTakedownEventQueue @0x827A98F8 does
    //     *(_DWORD *)(this + 208984) = 0;                        // == member+8 == miLength (Clear)
    //     BrnGameState::TakedownEvent_::Append(this + 208976, src);
    //   i.e. Clear() + the committed BaseEventQueue<TakedownEvent>::Append merge on the MEMBER
    //   itself -- so the member must BE the real fixed-capacity instantiation, not an opaque
    //   blob. The console span pins it exactly: mTakedownEventQueue @+208976 and
    //   mScoringInterface @+209312 (== +212048 - 2736, from SetOnlineScoringInterface's
    //   memcpy target and SetScoringInterface's 2736-byte block), i.e. 336 bytes
    //   == 16 header + 8 * 40 == EventQueue<TakedownEvent,8> -- NOT 256. Appending through
    //   the old blob would have run off the end of the member and over mScoringInterface.
    //   The same EventQueue<BrnGameState::TakedownEvent,8> is already the committed shape of
    //   the producing side (BrnWorldIO::UpdateInputBuffer::TakedownEventQueue), which is the
    //   second independent attestation of the width.
    // Derives (rather than typedefs) so existing `struct TakedownEventQueue` forward
    // references stay valid -- the SceneResultQueue precedent below. Same fork class as
    // GameActionQueue above (DWARF :97 -> `typedef InputBuffer::TakedownEventQueue
    // TakedownEventQueue`, chaining to EventQueue<BrnGameState::TakedownEvent,8>); left forked
    // because no measured consumer needs the base type by pointer.
    struct alignas(16) TakedownEventQueue
        : public CgsModule::EventQueue<BrnGameState::TakedownEvent, 8>
    {
    };

    // ---- Game event queue (OutputBuffer_PrePhysics :491 / OutputBuffer_PostPhysics :602) --
    // World-level EventQueue<BrnGameState::GameEvent, N> -> alignas(16). Grown by its own TU.
    // ⛔ GROWN 2026-08-01 (drivable wave) -- THE FOURTH 256-BYTE-BLOB-VS-REAL-QUEUE FINDING
    // IN THREE WAVES, and it is the LAST member of OutputBuffer_PrePhysics, so the overrun
    // would have run off the end of the buffer.
    //   RaceCarEntityModuleIO::OutputBuffer_PrePhysics::Construct @0x822EA7B0 does
    //     CgsModule::VariableEventQueue<1536,16>::Construct(this + 149312)   [== +0x24740]
    //     CgsModule::VariableEventQueue<1536,16>::Clear    (this + 149312)
    //   and PlaceOnTrackManager::PrePhysicsUpdate @0x822F6DF8 AddEvents an 8-byte type-13
    //   game event into it for the player's car. sizeof is 1552, not 256.
    // The same VariableEventQueue<1536,16> is already the committed shape of the game-state
    // module's own game-event queue (GameBridgeGUIToX.cpp:146), i.e. two independent
    // attestations of the width.
    // Derives (rather than typedefs) so existing `struct GameEventQueue` forward references
    // stay valid -- same move as SceneResultQueue below. (PotentialContactQueue no longer
    // belongs on that list: its fork was collapsed to the DWARF typedef on 2026-08-19.)
    struct alignas(16) GameEventQueue : public CgsModule::VariableEventQueue<1536, 16>
    {
    };

    // ---- Potential-contact queue (InputBuffer_PrePhysics :441) ----------------------------
    // GROWN (was a 256-byte NOMINAL opaque): InputBuffer_PrePhysics::SetPotentialContactQueue
    // (X360 0x827A9840) Clear()s the member queue then Append()s the caller's queue -- both
    // inline-generic BaseEventQueue<T> methods -- so the member must BE the real fixed-cap
    // instantiation, not an opaque blob. Per the DecFIGS DWARF it is
    // CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>
    // (== OutputBuffer::OutPotentialContactQueue; element home CgsPotentialContact.h, the
    // committed EventQueue<PotentialContact,2048> instantiation lives in
    // GameShared/GameClasses/SceneManager/SharedIO/EventQueue_PotentialContact_2048.cpp).
    //
    // ⭐ FORK COLLAPSED 2026-08-19 (wave Q6 cluster C5). This was
    //     struct alignas(16) PotentialContactQueue : public EventQueue<PotentialContact,2048> {};
    // -- a DERIVED struct, i.e. a type distinct from the scene manager's
    // OutputBuffer::OutPotentialContactQueue. The DecFIGS DWARF spells it as a TYPEDEF of
    // exactly that type (BrnRaceCarEntityModuleIO.h:89 ->
    // `typedef OutputBuffer::OutPotentialContactQueue PotentialContactQueue`, where
    // OutputBuffer is CgsSceneManager::SceneManagerIO::OutputBuffer, DWARF
    // CgsSceneManagerModuleIO.h:474). Traffic's twin (:91) is already a direct typedef in this
    // tree (BrnTrafficEntityModuleIO.h:203), which is why its scene bridge landed in wave Q5
    // and the race-car one could not: base -> derived does not convert, so
    //   WorldModule::BridgeSceneContactsToRaceCarModule_PrePhysics @0x827ABBD0
    //     lpRaceCarInput->SetPotentialContactQueue( lpSceneOut->GetPotentialContactQueue() )
    // was a hard error C2664 (measured; scratchpad/waveQ5/f2.owner.md:206-219). The console
    // has ONE type here and hands the pointer straight across with no conversion at all.
    //
    // The reason the old note gave for deriving -- "so existing `struct PotentialContactQueue`
    // forward references stay valid" -- did not hold: grepped the whole tree (src + vendor),
    // there is NO forward declaration of that name anywhere; the only spellings are the
    // typedef at BrnRaceCarEntityModuleIO.h:515 and the member at :571.
    //
    // OFFSET-NEUTRAL, MEASURED (scratchpad/waveQ6/probe_rcfork/measure_sizes.cpp, MSVC
    // diagnostic, before and after): the derived struct and the typedef target are BOTH
    // sizeof 163856 (== 16 + 2048*80) / alignof 16, and InputBuffer_PrePhysics stays
    // sizeof 212160 / alignof 16. The `alignas(16)` the struct carried was redundant -- the
    // element PotentialContact is itself `alignas(16)` (CgsPotentialContact.h:38), so the
    // inline maEvents array already forces 16 on the queue.
    typedef CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>
            PotentialContactQueue;                                                   // DWARF :89

    // ---- Scene-query-results queue (InputBuffer_PrePhysics :442 / GenerateDispatch :648) --
    // GROWN (was a 256-byte NOMINAL opaque): the scene->race-car bridge
    // BridgeSceneQueryResultsToRaceCarModule_PrePhysics @0x827ABB44 Appends the scene
    // manager's queue into this member through the REAL instantiated symbol
    // CgsModule::VariableEventQueue<32768,16>::Append<32768,16>(const VariableEventQueue
    // <32768,16>&) -- pinning the type. Derives (rather than typedefs) so existing
    // `struct SceneResultQueue` forward references stay valid.
    struct alignas(16) SceneResultQueue : public CgsModule::VariableEventQueue<32768, 16>
    {
    };

    // ---- Resource-request interface (OutputBuffer_Prepare :131 / PostPhysics :595) --------
    // = BrnResource::GameDataIO::RequestInterface<8192>. The inline request ring is SIMD-aligned
    // on X360 -> alignas(16).
    //
    // REAL as of the race-car global-resource wave: the sized-blob stand-in is retired.
    // RaceCarEntityModule::LoadGlobalResources @0x82300730 calls
    // RequestInterface<8192>::LoadBundle / ::GetVehicleList / ::GetWheelList straight on
    // this member and raw-AddEvents an AcquireResourceRequest (type 4) for "CarColours",
    // so it must be the live queue, not reserved bytes. Derives (rather than typedefs) so
    // the existing `struct ResourceRequestInterface` forward references stay valid, exactly
    // like SceneResultQueue above; RequestInterface<8192> holds its RequestQueue<8192> at
    // offset 0, so WorldModule::BridgeRaceCarResourceRequestsToOutput_Prepare's
    // reinterpret_cast to VariableEventQueue<8192,16> keeps working unchanged.
    struct alignas(16) ResourceRequestInterface
        : public BrnResource::GameDataIO::RequestInterface<8192>
    {
    };
}
}
