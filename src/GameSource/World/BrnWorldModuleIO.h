// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO.h
//
// Home of BrnWorldIO::UpdateInputBuffer -- the World module's per-frame "Update"
// input IO-buffer (derives CgsModule::IOBuffer). The 32 ledger functions for
// class:BrnWorldIO::UpdateInputBuffer are the out-of-line accessors/mutators the
// X360 build emitted for this buffer (the const/non-const getters that did not get
// inlined, the queue Append* mergers, and the interface Set* writers).
//
// LAYOUT (FROZEN, X360-authoritative):
//   The member list + order is the DecFIGS DWARF for BrnWorldModuleIO.h:324..354
//   (UpdateInputBuffer private members). The leading per-active-race-car arrays sit
//   at proven byte offsets (the X360 SetRaceCar*/SetLostContact/SetCarSelectStatus
//   bodies store at this+2/this+18/this+34/.../this+74), pinned below with
//   static_asserts in _AssertLayout(). The IOBuffer base is a single status byte;
//   MSVC pads it to +2 before the first u16 array exactly as the X360 layout does.
//
//   The large per-frame interface/queue payloads (VehicleInputInterface,
//   TakedownEventQueue, the trigger/traffic/crash/scoring/replay interfaces, ...) are
//   modelled as minimal-complete sized slices in their canonical-spelling local
//   namespaces. Per the project minimal-slice pattern (see the sibling
//   BrnRaceCarEntityModuleIO.h traffic slices) they are embedded BY VALUE and accessed
//   only BY NAME -- the accessor bodies return &member / forward to a member method, so
//   the byte-exact internal size of each payload is NOT load-bearing for this TU and is
//   grown additively by each payload's own canonical TU. The accessor lock semantics,
//   the member touched, and the store-for-store mutation ARE the parity contract here.
//
// ACCESSOR SHAPE (X360 binary, authoritative): every body tests the buffer status flag
// then acts on the named member. Lock bit -> const-ness / message:
//   ">>3 &1" (eStatusLockedForWrite 0x08) => write-lock, "Not locked for writing",
//                                            mutable getter / Append / Set mutator;
//   ">>4 &1" (eStatusLockedForRead  0x10) => read-lock,  "Not locked for reading",
//                                            const getter.
#pragma once

#include "types.hpp"                                              // s8/s16/s32/u8/u16/u32/f32
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer base
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<N,16>
#include "GameSource/BurnoutConstants.h"                          // EActiveRaceCarIndex

#include <cstddef>   // offsetof
#include <cstring>   // memcpy

// EPaybackType is a network enum homed elsewhere; only its name/width is needed here.
namespace BrnNetwork { enum EPaybackType : s32; }

namespace BrnWorldIO
{
    // BrnWorldModuleIO.h:67-68
    const s32 KI_WORLD_EVENT_QUEUE_MAX_SIZE        = 4096;
    const s32 KI_WORLD_RENDER_EVENT_QUEUE_MAX_SIZE = 32768;

    // ------------------------------------------------------------------------
    // Minimal-complete sized payload slices (canonical-spelling local namespaces).
    // Each carries the small surface the X360 accessor bodies actually invoke:
    //   - interface payloads: an Append(const T*) / Set(const T*) merge-or-copy entry;
    //   - the trigger/game-action queues: VariableEventQueue<N,16> typedefs reused by name.
    // Real layouts/sizes are grown by each payload's own TU; sizes here are NOMINAL.
    // ------------------------------------------------------------------------

    // VehicleInputInterface / VehicleDriverInputInterface (mGameSource Physics/Vehicle homes).
    // Embedded BY VALUE; AppendVehicleInputInterface/AppendVehicleDriverInputInterface call
    // their Append(const T*) which merges the source interface's pending input. Modelled as a
    // sized slice with that single by-name entry point.
    struct VehicleInputInterface
    {
        void Append(const VehicleInputInterface* lpSource)
        {
            // Faithful merge: copy the source payload image (store-for-store).
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[256];   // NOMINAL -- grown by BrnVehicleInputInterface TU
    };

    struct VehicleDriverInputInterface
    {
        void Append(const VehicleDriverInputInterface* lpSource)
        {
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[256];   // NOMINAL -- grown by BrnVehicleDriverInputInterface TU
    };

    // Trigger query input interface = VariableEventQueue<4096,16> (X360 Append into +293412
    // dispatches CgsModule::VariableEventQueue<4096,16>::Append). Reused by name.
    typedef CgsModule::VariableEventQueue<KI_WORLD_EVENT_QUEUE_MAX_SIZE, 16> TriggerQueryInputInterface;

    // Trigger management input interface (Append* exists in DWARF :276; modelled as a sized
    // slice with an Append entry; not in the 32 out-of-line ledger set -- inlined on X360).
    struct TriggerManagementInputInterface
    {
        void Append(const TriggerManagementInputInterface* lpSource)
        {
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[64];    // NOMINAL
    };

    // TimerStatusInterface (X360 SetTimerStatusInterface copies 11 words from the source into
    // this+160900..; modelled as a fixed POD the setter memcpy-copies field-for-field).
    struct TimerStatusInterface
    {
        f32 maData[11];      // X360 copies *a2 .. *(a2+44): 11 words
    };

    // RaceCarRaceDistanceInterface (X360 SetRaceCarRaceDistanceInterface copies 10 words).
    struct RaceCarRaceDistanceInterface
    {
        s32 maData[10];      // X360 copies a 10-word block
    };

    // ScoringInterface (X360 SetScoringInterface XMemCpy 2736 bytes into +317536).
    struct ScoringInterface
    {
        u8 maData[2736];     // X360 SetScoringInterface copies 2736 bytes
    };

    // OnlineScoringInterface (X360 SetOnlineScoringInterface memcpy 164 bytes into +322384).
    struct OnlineScoringInterface
    {
        u8 maData[164];      // X360 SetOnlineScoringInterface copies 164 bytes
    };

    // TrafficNetworkInputInterface (X360 SetTrafficNetworkInterface fires an ActivateHullEvent
    // Append into +301644 then copies a trailing word). Modelled with an Append+Set entry.
    struct TrafficNetworkInputInterface
    {
        void Set(const TrafficNetworkInputInterface* lpSource)
        {
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[256];   // NOMINAL
    };

    // CrashNetworkInputInterface (X360 SetCrashNetworkInterface forwards to the crash
    // NetworkInputInterface operator= at +301760). Modelled with a Set entry.
    struct CrashNetworkInputInterface
    {
        void Set(const CrashNetworkInputInterface* lpSource)
        {
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[128];   // NOMINAL
    };

    // PlayerVehicleControls (X360 SetPlayerVehicleControls memcpy 60 bytes into +317264).
    struct PlayerVehicleControls
    {
        u8 maData[60];       // X360 SetPlayerVehicleControls copies 60 bytes
    };

    // DebugController (X360 Get const at +317324 read-lock, Get non-const at +317324 write-lock).
    struct DebugController
    {
        u8 maData[212];      // NOMINAL (between +317324 and +317536)
    };

    // ReplayStatusInterface (X360 SetReplayStatusInterface forwards to StatusInterface operator=
    // at +320276). Modelled with a Set entry.
    struct ReplayStatusInterface
    {
        void Set(const ReplayStatusInterface* lpSource)
        {
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[64];    // NOMINAL
    };

    // RequestInterface (mWorldEntityRequestInterface; X360 returns &member, both const R and
    // non-const W overloads). Modelled as a sized slice (accessed by-name only).
    struct WorldEntityRequestInterface
    {
        u8 maPayload[64];    // NOMINAL
    };

    // GameActionQueue / TakedownEventQueue are large variable-size queues whose Append merges a
    // source queue. GameActionQueue = VariableEventQueue<13312,16> (X360 +147572 Append).
    typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueue;

    // TakedownEventQueue: X360 +160952 Append dispatches BrnGameState::TakedownEvent_::Append
    // (a BaseEventQueue<TakedownEvent>-style merge). Modelled here as a sized slice exposing the
    // Append(const TakedownEventQueue*) merge entry the accessor calls by name.
    struct TakedownEventQueue
    {
        void Append(const TakedownEventQueue* lpSource)
        {
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[256];   // NOMINAL -- grown by the canonical TakedownEvent queue TU
    };

    // ========================================================================
    // BrnWorldIO::UpdateInputBuffer  (DWARF BrnWorldModuleIO.h:184)
    // ========================================================================
    struct UpdateInputBuffer : public CgsModule::IOBuffer
    {
        // ---- per-active-race-car colour / paint / contact / select state -----------
        void SetRaceCarColourIndex(EActiveRaceCarIndex leActiveRaceCarIndex, u16 lu16ColourIndex);       // :203 (W store-only)
        void SetRaceCarPaintFinishIndex(EActiveRaceCarIndex leActiveRaceCarIndex, u16 lu16PaintIndex);   // :216 (W store-only)
        void SetLostContact(EActiveRaceCarIndex leActiveRaceCarIndex);                                   // :228 (W store-only)
        void SetRegainedContact(EActiveRaceCarIndex leActiveRaceCarIndex);                               // :236 (W store-only)
        void SetCarSelectStatus(EActiveRaceCarIndex leActiveRaceCarIndex, bool lbStatus);                // :245 (W store-only)

        // ---- vehicle input interfaces ---------------------------------------------
        const BrnWorldIO::VehicleInputInterface*       GetVehicleInputInterface() const;                // :259 R (inlined; provided for completeness)
        void                                           AppendVehicleInputInterface(const BrnWorldIO::VehicleInputInterface*);       // :260 W
        const BrnWorldIO::VehicleDriverInputInterface* GetVehicleDriverInputInterface() const;           // :262 R (0x827A3660 GetVehicl)
        void                                           AppendVehicleDriverInputInterface(const BrnWorldIO::VehicleDriverInputInterface*); // :263 W

        // ---- game-action queue ----------------------------------------------------
        GameActionQueue*       GetGameActionQueue();                                                     // :266 W (0x823B4738 GetG)
        void                   AppendGameActionQueue(const GameActionQueue*);                            // :267 W (0x823C8B80)

        // ---- timer status ---------------------------------------------------------
        void                   SetTimerStatusInterface(const TimerStatusInterface*);                     // :270 W

        // ---- takedown event queue -------------------------------------------------
        void                   AppendTakedownEventQueue(const TakedownEventQueue*);                      // :273 W (0x823C8C38)

        // ---- trigger query interface ----------------------------------------------
        void                   AppendTriggerQueryInputInterface(const TriggerQueryInputInterface*);      // :279 W (0x823C8CF0)

        // ---- traffic / crash network interfaces -----------------------------------
        const TrafficNetworkInputInterface* GetTrafficNetworkInterface() const;                         // :284 R (0x827A3AF8 Get)
        void                                SetTrafficNetworkInterface(const TrafficNetworkInputInterface*); // :285 W (0x823C8DA8)
        const CrashNetworkInputInterface*   GetCrashNetworkInterface() const;                            // :287 R (0x827A3BA0 GetCrashNetworkIn)
        void                                SetCrashNetworkInterface(const CrashNetworkInputInterface*); // :288 W (0x823C8E70)

        // ---- debug controller -----------------------------------------------------
        const DebugController* GetDebugController() const;                                               // :290 R (0x827BBE48 GetDebugContro)
        DebugController*       GetDebugController();                                                     // :291 W (0x823B49B0)

        // ---- race-car race-distance interface -------------------------------------
        void                   SetRaceCarRaceDistanceInterface(const RaceCarRaceDistanceInterface*);     // :295 W (0x823B4A58)

        // ---- scoring interfaces ---------------------------------------------------
        const ScoringInterface* GetScoringInterface() const;                                            // :297 R (0x827A3CF0 Ge)
        void                    SetScoringInterface(const ScoringInterface*);                            // :298 W (0x823B4B28)
        void                    SetOnlineScoringInterface(const OnlineScoringInterface*);                // :301 W (0x823B4BE0)

        // ---- controller-active flag -----------------------------------------------
        bool GetControllerActive() const;                                                               // :303 R (0x827A3E40)
        void SetControllerActive(bool);                                                                 // :304 W (0x823B4C98)

        // ---- world-entity request interface ---------------------------------------
        WorldEntityRequestInterface*       GetWorldEntityRequestInterface();                            // :307 W (0x823B4D48 GetWorldEntityRequestI)
        const WorldEntityRequestInterface* GetWorldEntityRequestInterface() const;                      // :308 R (0x827A3EF0 GetWorldEntityRe)

        // ---- replay status interface ----------------------------------------------
        const ReplayStatusInterface* GetReplayStatusInterface() const;                                  // :310 R (0x827A3F98 GetReplayStatusInter)
        void                         SetReplayStatusInterface(const ReplayStatusInterface*);             // :311 W (0x823B4DF0)

        // ---- player vehicle controls ----------------------------------------------
        void SetPlayerVehicleControls(const PlayerVehicleControls*);                                    // :282 W (0x823B48F8)

        // ---- active payback -------------------------------------------------------
        void SetActivePaybackType(BrnNetwork::EPaybackType);                                            // :317 W (0x823B4F50)
        void SetActivePaybackAggressor(EActiveRaceCarIndex);                                            // :319 W (0x823B5000)

    private:
        // ---- FROZEN LAYOUT (DWARF :324..354 order; leading arrays X360-offset-proven) ----
        u16  mau16RaceCarColourIndex[8];        // :324  this+2
        u16  mau16RaceCarPaintFinishIndex[8];   // :325  this+18
        bool mabRaceCarColourIndexValid[8];     // :326  this+34
        bool mabRaceCarPaintFinishIndexValid[8];// :327  this+42
        bool mabLostContactThisFrame[8];        // :328  this+50
        bool mabRegainedContactThisFrame[8];    // :329  this+58
        bool mabCarSelectStatus[8];             // :330  this+66
        bool mabCarSelectStatusValid[8];        // :331  this+74
        VehicleInputInterface        mVehicleInputInterface;        // :332
        VehicleDriverInputInterface  mVehicleDriverInputInterface;  // :333
        s32                          miPlayerRaceCarIndex;          // :334
        GameActionQueue              mGameActionQueue;              // :335
        TimerStatusInterface         mTimerStatusInterface;         // :336
        TakedownEventQueue           mTakedownEventQueue;           // :337
        TriggerManagementInputInterface mTriggerManagementInputInterface; // :338
        TriggerQueryInputInterface   mTriggerQueryInputInterface;   // :339
        BrnNetwork::EPaybackType     meActivePaybackType;           // :340
        EActiveRaceCarIndex          meActivePaybackAggressor;      // :341
        TrafficNetworkInputInterface mTrafficNetworkInterface;      // :343
        CrashNetworkInputInterface   mCrashNetworkInterface;        // :344
        PlayerVehicleControls        mPlayerVehicleControls;        // :345
        DebugController              mDebugController;              // :346
        RaceCarRaceDistanceInterface mRaceCarRaceDistanceInterface; // :347
        ScoringInterface             mScoringInterface;             // :348
        bool                         mbControllerActive;            // :349
        WorldEntityRequestInterface  mWorldEntityRequestInterface;  // :350
        ReplayStatusInterface        mReplayStatusInterface;        // :351
        OnlineScoringInterface       mOnlineScoringInterface;       // :354

        // Pin the X360-proven leading-array byte offsets (store-for-store load-bearing).
        // Defined non-inline in BrnWorldModuleIO.cpp so the static_asserts are always compiled
        // (a never-ODR-used inline member would be skipped by MSVC); it is a private member so the
        // offsetof()s have legal access to the private array members.
        static void _AssertLayout();
    };
}
