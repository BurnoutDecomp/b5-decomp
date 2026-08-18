// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h
//
// "BrnWorld::RaceCarEntityModuleIO" is a NAMESPACE. The ledger functions are inline
// IOBuffer accessors spread across 10 per-frame IO-buffer structs (each derives
// CgsModule::IOBuffer), plus 2 per-index element accessors on the output interfaces
// (homed in BrnRaceCarEntityModuleOutputInterface.h) and 3 EventQueue template
// instantiations for the traffic-system events (homed in BrnRaceCarToTrafficInterface.h).
// Layout/signatures are X360-gated against the FULL DecFIGS DWARF for
// BrnRaceCarEntityModuleIO.h (buffers laid out at DWARF lines 118..652) and the two
// SharedIO headers.
//
// ACCESSOR SHAPE (X360 binary, authoritative): every Get*Interface()/Get*Queue() body
// asserts the buffer's lock bit then returns &member. Lock bit -> const-ness:
//   ">>3 &1" (eStatusLockedForWrite 0x08) => NON-const mutable getter;
//   ">>4 &1" (eStatusLockedForRead  0x10) => CONST getter.
// Offsets are NOT hardcoded -- bodies return &member, which lands the right byte offset
// once each sub-interface payload carries its real size in its own TU.
//
// SUB-INTERFACE PAYLOADS (this assemble pass): the ~27 sub-interface/queue payloads
// embedded BY VALUE are now COMPLETE types living in their CANONICAL homes/namespaces
// (the stub phase landed minimal-complete sized slices per the committed
// CheckpointData/OpponentData pattern). This header therefore #includes those homes and
// references each payload by its canonical fully-qualified name (per-buffer typedefs are
// retargeted to the canonical FQN, member types unchanged in spelling). The two
// BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_* payloads had no stub group, so a
// minimal-complete local slice is defined inline below (see RISK note in the assemble
// report); they are accessed only by-name, so byte-exact size is not required.
//
// CORRECTIONS baked in (verified punch-list):
//  (1) X360 0x8279E310 is the NON-const GetSceneInputInterface() of InputBuffer_PostPhysics
//      (write-lock; returns &mSceneInputInterface), NOT a non-const GetContactSpyInterface.
//  (2) There is NO non-const GetContactSpyInterface: InputBuffer_PostPhysics declares only
//      the const getter (:528) + SetContactSpyInterface (:529).
//  (3) EActiveRaceCarEngineState uses the full DWARF enumerators (in the OutputInterface header).
#pragma once

#include "types.hpp"
#include <cstring>   // memset (partial-slice Constructs)                                                       // s8/s32/u8/u16/u32/f32
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT (per-race-car header-inline setters)
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                      // CgsModule::IOBuffer base
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"    // CgsSystem::TimerStatusInterface (by value)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                    // CgsModule::EventQueue<T,N>
#include "BrnCommonTypes.h"                                                 // CgsID, Vector3, Vector4
#include "GameSource/BurnoutConstants.h"                                    // EActiveRaceCarIndex, EGlobalRaceCarIndex
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntity* interfaces, AudioCarDataLoadedEvent
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarToTrafficInterface.h"          // RaceCarToTrafficInterface + Create/Remove Rival events

// ---- Canonical homes of the sub-interface/queue payloads embedded by value ----------
// (Each was reconstructed as a minimal-complete sized slice in its DWARF home + namespace.
//  These replace the .ref's local namespace-scope forward declarations.)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"          // BrnPhysics::Vehicle::VehicleInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"    // BrnPhysics::Vehicle::VehicleDriverInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"         // BrnPhysics::Vehicle::VehicleOutputInterface + VehicleManagerOutputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEffectsInputInterface.h"   // BrnPhysics::Vehicle::VehicleEffectsInputInterface
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleRaceCarIOInterfaces.h"      // BrnWorld::CrashIO::RaceCarOutputInterface (CrashInterface)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h" // BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene (canonical 544B)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h" // BrnWorld::PlayerVehicleControls
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"                          // BrnAI::AIModuleIO::RaceCarAIInterface + AIRaceCarInterface
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"                     // BrnAI::AIModuleIO::AIModuleRequestInterface
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"                      // BrnAI::AIModuleIO::AIModuleResultInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"            // CgsSceneManager::SceneManagerIO::InSceneUpdateInterface (SceneInputInterface)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQuery.h"            // CgsSceneManager::SceneManagerIO::SceneCoarseQueryQueue
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"                  // CgsSceneManager::SceneManagerIO::SceneFineLineTestQueue
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // BrnPhysics::Deformation::DeformationOutputInterface (+ ...ForEntityModules)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                    // BrnGameState::GameStateModuleIO::ScoringOutputInterface + OnlineScoringOutputInterface
#include "GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.h"                // BrnDirector::BrnDirectorVehicleInputInterface
#include "GameSource/Replays/BrnReplayStatusInterface.h"                                  // BrnReplays::ReplayIO::StatusInterface (ReplayStatusInterface)
#include "GameSource/Director/Camera/Camera.h"                                            // BrnDirector::Camera::Camera
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"                       // BrnPhysics::ContactSpy::ContactSpyInterface
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleIOQueues.h" // local queue slices (GameActionQueue/GameEventQueue/PotentialContactQueue/SceneResultQueue/TakedownEventQueue/ResourceRequestInterface)

// EPaybackType used to be an opaque `enum EPaybackType : s32;` forward declaration here, which
// forced the "no dirty trick" sentinel in InputBuffer_PreScene::Construct to be spelled as a
// static_cast of the literal 3. Include the real home instead so the enumerator can be named.
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::EPaybackType (:32)
namespace CgsGraphics { class DispatchFrame; }   // class (matches CgsDispatcher.h:211; struct fwd-decl skewed MSVC mangling)
// RECONCILED 2026-07-24 (ODR fix, see BrnRendererModuleIO.h): BrnBlobbyShadowBuffer
// is a NESTED class of the real BrnBlobbyShadowManager CLASS -- include the home.
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"
// RECONCILED 2026-07-24 (ODR fix, see BrnRendererModuleIO.h): BrnSubmissionInterface is a
// NESTED class of the real BrnCoronaManager CLASS. The partial-class spelling that used to
// stand here re-declared the enclosing class and collided (C2011) with the real definition
// in every TU that saw both -- include the real home instead.
#include "GameSource/Graphics/BrnCoronaManager.h"   // BrnCoronaManager::BrnSubmissionInterface

namespace BrnWorld
{
struct ShadowMap;                                                          // :652 mpShadowMap target

namespace RaceCarEntityModuleIO
{
    // ---- Remaining locally-homed payloads -----------------------------------------
    // TimerStatusInterface (:254 member type): the DecFIGS DWARF for InputBuffer_PreScene
    // (BrnRaceCarEntityModuleIO.h:53) declares `TimerStatusInterface mTimerStatusInterface;`
    // -- a BY-VALUE member, NOT a pointer. The X360 getter (0x822B4A38) does
    // `addi r3, this, 0x5C` (returns &member), and the by-value member pins mCameraInput at
    // +0x90 (0x5C + 48, Camera 16-byte aligned); a pointer member would instead put
    // mCameraInput at +0x60, contradicting the 0x822B4AE0 getter's `addi r3, this, 0x90`.
    // It is not in the canonical type-map (its DWARF home was not dumped), so a
    // minimal-complete 48-byte slice is defined here in this namespace, mirroring the X360
    // CgsSystem::TimerStatusInterface payload (two 24-byte TimerStatus blocks, game then sim).
    // Accessed only by-name across homes, so the exact field spelling is not load-bearing;
    // the 48-byte size IS (it fixes mCameraInput's offset).
    // REAL as of the race-car streamer wave (2026-07-31): the 48-byte reserved blob is
    // retired in favour of the engine's own CgsSystem::TimerStatusInterface, which is
    // exactly the same 48 bytes (two 24-byte TimerStatus blocks, game then sim, align 4)
    // and carries the member NAMES. RaceCarEntityModule::PreSceneUpdate @0x8230D928 reads
    // `*(iface+28) * *(iface+32)` for the frame's sim step -- i.e. the SIM block's
    // mfBaseTimeStep * mfTimeStepMultiplier -- and with the blob that could only have been
    // spelled as a raw offset poke. Type alias, so the FQN and the 48-byte footprint (which
    // pins mCameraInput at +0x90) are unchanged.
    typedef CgsSystem::TimerStatusInterface TimerStatusInterface;   // :254 (by-value, 48B)

    // AudioCarLoadedDataQueue (homed in BrnRaceCarEntityModuleOutputInterface.h via the
    // AudioCarDataLoadedEvent element, which is in this namespace): EventQueue<...,16>.
    typedef CgsModule::EventQueue<AudioCarDataLoadedEvent, 16> AudioCarLoadedDataQueue; // OutputInterface.h

    // ---- BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_* --------------------------
    // TrafficToRaceCarInterface_PreScene now has a canonical byte-exact (544B) definition in
    // GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h
    // (#included above), so the earlier inline 256-byte slice is deleted here to avoid an ODR
    // duplicate of that FQN and to make SetTrafficToRaceCarInterface_PreScene's memcpy copy the
    // full 0x220 (544) bytes at member offset +0xC0. TrafficToRaceCarInterface_PostScene has NO
    // canonical home yet (the traffic ledger only landed the PreScene type), so its DWARF-faithful
    // 1-byte muDUMMY slice remains defined inline below; it is embedded BY VALUE
    // (mTrafficToRaceCarInterface_PostScene :447) and accessed only by-name.
}
}

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout reconstructed
    // by its own TU (DWARF home BrnTrafficToRaceCarInterface.h:175). Size 1 (DWARF-faithful:
    // the real type is a single uint8_t muDUMMY placeholder, BrnTrafficToRaceCarInterface.h:187).
    // Natural alignment: no Vector*/Matrix*/SIMD/EventQueue member.
    struct TrafficToRaceCarInterface_PostScene
    {
        u8 muDUMMY;   // BrnTrafficToRaceCarInterface.h:187
    };
}
}

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    // ============================================================================
    // OutputBuffer_Prepare  (DWARF :118)
    // ============================================================================
    struct OutputBuffer_Prepare : public CgsModule::IOBuffer
    {
        typedef RaceCarEntityModuleIO::ResourceRequestInterface ResourceRequestInterface;  // :70
        // X360 0x822EA380 -- IOBuffer status then VariableEventQueue<8192,16>::Construct +
        // ::Clear on the embedded request ring. Now REAL: ResourceRequestInterface is the
        // live BrnResource::GameDataIO::RequestInterface<8192>, so the ring is brought up
        // by its own Construct/Clear exactly as the console does (the previous zero-fill
        // stood in for it while the member was a reserved-byte blob).
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mResourceRequestInterface.mRequestQueue.Construct();
            mResourceRequestInterface.mRequestQueue.Clear();
        }
        const ResourceRequestInterface* GetResourceRequestInterface() const;              // :126 R  (0x8279CDF0)
        ResourceRequestInterface*       GetResourceRequestInterface();                     // :127 W  (0x822B4990)
    private:
        ResourceRequestInterface mResourceRequestInterface;                                // :131
    };

    // ============================================================================
    // InputBuffer_PreScene  (DWARF :146)  (0x8279D060 W GetGameActionQueue, assert :164)
    // ============================================================================
    struct InputBuffer_PreScene : public CgsModule::IOBuffer
    {
        typedef RaceCarEntityModuleIO::GameActionQueue          GameActionQueue;           // :98
        typedef BrnReplays::ReplayIO::StatusInterface           ReplayStatusInterface;     // :106
        typedef RaceCarEntityModuleIO::AudioCarLoadedDataQueue  AudioCarLoadedDataQueue;   // OutputInterface.h

        // X360 0x822EA3C0 -- IOBuffer status, TimerStatusInterface::Clear(+92),
        // Camera::Construct(+144), the seven-slot 0x700000000 seed, VariableEventQueue<13312,16>
        // ::Construct(+556) [== mGameActionQueue], the payback/replay scalars, then
        // EventQueue<AudioCarDataLoadedEvent,16>::Construct(+15456) [== mAudioCarLoadedDataQueue]
        // and the eight-slot per-car clear.
        // PARTIAL SLICE: the three members whose committed types expose Construct/Clear run the
        // REAL call; the rest belong to their own TUs [marked deviation]. This replaces the
        // WorldLinkStubs base-only gate, which left both queues un-Constructed --
        // RaceCarAudioStreamer::Update reads mAudioCarLoadedDataQueue every frame (it fired
        // "mpEvents != NULL" + "Base event queue overflow"), and PreSceneUpdate reads the timer
        // status interface for the frame's sim step.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mTimerStatusInterface.Clear();
            // The console's VariableEventQueue<13312,16>::Construct(+556). REAL as of the
            // reset-player-car wave (the member is the live queue now, not a 256-byte blob):
            // without it the first game action delivered by BridgeActionsToRaceCarModule fires
            // the "Not Constructed" pair (CgsVariableEventQueue.h:454 then :728) -- the exact
            // pair the world input buffers fired one wave ago.
            mGameActionQueue.Construct();
            mAudioCarLoadedDataQueue.Construct();
            // ⭐⭐ 2026-08-15 (IO-buffer zero-fill removal audit) -- THE MISSING STORES.
            // Everything below is in the console Construct @0x822EA3C0 and was omitted here;
            // it only ever worked because the old PC CreateIOBuffer<T> value-initialised
            // (zero-filled) the whole buffer. With default-init these members arrive holding
            // the previous IO-stack tenant's bytes, and the per-car "valid"/"this frame"
            // latches below are pure read-if-set flags -- a stale 1 makes the pre-scene pass
            // act on a colour/paint/select/contact event that never happened.
            //   *(this+0x363C) = 3   -- the EPaybackType sentinel: literal 3 IS the named
            //                           enumerator E_PAYBACK_TYPE_SIX_AXIS_STEERING
            //                           (BrnNetworkSharedIO.h:32), the last real payback type and
            //                           the value the world twin stores too (BrnWorldModuleIO.cpp).
            //                           (It is EPaybackType, NOT EDirtyTrickStatus -- an earlier
            //                           note here named the wrong enum.)
            //   *(this+0x3640) = -1
            //   the replay-status seed at +0x3644 (flag word, six reel head bytes,
            //   the two current-reel indices = -1, the trailing debug alpha = 0.0)
            //   the eight-iteration per-active-race-car clear over the nine leading arrays
            //   (stores at +34/+42/+2/+50/+18/+58/+66/+74/+82, 0x822EA480..0x822EA4A4)
            meActivePaybackType      = BrnNetwork::E_PAYBACK_TYPE_SIX_AXIS_STEERING;   // == 3
            meActivePaybackAggressor = E_ACTIVE_RACE_CAR_INDEX_INVALID;
            mReplayStatusInterface.mxStatusFlags = 0;
            for (s32 liReel = 0; liReel < 6; ++liReel)
                mReplayStatusInterface.maReels[liReel].macName[0] = '\0';
            mReplayStatusInterface.miCurrentRecordReel   = -1;
            mReplayStatusInterface.miCurrentPlaybackReel = -1;
            mReplayStatusInterface.mfDebugHudAlpha       = 0.0f;
            // The range-guarded EActiveRaceCarIndex operator++ is what bakes the
            // "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" assert (BurnoutConstants.h:39)
            // the X360 body carries -- same shape as the world twin's trailing loop.
            for (EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0;
                 leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
                 leIndex++)
            {
                mabReceivedNetworkDriverControls[leIndex] = false;
                mabRaceCarColourIndexValid[leIndex]       = false;
                mau16RaceCarColourIndex[leIndex]          = 0;
                mabRaceCarPaintFinishIndexValid[leIndex]  = false;
                mau16RaceCarPaintFinishIndex[leIndex]     = 0;
                mabLostContactThisFrame[leIndex]          = false;
                mabRegainedContactThisFrame[leIndex]      = false;
                mabCarSelectStatus[leIndex]               = false;
                mabCarSelectStatusValid[leIndex]          = false;
            }
            // [FLAG] still NOT emitted (their own TUs own the bring-up): the console's
            // Camera::Construct(+144) on mCameraInput, and the 60-byte mPlayerVehicleControls
            // seed at +496 (BrnWorld::PlayerVehicleControls::Construct is declared but has NO
            // body in this tree yet -- calling it would be an unresolved external).
            // ⚠️ CORRECTED 2026-08-15: that seed is NOT a zero-fill. X360 0x822EA3C0 runs a
            // 7-iteration loop storing the 64-bit literal 0x700000000 at +496 and then one
            // trailing 32-bit 0 -- i.e. the 60 bytes come out as the dword pattern
            // 7,0,7,0,7,0,7,0,7,0,7,0,7,0,0, seven `7`s interleaved with zeros. Whatever
            // PlayerVehicleControls' first-of-each-pair field is, its idle value is 7, not 0,
            // so a memset here would be actively wrong -- which is exactly why this stays a
            // FLAG until PlayerVehicleControls::Construct is bodied.
        }
        const TimerStatusInterface*    GetTimerStatusInterface() const;                    // :154
        void                           SetTimerStatusInterface(const TimerStatusInterface*); // :155
        const BrnDirector::Camera::Camera* GetCameraInput() const;                         // :157
        void                           SetCameraInput(const BrnDirector::Camera::Camera*); // :158
        const BrnWorld::PlayerVehicleControls* GetPlayerVehicleControls() const;           // :160
        void                           SetPlayerVehicleControls(const BrnWorld::PlayerVehicleControls*); // :161
        const GameActionQueue*         GetGameActionQueue() const;                         // :163
        GameActionQueue*               GetGameActionQueue();                               // :164 W  (0x8279D060)
        BrnNetwork::EPaybackType       GetActivePaybackType() const;                       // :166
        void                           SetActivePaybackType(BrnNetwork::EPaybackType);     // :167
        EActiveRaceCarIndex            GetActivePaybackAggressor() const;                  // :169
        void                           SetActivePaybackAggressor(EActiveRaceCarIndex);     // :170
        const ReplayStatusInterface*   GetReplayStatusInterface() const;                   // :173
        void                           SetReplayStatusInterface(const ReplayStatusInterface*); // :174
        AudioCarLoadedDataQueue*       GetAudioCarLoadedDataQueue();                        // :176
        const AudioCarLoadedDataQueue* GetAudioCarLoadedDataQueue() const;                 // :177
        bool                           GetReceivedNetworkDriverControls(EActiveRaceCarIndex) const; // :181

        // ---- per-active-race-car latch SETTERS: X360 HEADER-INLINES -------------------
        // (bodied 2026-08-11, WorldBridgeInputToEntityModules mount). These six carry no
        // out-of-line symbol in the ARTIST image: WorldModule::BridgeInputToEntityModules
        // @0x827ADF88 has NO `bl` for them -- it inlines each one, and the inlined copies
        // are exactly the range-assert pair + the store(s) reproduced below, with the
        // X360-baked __LINE__ of THIS header on every assert (705/706, 760/761, 808/809,
        // 826/827, 845/846). The store offsets in that inlined code pin the leading arrays
        // one-for-one against the member list below (colour@+2, paint@+18, received@+34,
        // colourValid@+42, paintValid@+50, lost@+58, regained@+66, select@+74,
        // selectValid@+82). Modelled as header inlines to match the console, exactly as the
        // producing side's twins are in BrnWorldModuleIO.h.
        //
        // NOTE on the assert shape: unlike the WORLD buffer's combined-form guards, the
        // race-car side fires TWO SEPARATE asserts on every one of these (>= _INDEX_0 then
        // < _INDEX_COUNT), each its own BeginAssert/FireAssert/EndAssert in the asm --
        // including SetLostContact/SetRegainedContact, where the world twin uses one
        // combined assert. Reproduced faithfully.

        // :185 -- latch "this car's network driver controls arrived this frame".
        // The only producer is WorldModule::CheckForNetworkDriverControlsReceived
        // @0x827A8C58 (a HOLE in the .ida-exports dump; the store `racecar byte[34 + idx]
        // = 1` is the PS3-asm-attested shape recorded on the bridge's own reconstruction),
        // and +34 is mabReceivedNetworkDriverControls -- the offset the surrounding
        // inlined per-car block independently pins.
        void SetReceivedNetworkDriverControls(EActiveRaceCarIndex leActiveRaceCarIndex)
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            mabReceivedNetworkDriverControls[leActiveRaceCarIndex] = true;
        }
        // :190 -- X360 h:705/:706; stores the u16 at +2+2*idx then the valid flag at +42+idx.
        void SetRaceCarColourIndex(EActiveRaceCarIndex leActiveRaceCarIndex, u16 lu16ColourIndex)
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            mau16RaceCarColourIndex[leActiveRaceCarIndex]    = lu16ColourIndex;
            mabRaceCarColourIndexValid[leActiveRaceCarIndex] = true;
        }
        u16                            GetRaceCarColourIndex(EActiveRaceCarIndex) const;   // :194
        bool                           IsRaceCarColourIndexValid(EActiveRaceCarIndex) const; // :198
        // :203 -- X360 h:760/:761; stores the u16 at +18+2*idx then the valid flag at +50+idx.
        void SetRaceCarPaintFinishIndex(EActiveRaceCarIndex leActiveRaceCarIndex, u16 lu16PaintIndex)
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            mau16RaceCarPaintFinishIndex[leActiveRaceCarIndex]    = lu16PaintIndex;
            mabRaceCarPaintFinishIndexValid[leActiveRaceCarIndex] = true;
        }
        u16                            GetRaceCarPaintFinishIndex(EActiveRaceCarIndex) const; // :207
        bool                           IsRaceCarPaintFinishIndexValid(EActiveRaceCarIndex) const; // :211
        // :215 -- X360 h:808/:809; `a5[idx + 58] = 1`.
        void SetLostContact(EActiveRaceCarIndex leActiveRaceCarIndex)
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            mabLostContactThisFrame[leActiveRaceCarIndex] = true;
        }
        bool                           GetLostContact(EActiveRaceCarIndex) const;          // :219
        // :223 -- X360 h:826/:827; `a5[idx + 66] = 1`.
        void SetRegainedContact(EActiveRaceCarIndex leActiveRaceCarIndex)
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            mabRegainedContactThisFrame[leActiveRaceCarIndex] = true;
        }
        bool                           GetRegainedContact(EActiveRaceCarIndex) const;      // :227
        // :232 -- X360 h:845/:846; `a5[idx + 74] = status` then `a5[idx + 82] = 1`.
        void SetCarSelectStatus(EActiveRaceCarIndex leActiveRaceCarIndex, bool lbStatus)
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            mabCarSelectStatus[leActiveRaceCarIndex]      = lbStatus;
            mabCarSelectStatusValid[leActiveRaceCarIndex] = true;
        }
        bool                           GetCarSelectStatus(EActiveRaceCarIndex) const;      // :236
        bool                           IsCarSelectStatusValid(EActiveRaceCarIndex) const;  // :240
    private:
        u16                     mau16RaceCarColourIndex[8];                                // :244
        u16                     mau16RaceCarPaintFinishIndex[8];                           // :245
        bool                    mabReceivedNetworkDriverControls[8];                       // :246
        bool                    mabRaceCarColourIndexValid[8];                             // :247
        bool                    mabRaceCarPaintFinishIndexValid[8];                        // :248
        bool                    mabLostContactThisFrame[8];                                // :249
        bool                    mabRegainedContactThisFrame[8];                            // :250
        bool                    mabCarSelectStatus[8];                                     // :251
        bool                    mabCarSelectStatusValid[8];                                // :252
        TimerStatusInterface    mTimerStatusInterface;                                     // :254 (BY VALUE; DWARF :53, &member @+0x5C)
        BrnDirector::Camera::Camera mCameraInput;                                          // :255 (@+0x90)
        BrnWorld::PlayerVehicleControls mPlayerVehicleControls;                            // :256
        GameActionQueue         mGameActionQueue;                                          // :257  (0x8279D060 -> this)
        BrnNetwork::EPaybackType meActivePaybackType;                                      // :258
        EActiveRaceCarIndex     meActivePaybackAggressor;                                  // :259
        ReplayStatusInterface   mReplayStatusInterface;                                    // :260 (BY VALUE; DWARF :79, operator=-assigned @+0x3644)
        AudioCarLoadedDataQueue mAudioCarLoadedDataQueue;                                  // :261
    };

    // ============================================================================
    // OutputBuffer_PreScene  (DWARF :274)
    // ============================================================================
    struct OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
        typedef BrnPhysics::Vehicle::VehicleInputInterface          VehicleInputInterface; // :72
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface; // :71
        typedef BrnAI::AIModuleIO::RaceCarAIInterface               RaceCarAIInterface;     // :81
        typedef RaceCarEntityModuleIO::AudioCarLoadedDataQueue      AudioCarLoadedDataQueue; // OutputInterface.h

        // X360 0x822EA4E0 -- IOBuffer status, VehicleInputInterface::Construct(+16),
        // InSceneUpdateInterface::Construct(+142192), the four RCEntity*OutputInterface::Clear
        // calls, a nine-slot zero fill, VariableEventQueue<16384,16>::Construct(+987512), then
        // EventQueue<AudioCarDataLoadedEvent,16>::Construct(+1004120)
        // [== mAudioCarLoadedDataQueue] and the eight-slot per-car clear.
        // PARTIAL SLICE for the same reason as the InputBuffer twin above: the audio streamer
        // appends its per-frame (un)load requests into this queue every frame and the base-only
        // WorldLinkStubs gate left it un-Constructed. GROW as the other members' types land.
        // ⭐⭐ REORDERED + COMPLETED 2026-08-15 (IO-buffer zero-fill removal audit). The body is
        // now in the console's own call order, and THE FOUR RCEntity*OutputInterface::Clear CALLS
        // THE BANNER ABOVE ALWAYS TRANSCRIBED ARE ACTUALLY MADE. They were listed and never
        // emitted; that only ever worked because the old PC CreateIOBuffer<T> value-initialised
        // the whole buffer. With default-init the four interfaces arrive holding the previous
        // IO-stack tenant's bytes -- and these are the per-car "is active"/"is in current mode"/
        // per-index state tables the pre-scene pass publishes, so a stale byte re-animates a car
        // that is not in the race. RCEntityActiveRaceCarOutputInterface::Clear had ZERO callers
        // in this tree before this change (BrnRCEntityActiveRaceCarOutputInterface.cpp:151).
        void Construct()
        {
            CgsModule::IOBuffer::Construct();                 // X360 *a1 = 1
            // The console's own list names VehicleInputInterface::Construct(+16) FIRST and
            // the body never made it -- same omission as the OutputBuffer_PrePhysics twin
            // below (drivable wave 2026-08-01). Unreached today (nothing posts into the
            // PRE-SCENE copy of the interface) but it is the same fifteen un-Constructed
            // queues, so it is made here rather than left as a latent trap.
            mVehicleInputInterface.Construct();               // X360 +16
            // The console's InSceneUpdateInterface::Construct(+142192) on mSceneInputInterface.
            // Emitted as of cars step 1c (2026-08-17): the body exists (CgsSceneManagerIO_SceneUpdate
            // .cpp:276, X360 0x822E6550, 25 queue Constructs) and the ghost-car fix's
            // ActiveRaceCar::RemoveFromScene now posts into this very copy of the interface --
            // the first boot without this line died on "mpEvents != NULL" /
            // "mRemoveVolumeInstanceQueue too small" (never-Constructed queue, the IO-buffer trap).
            mSceneInputInterface.Construct();                 // X360 +142192
            // ---- the four Clears, in the console's order (0x822EA4E0) ----
            mActiveRaceCarOutputInterface.Clear();            // X360 +960960
            mGlobalRaceCarOutputInterface.Clear();            // X360 +971440
            mReplayActiveRaceCarOutputInterface.Clear();      // X360 +973856
            mReplayGlobalRaceCarOutputInterface.Clear();      // X360 +984336
            // The console's nine-slot zero fill at +987440..+987504 lands inside
            // mRaceCarAIInterface, ahead of its management queue; those words belong to that
            // interface's own bring-up [FLAG, unchanged from the previous body].
            // The console's VariableEventQueue<16384,16>::Construct(+987512). REAL as of the
            // reset-player-car wave: that offset is mRaceCarAIInterface.mManagementQueue (the
            // interface's own +0x2F8), and RaceCarEntityModule::SpawnRaceCar AddEvents an
            // AttachAIControlEvent into it for every car it spawns. The transcribed console
            // list above named this call and the body never made it -- the first spawn would
            // have fired the "Not Constructed" pair (CgsVariableEventQueue.h:454 / :728).
            mRaceCarAIInterface.mManagementQueue.Construct(); // X360 +987512
            mAudioCarLoadedDataQueue.Construct();             // X360 +1004120
            mbRequestingRivalUpdate = false;                  // X360 +1004112
            // [FLAG] the console's trailing eight-slot per-car clear at +1004520 (with the
            // EActiveRaceCarIndex range assert, BurnoutConstants.h:39) is past this buffer's
            // modelled member list -- it lands inside mAudioCarLoadedDataQueue's span on the
            // console offsets, which the host layout does not reproduce byte-for-byte. Left out
            // rather than poked by offset.
        }
        const VehicleInputInterface* GetVehicleInputInterface() const;                      // :282
        VehicleInputInterface*       GetVehicleInputInterface();                            // :283 W  (0x822B4ED0)
        const SceneInputInterface*   GetSceneInputInterface() const;                        // :285
        SceneInputInterface*         GetSceneInputInterface();                              // :286 W  (0x822B4F78)
        const RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const;        // :288
        RCEntityActiveRaceCarOutputInterface*       GetActiveRaceCarOutputInterface();              // :289
        const RCEntityGlobalRaceCarOutputInterface* GetGlobalRaceCarOutputInterface() const;        // :291
        RCEntityGlobalRaceCarOutputInterface*       GetGlobalRaceCarOutputInterface();              // :292
        const RCEntityActiveRaceCarOutputInterface* GetReplayActiveRaceCarOutputInterface() const;  // :294
        RCEntityActiveRaceCarOutputInterface*       GetReplayActiveRaceCarOutputInterface();        // :295
        const RCEntityGlobalRaceCarOutputInterface* GetReplayGlobalRaceCarOutputInterface() const;  // :297
        RCEntityGlobalRaceCarOutputInterface*       GetReplayGlobalRaceCarOutputInterface();        // :298
        const RaceCarAIInterface*    GetRaceCarAIInterface() const;                          // :300 R  (0x8279D6F8)
        RaceCarAIInterface*          GetRaceCarAIInterface();                                // :301
        bool                         IsRequestingRivalUpdate() const;                       // :303
        void                         SetRequestingRivalUpdate(bool);                        // :304
        AudioCarLoadedDataQueue*       GetAudioCarLoadedDataQueue();                         // :306
        const AudioCarLoadedDataQueue* GetAudioCarLoadedDataQueue() const;                  // :307
    private:
        VehicleInputInterface                mVehicleInputInterface;                        // :311
        SceneInputInterface                  mSceneInputInterface;                          // :312
        RCEntityActiveRaceCarOutputInterface mActiveRaceCarOutputInterface;                 // :313
        RCEntityGlobalRaceCarOutputInterface mGlobalRaceCarOutputInterface;                 // :314
        RCEntityActiveRaceCarOutputInterface mReplayActiveRaceCarOutputInterface;           // :315
        RCEntityGlobalRaceCarOutputInterface mReplayGlobalRaceCarOutputInterface;           // :316
        RaceCarAIInterface                   mRaceCarAIInterface;                           // :317
        bool                                 mbRequestingRivalUpdate;                       // :318
        AudioCarLoadedDataQueue              mAudioCarLoadedDataQueue;                      // :320
    };

    // ============================================================================
    // InputBuffer_PostScene  (DWARF :335)  (0x822B5410 R GetTrafficToRaceCarInterface_PreScene)
    // ============================================================================
    struct InputBuffer_PostScene : public CgsModule::IOBuffer
    {
        typedef BrnWorld::CrashIO::RaceCarOutputInterface                CrashInterface;                   // :92
        typedef BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene TrafficToRaceCarInterface_PreScene; // :94
        void Construct();                                                                  // :340
        const CrashInterface* GetCrashInterface() const;                                   // :343
        void                  SetCrashInterface(const CrashInterface*);                    // :344
        const TrafficToRaceCarInterface_PreScene* GetTrafficToRaceCarInterface_PreScene() const; // :346 R (0x822B5410)
        void                  SetTrafficToRaceCarInterface_PreScene(const TrafficToRaceCarInterface_PreScene*); // :347
    private:
        CrashInterface                     mCrashInterface;                               // :351
        TrafficToRaceCarInterface_PreScene mTrafficToRaceCarInterface_PreScene;           // :352
    };

    // ============================================================================
    // OutputBuffer_PostScene  (DWARF :365)
    // ============================================================================
    struct OutputBuffer_PostScene : public CgsModule::IOBuffer
    {
        typedef CgsSceneManager::SceneManagerIO::SceneCoarseQueryQueue  SceneCoarseQueryQueue;  // :77
        typedef CgsSceneManager::SceneManagerIO::SceneFineLineTestQueue SceneFineLineTestQueue; // :78
        typedef BrnAI::AIModuleIO::AIModuleRequestInterface             AIModuleRequestInterface; // :83
        void Construct();                                                                  // :370
        const SceneCoarseQueryQueue*  GetSceneCoarseQueryQueue() const;                    // :373
        SceneCoarseQueryQueue*        GetSceneCoarseQueryQueue();                          // :374
        const SceneFineLineTestQueue* GetSceneFineLineTestQueue() const;                   // :376
        SceneFineLineTestQueue*       GetSceneFineLineTestQueue();                         // :377
        const AIModuleRequestInterface* GetAIModuleRequestInterface() const;               // :379
        AIModuleRequestInterface*       GetAIModuleRequestInterface();                     // :380 W (0x822B5608)
        const RaceCarToTrafficInterface* GetRaceCarToTrafficInterface() const;             // :382
        RaceCarToTrafficInterface*       GetRaceCarToTrafficInterface();                   // :383 W (0x822B56B0)
    private:
        SceneCoarseQueryQueue     mSceneCoarseQueryQueue;                                  // :387
        SceneFineLineTestQueue    mSceneFineLineTestQueue;                                 // :388
        AIModuleRequestInterface  mAIModuleRequestInterface;                              // :389
        RaceCarToTrafficInterface mRaceCarToTrafficInterface;                             // :390
    };

    // ============================================================================
    // InputBuffer_PrePhysics  (DWARF :404)
    // ============================================================================
    struct InputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
        typedef RaceCarEntityModuleIO::PotentialContactQueue          PotentialContactQueue;       // :89
        typedef RaceCarEntityModuleIO::SceneResultQueue               SceneResultQueue;            // :79
        typedef BrnAI::AIModuleIO::AIModuleResultInterface            AIModuleResultInterface;     // :84
        typedef RaceCarEntityModuleIO::TakedownEventQueue             TakedownEventQueue;          // :97
        typedef BrnGameState::GameStateModuleIO::ScoringOutputInterface       ScoringInterface;       // :100
        typedef BrnGameState::GameStateModuleIO::OnlineScoringOutputInterface OnlineScoringInterface; // :101
        typedef BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PostScene TrafficToRaceCarInterface_PostScene; // :95
        // X360 0x822EA6F0 -- IOBuffer status, then PotentialContact<2048>::Construct(+16),
        // VariableEventQueue<32768,16>::Construct(+163872) [== mSceneResultQueue],
        // ResetOnTrackResult<128>/PlaceOnTrackRequest<128> (inside the AI result interface),
        // TakedownEvent<8>::Construct(+208976), memset(scoring, 0, 2736), the eight-word
        // online-scoring block seeded to -1 and the two trailing flag bytes cleared.
        // PARTIAL SLICE: the members whose committed types expose Construct run the REAL
        // call; the rest are covered by their own TUs [marked deviation]. This replaces the
        // WorldLinkStubs base-only gate, which left mSceneResultQueue un-Constructed -- the
        // scene->race-car pre-physics bridge Appends into it every frame.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mSceneResultQueue.Construct();
            // ⭐⭐ 2026-08-11 (WorldBridgeInputToEntityModules mount) -- ADDED, AND IT HAD
            // BECOME MANDATORY. The X360 Construct's own list above names
            // "TakedownEvent<8>::Construct(+208976)"; the PC slice skipped it while the
            // member was a 256-byte blob nothing wrote. BridgeInputToEntityModules
            // @0x827ADF88 now drives SetTakedownEventQueue every frame, and that body
            // Clear()s + Append()s the member queue -- which asserts "Not Constructed" and
            // then walks a NULL mpEvents if Construct never ran. Same defect class as the
            // mVehicleDriverInterface line in the OutputBuffer_PrePhysics twin below.
            mTakedownEventQueue.Construct();
            mbControllerActive  = false;
            mbInHardStopCamera  = false;
        }
        const PotentialContactQueue* GetPotentialContactQueue() const;                     // :412
        void                         SetPotentialContactQueue(const PotentialContactQueue*); // :413
        // Bodied inline 2026-08-01 (drivable wave) alongside its non-const twin below: it
        // was declaration-only, and PlaceOnTrackManager::PrePhysicsUpdate -- which takes the
        // input buffer by const pointer, as the console signature does -- is its first caller.
        const SceneResultQueue* GetSceneResultQueue() const { return &mSceneResultQueue; }  // :415
        // Real accessor (was a WorldLinkStubs stub that returned NULL, which the
        // scene->race-car pre-physics bridge then dereferenced). The member is committed.
        SceneResultQueue*       GetSceneResultQueue() { return &mSceneResultQueue; }      // :416
        const AIModuleResultInterface* GetAIModuleResultInterface() const;                // :418
        void                           SetAIModuleResultInterface(const AIModuleResultInterface*); // :419
        const TakedownEventQueue* GetTakedownEventQueue() const;                          // :421
        void                      SetTakedownEventQueue(const TakedownEventQueue*);        // :422
        const ScoringInterface* GetScoringInterface() const;                              // :424
        void                    SetScoringInterface(const ScoringInterface*);             // :425
        const OnlineScoringInterface* GetOnlineScoringInterface() const;                  // :427 R (0x822B59F8, X360 line 436)
        void                          SetOnlineScoringInterface(const OnlineScoringInterface*); // :428 W (0x8279DCF8, X360 line 437)
        const TrafficToRaceCarInterface_PostScene* GetTrafficToRaceCarInterface_PostScene() const; // :430 R (0x822B5A48, export hole)
        void                                       SetTrafficToRaceCarInterface_PostScene(const TrafficToRaceCarInterface_PostScene*); // :431
        bool GetControllerActive() const;                                                 // :433
        void SetControllerActive(bool);                                                   // :434
        bool GetInHardStopCamera() const;                                                 // :436
        void SetInHardStopCamera(bool);                                                   // :437
    private:
        PotentialContactQueue               mPotentialContactQueue;                       // :441
        SceneResultQueue                    mSceneResultQueue;                            // :442
        AIModuleResultInterface             mAIModuleResultInterface;                      // :443
        TakedownEventQueue                  mTakedownEventQueue;                          // :444
        ScoringInterface                    mScoringInterface;                            // :445
        OnlineScoringInterface              mOnlineScoringInterface;                       // :446  (0x822B5800)
        TrafficToRaceCarInterface_PostScene mTrafficToRaceCarInterface_PostScene;         // :447  (0x822B5950)
        bool                                mbControllerActive;                           // :448
        bool                                mbInHardStopCamera;                           // :449
    };

    // ============================================================================
    // OutputBuffer_PrePhysics  (DWARF :462)
    // ============================================================================
    struct OutputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
        typedef BrnPhysics::Vehicle::VehicleDriverInputInterface  VehicleDriverInputInterface;  // :73
        typedef BrnPhysics::Vehicle::VehicleEffectsInputInterface VehicleEffectsInputInterface; // :76
        typedef RaceCarEntityModuleIO::GameEventQueue             GameEventQueue;               // :99

        // X360 0x822EA7B0 -- the status byte, then
        //   VehicleInputInterface::Construct(+16)
        //   VehicleDriverInputInterface::Construct(+142192)
        //   CreateAirRamEvent<20>::Construct(+147488) / CreateSpinEvent<10>::Construct(+148784)
        //   the two counters at +147496/+148792 cleared
        //   VariableEventQueue<1536,16>::Construct + ::Clear (+149312)  [== mGameEventQueue]
        //   the 16-byte block at +149280 zeroed and the word at +149296 cleared
        //
        // ⛔ WAS A BASE-ONLY BOOT GATE IN WorldLinkStubs.cpp AND IT WAS A LIVE DEFECT
        // (drivable wave 2026-08-01). mVehicleInputInterface embeds fifteen EventQueues;
        // none of them had mpEvents. MEASURED: the first ResetActiveRaceCar ->
        // AddHandlingModel -> CreateRaceCar fired "mpEvents != NULL" + "Reached Max length"
        // and the process died. PARTIAL SLICE: the members whose committed types expose
        // Construct run the REAL call; the rest are an explicit list, not a silence.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mVehicleInputInterface.Construct();
            // ⭐⭐ 2026-08-10 (pre-physics bridge wave) -- ADDED, AND IT HAD BECOME MANDATORY.
            // The X360 Construct @0x822EA7B0 calls `VehicleDriverInputInterface::Construct(
            // this+142192)` as its SECOND act; the PC slice skipped it while the member was
            // unread. WorldModule::BridgeEntityModulesToPhysicsModule_PrePhysics @0x827AAEC0
            // now drains this interface's VariableEventQueue<5040,16> every frame
            // (GetFirstEvent/GetNextEvent), and that queue asserts "Not Constructed" -- and then
            // walks a NULL buffer -- if Construct never ran. Same defect class as the
            // mVehicleInputInterface line above, which cost a whole wave in 2026-08-01.
            mVehicleDriverInterface.Construct();
            // ⭐ 2026-08-06 (PhysicsModule::Update leaves wave): the air-ram/spin queues are no
            // longer sized-opaque -- VehicleEffectsInputInterface was promoted to its real DWARF
            // layout, so the real Construct (the two queue Constructs the old FLAG below already
            // attested the X360 runs here) is callable by name.
            mVehicleEffectsInterface.Construct();
            mGameEventQueue.Construct();
            mGameEventQueue.Clear();
            // [FLAG] the two effects counters and the +149280/+149296 block (mPlayerResetInterface):
            // those members are still sized-opaque here. The counters are redundant with the two
            // queue Constructs above (EventQueue::Construct clears miLength); the X360 emits them
            // separately only because it inlined both Constructs.
        }
        // ⚠️ THE :4xx NUMBERS BELOW ARE **PS3 DWARF** DECL LINES. The X360 bodies' baked
        // __LINE__ for the same five const accessors is +9 (479/482/485/488/491) -- do NOT use
        // the line to identify an export here, use the OFFSET IT RETURNS. Getting this wrong is
        // what put 0x8279E070 on GetGameEventQueue for months (corrected 2026-08-10; the full
        // offset table is in BrnRaceCarEntityModuleIO.cpp above the accessors).
        const OutputBuffer_PreScene::VehicleInputInterface* GetVehicleInputInterface() const; // :470 R (0x8279DFC8, +16)
        OutputBuffer_PreScene::VehicleInputInterface*       GetVehicleInputInterface();       // :471 W (0x822B5C00)
        const VehicleDriverInputInterface*  GetVehicleDriverInterface() const;             // :473 R (0x8279E070, +142192)
        VehicleDriverInputInterface*        GetVehicleDriverInterface();                   // :474
        const VehicleEffectsInputInterface* GetVehicleEffectsInterface() const;            // :476 R (0x8279E118, +147488)
        VehicleEffectsInputInterface*       GetVehicleEffectsInterface();                  // :477
        const RCEntityPlayerResetInterface* GetPlayerResetInterface() const;               // :479 R (0x8279E1C0, +149280)
        RCEntityPlayerResetInterface*       GetPlayerResetInterface();                     // :480
        const GameEventQueue* GetGameEventQueue() const;                                   // :482 R (0x8279E268, +149312)
        GameEventQueue*       GetGameEventQueue();                                         // :483 W (0x822B5CA8)
    private:
        OutputBuffer_PreScene::VehicleInputInterface mVehicleInputInterface;               // :487
        VehicleDriverInputInterface  mVehicleDriverInterface;                              // :488
        VehicleEffectsInputInterface mVehicleEffectsInterface;                             // :489
        RCEntityPlayerResetInterface mPlayerResetInterface;                                // :490
        GameEventQueue               mGameEventQueue;                                      // :491
    };

    // ============================================================================
    // InputBuffer_PostPhysics  (DWARF :505)
    //   CORRECTION (1): 0x8279E310 (W,+29856) = the non-const GetSceneInputInterface()
    //                   (returns &mSceneInputInterface), NOT a non-const GetContactSpyInterface.
    //   CORRECTION (2): the DWARF declares ONLY the const GetContactSpyInterface() (:528) +
    //                   SetContactSpyInterface(:529) -- NO non-const getter overload exists.
    // ============================================================================
    struct InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        typedef BrnPhysics::Vehicle::VehicleOutputInterface        VehicleOutputInterface;        // :74
        typedef BrnPhysics::Vehicle::VehicleManagerOutputInterface VehicleManagerOutputInterface; // :75
        typedef BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules DeformationOutputInterfaceForEntityModules; // :87
        typedef BrnPhysics::Deformation::DeformationOutputInterface DeformationOutputInterface;   // :88
        typedef BrnPhysics::ContactSpy::ContactSpyInterface        ContactSpyInterface;           // :90
        typedef BrnAI::AIModuleIO::AIRaceCarInterface              AIRaceCarInterface;            // :82
        void Construct();                                                                  // :510
        // ⛔ CORRECTION (3) -- 2026-08-11 (physics-readback wave). EVERY const-getter address
        // below was attributed ONE-TO-TWO SLOTS TOO EARLY. The six out-of-line const getters
        // in the ARTIST image are, by the offset each one returns:
        //     0x822B5F48 -> this+16      == &mVehicleOutputInterface
        //     0x822B5FF0 -> this+27680   == &mVehicleManagerOutputInterface
        //     0x822B6098 -> this+848624  == &mDeformationOutputInterfaceForEntityModules
        //     0x822B6140 -> this+868400  == &mDeformationOutputInterface
        //     0x822B61E8 -> this+879392  == &mContactSpyInterface
        //     0x822B6290 -> this+879408  == &mAIRaceCarInterface
        // 0x822B6098 and 0x822B61E8 were MISSING from the previous attribution entirely,
        // which is what shifted the other four. Three independent proofs:
        //   (a) +16 is the FIRST member after the 16-byte IOBuffer base, and
        //       ReadUpdatedActiveRaceCarDataFromPhysics @0x822E87B8 feeds 0x822B5F48's
        //       return value straight into VehicleOutputInterface::GetRaceCar;
        //   (b) 0x822B6098's return has muNumEntries@+0, maWheelStates@+0xF0 (stride 400),
        //       miNumSkinnedModels@+0x2CB0, miNumLocatorOutputs@+0x2D94 and
        //       mDetachedPartRenderQueue@+0x2E80 -- the DeformationOutputInterfaceFor-
        //       EntityModules layout exactly; 0x822B6140's has mpDeformationState@+0x70 and
        //       mGlassSmashOrCrackQueue@+0x1AF0 -- DeformationOutputInterface exactly;
        //   (c) the "Not locked for reading" asserts these getters fire carry THIS header's
        //       X360 __LINE__: 522 / 525 / 531 / 534 / 537 / 540 -- a perfectly regular
        //       3-line ladder with the missing rung at 528 (the const GetSceneInputInterface,
        //       which the X360 has no out-of-line body for; only its non-const twin
        //       @0x8279E310 survives). The DWARF declaration lines below are the PS3 header's
        //       and run a few lines earlier; the X360 assert lines are what pin the bodies.
        const VehicleOutputInterface* GetVehicleOutputInterface() const;                   // :513 R (0x822B5F48, +16)
        void                          SetVehicleOutputInterface(const VehicleOutputInterface*); // :514
        const VehicleManagerOutputInterface* GetVehicleManagerOutputInterface() const;     // :516 R (0x822B5FF0, +27680)
        void                                 SetVehicleManagerOutputInterface(const VehicleManagerOutputInterface*); // :517
        const OutputBuffer_PreScene::SceneInputInterface* GetSceneInputInterface() const;  // :519 (no out-of-line body; X360 :528 rung is empty)
        OutputBuffer_PreScene::SceneInputInterface*       GetSceneInputInterface();        // :520 W (0x8279E310) [CORRECTION (1)]
        const DeformationOutputInterfaceForEntityModules* GetDeformationOutputInterfaceForEntityModules() const; // :522 R (0x822B6098, +848624)
        void SetDeformationOutputInterfaceForEntityModules(const DeformationOutputInterfaceForEntityModules*); // :523
        const DeformationOutputInterface* GetDeformationOutputInterface() const;           // :525 R (0x822B6140, +868400)
        void                              SetDeformationOutputInterface(const DeformationOutputInterface*); // :526
        const ContactSpyInterface* GetContactSpyInterface() const;                         // :528 R (0x822B61E8, +879392)
        void                       SetContactSpyInterface(const ContactSpyInterface*);     // :529 [CORRECTION (2): no non-const getter]
        const AIRaceCarInterface*  GetAIRaceCarInterface() const;                          // :531 R (0x822B6290, +879408)
        void                       SetAIRaceCarInterface(const AIRaceCarInterface*);       // :532
    private:
        VehicleOutputInterface                     mVehicleOutputInterface;               // :536
        VehicleManagerOutputInterface              mVehicleManagerOutputInterface;        // :537
        OutputBuffer_PreScene::SceneInputInterface mSceneInputInterface;                  // :538  (0x8279E310 -> +29856)
        DeformationOutputInterfaceForEntityModules mDeformationOutputInterfaceForEntityModules; // :539
        DeformationOutputInterface                 mDeformationOutputInterface;           // :540
        ContactSpyInterface                        mContactSpyInterface;                  // :541
        AIRaceCarInterface                         mAIRaceCarInterface;                   // :542
    };

    // ============================================================================
    // OutputBuffer_PostPhysics  (DWARF :555)
    // ============================================================================
    struct OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        typedef BrnDirector::BrnDirectorVehicleInputInterface DirectorVehicleInputInterface; // :103
        typedef RaceCarEntityModuleIO::ResourceRequestInterface ReplayRequestInterface;       // :107

        // X360 0x822EA8F8 -- IOBuffer status, then VariableEventQueue<8192,16>::Construct +
        // ::Clear on the resource-request ring at +4 (== mResourceRequestInterface.mRequestQueue),
        // InSceneUpdateInterface::Construct, NewVehicleEvent<50>::Construct, the four
        // RCEntity*OutputInterface::Clear calls, VariableEventQueue<1536,16>::Construct on the
        // game-event queue, an 11-word zero fill and VehicleInputInterface::Construct.
        // PARTIAL SLICE: only the request ring is brought up here -- it is the one this build
        // actually writes (RaceCarEntityModule::SendStreamerEvents @0x82304F70 Appends the five
        // component streamers' queues into it every frame, and the base-only WorldLinkStubs gate
        // this replaces left it un-Constructed, which fired "Not Constructed" once per frame).
        // GROW as the other members' types land.
        // ⭐⭐ COMPLETED 2026-08-15 (IO-buffer zero-fill removal audit) -- the four
        // RCEntity*OutputInterface::Clear calls the banner above always transcribed are now
        // actually made, in the console's order. Same defect and same reasoning as the
        // OutputBuffer_PreScene twin: they were listed and never emitted, and the removed PC
        // zero-fill was the only thing hiding it.
        void Construct()
        {
            CgsModule::IOBuffer::Construct();                    // X360 *a1 = 1
            mResourceRequestInterface.mRequestQueue.Construct();  // X360 +4
            mResourceRequestInterface.mRequestQueue.Clear();
            // [FLAG] the console's InSceneUpdateInterface::Construct(+8224) on
            // mSceneInputInterface is still not emitted (that interface's Construct lives in the
            // CgsSceneManager IO TU and has no body here yet).
            // ⭐ ADDED 2026-08-02 (camera parameter-chain wave) -- the "NewVehicleEvent<50>::
            // Construct" leg named in the X360 note above. The producer that writes it lands
            // in this wave (RaceCarEntityModule's new-vehicle publish), and an unconstructed
            // queue has mpEvents == NULL, which is what killed the process the last time an
            // embedded queue in this family went un-Constructed (see the retired
            // OutputBuffer_PrePhysics gate in WorldLinkStubs.cpp).
            mDirectorVehicleInputInterface.Construct();          // X360 +826992
            // ---- the four Clears, in the console's order (0x822EA8F8) ----
            mActiveRaceCarOutputInterface.Clear();               // X360 +827808
            mGlobalRaceCarOutputInterface.Clear();               // X360 +838288
            mReplayActiveRaceCarOutputInterface.Clear();         // X360 +840704
            mReplayGlobalRaceCarOutputInterface.Clear();         // X360 +851184
            mGameEventQueue.Construct();                         // X360 +853600 (VEQ<1536,16>)
            // [FLAG] the console's 11-word zero fill at +855152 and
            // VehicleInputInterface::Construct(+855200) are NOT emitted: on the console those two
            // seats sit between mGameEventQueue and the buffer's end, but the member-offset
            // attributions recorded on the declarations below disagree with the Construct's own
            // offsets for the same two seats (see the report's SUSPECT note), so naming them here
            // would be a guess. Restore both once that attribution is settled.
        }
        const ResourceRequestInterface* GetResourceRequestInterface() const;              // :563
        ResourceRequestInterface*       GetResourceRequestInterface();                     // :564
        const OutputBuffer_PreScene::SceneInputInterface* GetSceneInputInterface() const;  // :566
        OutputBuffer_PreScene::SceneInputInterface*       GetSceneInputInterface();        // :567 W (0x822B5D50)
        const DirectorVehicleInputInterface* GetDirectorVehicleInputInterface() const;     // :569
        DirectorVehicleInputInterface*       GetDirectorVehicleInputInterface();           // :570
        const RCEntityActiveRaceCarOutputInterface* GetActiveRaceCarOutputInterface() const;        // :572
        RCEntityActiveRaceCarOutputInterface*       GetActiveRaceCarOutputInterface();              // :573
        const RCEntityGlobalRaceCarOutputInterface* GetGlobalRaceCarOutputInterface() const;        // :575
        RCEntityGlobalRaceCarOutputInterface*       GetGlobalRaceCarOutputInterface();              // :576
        const RCEntityActiveRaceCarOutputInterface* GetReplayActiveRaceCarOutputInterface() const;  // :578 R (0x8279E678)
        RCEntityActiveRaceCarOutputInterface*       GetReplayActiveRaceCarOutputInterface();        // :579 W (0x822B6488)
        const RCEntityGlobalRaceCarOutputInterface* GetReplayGlobalRaceCarOutputInterface() const;  // :581
        RCEntityGlobalRaceCarOutputInterface*       GetReplayGlobalRaceCarOutputInterface();        // :582 W (0x822B6920)
        const GameEventQueue* GetGameEventQueue() const;                                   // :584
        GameEventQueue*       GetGameEventQueue();                                         // :585
        const ReplayRequestInterface* GetReplayRequestInterface() const;                   // :587
        ReplayRequestInterface*       GetReplayRequestInterface();                         // :588
        const OutputBuffer_PreScene::VehicleInputInterface* GetVehicleInputInterface() const; // :590 R (0x8279E9C0)
        OutputBuffer_PreScene::VehicleInputInterface*       GetVehicleInputInterface();       // :591 W (0x822B6878)
    private:
        ResourceRequestInterface             mResourceRequestInterface;                    // :595
        OutputBuffer_PreScene::SceneInputInterface mSceneInputInterface;                   // :596  (0x822B5D50 -> +147488)
        DirectorVehicleInputInterface        mDirectorVehicleInputInterface;               // :597
        RCEntityActiveRaceCarOutputInterface mActiveRaceCarOutputInterface;               // :598
        RCEntityGlobalRaceCarOutputInterface mGlobalRaceCarOutputInterface;               // :599
        RCEntityActiveRaceCarOutputInterface mReplayActiveRaceCarOutputInterface;         // :600  (0x822B6488/0x8279E678 -> +826992)
        RCEntityGlobalRaceCarOutputInterface mReplayGlobalRaceCarOutputInterface;         // :601  (0x822B6920 -> +855200)
        GameEventQueue                       mGameEventQueue;                              // :602
        ReplayRequestInterface               mReplayRequestInterface;                      // :603
        OutputBuffer_PreScene::VehicleInputInterface mVehicleInputInterface;               // :604  (0x822B6878/0x8279E9C0 -> +855152)
    };

    // ============================================================================
    // InputBuffer_GenerateDispatchLists  (DWARF :619)
    //
    // CORRECTED 2026-08-17 (coronas step 1). This block used to carry two INVENTED
    // members -- `bool mbDispatchFlagA/B` "at +8289/+8290" -- behind two getters that
    // claimed X360 0x822B6B20 / 0x822B6BD0. Both addresses are real and NEITHER is a byte
    // read; each is a 32-bit load of a trailing POINTER slot:
    //     0x822B6BBC  ori   r11, r11, 0x8184      0x822B6BC0  lwzx  r3, r28, r11
    //     0x822B6C6C  ori   r11, r11, 0x8188      0x822B6C70  lwzx  r3, r28, r11
    // i.e. this+0x8184 == mpBlobbyShadowBuffer and this+0x8188 == mpCoronaSubmissionInterface
    // -- exactly the two members whose DWARF-declared getters (:636 / :639) had NO definition
    // anywhere in this tree. The buffer's five getters are one 0xB0-apiece run over the camera
    // plus the four trailing pointer slots, and their assert line numbers step by 3 with the
    // DWARF declarations:
    //     0x822B69C8  GetCameraInput                this+0x0010   cpp line 639 (0x27F)
    //     0x822B6A70  GetDispatchFrame              this+0x8180   cpp line 642 (0x282)
    //     0x822B6B20  GetBlobbyShadowBuffer         this+0x8184   cpp line 645 (0x285)
    //     0x822B6BD0  GetCoronaSubmissionInterface  this+0x8188   cpp line 648 (0x288)
    //     0x822B6C80  GetShadowMap                  this+0x818C   cpp line 651 (0x28B)
    // The two placeholder members and their getters are DELETED (no reader existed anywhere:
    // `grep -rn "GetDispatchFlagA\|GetDispatchFlagB\|mbDispatchFlagA\|mbDispatchFlagB" src/`
    // matched only their own declaration + definition), and the two REAL getters are bodied in
    // the .cpp at those two addresses. RaceCarEntityModule::SubmitCoronasForRaceCar's call site
    // is the first consumer of GetCoronaSubmissionInterface.
    // ============================================================================
    struct InputBuffer_GenerateDispatchLists : public CgsModule::IOBuffer
    {
        // X360 0x822D3710 -- IOBuffer status, Camera::Construct(+16),
        // VariableEventQueue<32768,16>::Construct(+368) [== mSceneResultQueue] and the four
        // trailing pointer slots cleared. PARTIAL SLICE: the camera bring-up belongs to the
        // camera TU [marked deviation]. (Replaces the WorldLinkStubs base-only gate.)
        void Construct()
        {
            CgsModule::IOBuffer::Construct();
            mSceneResultQueue.Construct();
            mpDispatchFrame            = 0;
            mpBlobbyShadowBuffer       = 0;
            mpCoronaSubmissionInterface = 0;
            mpShadowMap                = 0;
        }
        const BrnDirector::Camera::Camera* GetCameraInput() const;                         // :627 R (0x822B69C8)
        void          SetCameraInput(const BrnDirector::Camera::Camera*);                  // :628
        const SceneResultQueue* GetSceneResultQueue() const;                              // :630
        // Real accessor (see the InputBuffer_PrePhysics twin above).
        SceneResultQueue*       GetSceneResultQueue() { return &mSceneResultQueue; }      // :631
        CgsGraphics::DispatchFrame* GetDispatchFrame() const;                             // :633
        void                        SetDispatchFrame(CgsGraphics::DispatchFrame*);        // :634
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* GetBlobbyShadowBuffer() const;     // :636
        void SetBlobbyShadowBuffer(BrnBlobbyShadowManager::BrnBlobbyShadowBuffer*);       // :637
        BrnCoronaManager::BrnSubmissionInterface* GetCoronaSubmissionInterface() const;   // :639
        void SetCoronaSubmissionInterface(BrnCoronaManager::BrnSubmissionInterface*);     // :640
        BrnWorld::ShadowMap* GetShadowMap() const;                                        // :642
        void                 SetShadowMap(BrnWorld::ShadowMap*);                          // :643
    private:
        BrnDirector::Camera::Camera  mCameraInput;                                        // :647
        SceneResultQueue             mSceneResultQueue;                                   // :648
        CgsGraphics::DispatchFrame*  mpDispatchFrame;                                     // :649
        BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* mpBlobbyShadowBuffer;              // :650
        BrnCoronaManager::BrnSubmissionInterface*      mpCoronaSubmissionInterface;       // :651
        BrnWorld::ShadowMap*         mpShadowMap;                                         // :652
    };

    // ============================================================================
    // Element accessors homed in BrnRaceCarEntityModuleOutputInterface.h (NOT here):
    //   0x8227D690 RCEntityActiveRaceCarOutputInterface::GetRaceCarStateMutable(EActiveRaceCarIndex)
    //              (asserts + IsRaceCarActive gate; &maRaceCarStates[idx]).
    //   0x823101C0 RCEntityGlobalRaceCarOutputInterface::GetActiveCarDataElementAddress(EActiveRaceCarIndex) const
    //              (const void* into the parallel-array window; 36*idx + base + 528).
    //
    // Traffic-system event-queue template instantiations (own TUs):
    //   0x827A7AB8 CgsModule::BaseEventQueue<CreateRivalInTrafficSystemEvent>::Append(const&)  (stride 48)
    //   0x827A7BA8 CgsModule::BaseEventQueue<RemoveRivalFromTrafficSystemEvent>::Append(const&) (stride 1)
    //   0x822E3520 CgsModule::EventQueue<RemoveRivalFromTrafficSystemEvent,34>::Construct()     (capacity 34)
    // ============================================================================
}
}
