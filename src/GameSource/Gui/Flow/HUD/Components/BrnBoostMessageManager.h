#ifndef BRN_BOOST_MESSAGE_MANAGER_H
#define BRN_BOOST_MESSAGE_MANAGER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                       // CgsID (typedef u64)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h" // BrnFlaptComponent (base)
#include "GameShared/GameClasses/Containers/CgsArray.h"                      // Array<BoostMessageSlot,3>
#include "GameSource/Gui/Flow/HUD/Components/BrnBoostMessageSlot.h"          // BoostMessageSlot (embedded x3)

// ===================================================================================
// GameSource/Gui/Flow/HUD/Components/BrnBoostMessageManager.h
//
// BrnGui::BoostMessageManager -- the in-game boost-HINT message machine. While the boost
// BAR shows the gauge, THIS component owns the stack of up-to-three contextual hint
// popups beside it ("HUD_BOOST_TAKEDOWN", "DRIFT 123", "AIR 1.42", "BARRELROLL_X",
// the jump/smash/billboard tallies, ...). RecvEvent latches per-trick state from the GUI
// event stream; Update and its Update* family decide per frame which hint to post,
// refresh or retire; AddMessage stages text into the three BoostMessageSlots.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; member NAMES/types from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Gui/Flow/Hud/Components/
// BrnBoostMessageManager.h), gated on X360 attestation:
//   Construct 0x82420298  Prepare 0x82428E80   RecvEvent 0x824204E8
//   AddMessage 0x8242E7B8 Update 0x8243E198    FindMessageSlot 0x82428F80
//   GetFreeSlot 0x82429088 GetNumActiveSlots 0x82429100 + the 16 Update*/Handle* bodies.
//
// LEDGER NOTE: Construct / Prepare / AddMessage / Update / FindMessageSlot / GetFreeSlot /
// GetNumActiveSlots were demangle-mishomed to GameShared/.../CgsStrStream.h in the ledger
// (their bodies call StrStream/Assert helpers) and carried "reviewed" marks WITHOUT any
// committed body anywhere in b5-decomp -- the phantom-reviewed set this TU repatriates.
//
// X360 LAYOUT (each offset is a Construct-seed or handler load/store witness;
// sizeof == 0x190, exactly FBurnMainHudState's absent-member carve +0x8B0..+0xA3F):
//   +0x000 BrnFlaptComponent base (mpStateInterface @+0x00, mAptRef @+0x04..+0x0B)
//   +0x00C meCurrentBoostType            +0x090 miJumpCurrentCount           (-1 seeded)
//   +0x010 mbChecking                    +0x094 miJumpTotalCount             (-1)
//   +0x014 miCheckingCount               +0x098 mbSmashesMessagePending
//   +0x018 mbNearMiss                    +0x09C miSmashesCurrentCount        (-1)
//   +0x019 mbIsCrashEscape               +0x0A0 miSmashesTotalCount          (-1)
//   +0x01C miNearMissCount               +0x0A4 mbBillBoardsMessagePending
//   +0x020 mbStuntDone                   +0x0A8 miBillBoardsCurrentCount     (-1)
//   +0x024 miStuntChain                  +0x0AC miBillBoardsTotalCount       (-1)
//   +0x028 mSignatureStuntId (INVALID)   +0x0B0 meHitVehicleCategory         (seeded 8)
//   +0x030 mShortcutId (0)               +0x0B4 miHitVehicleScore  (UNSEEDED)
//   +0x038 mfDriftingDist      +0x03C mfPrevDriftDist
//   +0x040 mfSpinAngle         +0x044 mfPrevSpinAngle     (written by RecvEvent 386; no
//                                                         X360 code reads either back)
//   +0x048 mfOncomingDist      +0x04C mfPrevOncomingDist
//   +0x050 mfAirTime           +0x054 mfPrevAirTime       (UNSEEDED by Construct)
//   +0x058 mfTailDist          +0x05C mfPrevTailDist
//   +0x060 mbBarrelRollCompleted           +0x074 mfPrevAirSpinAngle
//   +0x064 miNumberOfCompletedBarrelRolls  +0x078 mbHandBreakTurnCompleted
//   +0x068 mfCompletedBarrelRollAngle      +0x07C mfHandBrakeTurnAngle
//   +0x06C mbAirSpinInProgress             +0x080 mfPrevHandBrakeTurnAngle
//   +0x070 mfInProgressAirSpinAngle        +0x084 mbCleanLandingComplete
//   +0x085 mbSuccessfulLandingComplete     +0x0B8 miHitVehicleCount (-1)
//   +0x086 mbImpactEventRecorded           +0x0BC mbHitOverheadSign
//   +0x088 meImpactType (0)                +0x0C0 mSlots[3] (stride 0x44) + count word
//   +0x08C mbPlayerDoesTakedown            +0x18C mSlots count (Append'd to 3)
//   +0x08D mbJumpMessagePending
//
// PS3-ONLY HELPERS: the DecFIGS build bodies UpdateTakdowns / UpdateLandingsCheck /
// UpdateSignatureStunts / UpdateShortcuts / UpdateCrashEscape / UpdateBoosting as separate
// functions. The X360 folded them away -- their logic lives EXACTLY where this build's asm
// puts it: takedowns/signature-stunts/shortcuts inline in Update(), the landings check at
// Update()'s tail, the crash-escape arm inside UpdateNearMiss(). No phantom functions are
// minted here.
// ===================================================================================

namespace CgsGui { struct StateInterface; }
namespace CgsModule { struct Event; }
namespace BrnFlapt { struct FileRef; }
namespace BrnGui
{
    class GuiCache;
    // The two stunt-event payloads the private handlers take (real homes in
    // BrnGuiDemangledEventTypes.h; pointer-only here to keep this header light).
    struct GuiCompletedStuntEvent;
    struct GuiInProgressStuntEvent;
}

namespace BrnGui
{

class BoostMessageManager : public BrnFlaptComponent
{
public:
    // DWARF BrnBoostMessageManager.h:64 -- the message-type ids. These key BOTH the
    // mpacMessageTypeStrings string table (@X360 0x82F249D0) and every FindMessageSlot
    // lookup, so the values are wire facts.
    enum MessageType
    {
        E_MESSAGETYPE_GOOD_BOOSTING = 0,
        E_MESSAGETYPE_ONCOMING = 1,
        E_MESSAGETYPE_AIR = 2,
        E_MESSAGETYPE_DRIFT = 3,
        E_MESSAGETYPE_SPIN = 4,
        E_MESSAGETYPE_TWO_WHEELS = 5,
        E_MESSAGETYPE_TAILGATING = 6,
        E_MESSAGETYPE_TRAFFIC_CHECK = 7,
        E_MESSAGETYPE_NEARMISS = 8,
        E_MESSAGETYPE_STUNT = 9,
        E_MESSAGETYPE_SHORTCUT = 10,
        E_MESSAGETYPE_CAR_HIT = 11,
        E_MESSAGETYPE_VAN_HIT = 12,
        E_MESSAGETYPE_TRUCK_HIT = 13,
        E_MESSAGETYPE_BUS_HIT = 14,
        E_MESSAGETYPE_RIG_HIT = 15,
        E_MESSAGETYPE_LIMO_HIT = 16,
        E_MESSAGETYPE_TAXI_HIT = 17,
        E_MESSAGETYPE_TARGET_VEHICLE_HIT = 18,
        E_MESSAGETYPE_N_CARS_HIT = 19,
        E_MESSAGETYPE_OVERHEAD_SIGN = 20,
        E_MESSAGETYPE_TRADING_PAINT = 21,
        E_MESSAGETYPE_NUDGE = 22,
        E_MESSAGETYPE_SLAM = 23,
        E_MESSAGETYPE_SHUNT = 24,
        E_MESSAGETYPE_BOOST_SLAM = 25,
        E_MESSAGETYPE_BOOST_SHUNT = 26,
        E_MESSAGETYPE_GRINDING = 27,
        E_MESSAGETYPE_RUBBING = 28,
        E_MESSAGETYPE_CRASH_ESCAPE = 29,
        E_MESSAGETYPE_SIGNATURE_STUNT = 30,
        E_MESSAGETYPE_BARREL_ROLL = 31,
        E_MESSAGETYPE_HANDBRAKE_TURN = 32,
        E_MESSAGETYPE_TAKEDOWN = 33,
        E_MESSAGETYPE_CLEANLANDING = 34,
        E_MESSAGETYPE_SUCCESSFUL_LANDING = 35,
        E_MESSAGETYPE_JUMPS = 36,
        // RUNTIME NOTE (asm truth over the DWARF spelling): type 37 carries the BILLBOARDS
        // tally ("HUD_BOOST_BILLBOARDS") and type 38 the SMASHES tally
        // ("HUD_BOOST_SMASHES") -- see mpacMessageTypeStrings and
        // UpdateStuntsJumpsAndSmashes' FindMessageSlot/AddMessage ids. The DWARF names 37
        // "E_STUNTS"; the binary uses it for billboards. Spelled verbatim regardless.
        E_MESSAGETYPE_STUNTS = 37,
        E_MESSAGETYPE_SMASHES = 38,

        E_MESSAGETYPE_MAX = 39,
    };

    // KI_MAX_BOOSTMESSAGE_SLOTS (DWARF h:260) -- the mSlots capacity.
    static const s32 KI_MAX_BOOSTMESSAGE_SLOTS = 3;

    // K_SIGNATURE_STUNT_INVALID (DWARF h:261) -- Construct seeds mSignatureStuntId with
    // this all-ones CgsID (`std r10,-1` @0x82420434); Update() tests != -1 on the qword.
    static const CgsID K_SIGNATURE_STUNT_INVALID = static_cast<CgsID>(~0ull);

    // ---- lifecycle + input (the public surface FBurnMainHudState drives) ----
    void Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                   const char* lpacParentName);                             // 0x82420298
    void Prepare(const char* lacName, const BrnFlapt::FileRef& lFile);      // 0x82428E80
    void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType,
                   GuiCache* lpGuiCache);                                  // 0x824204E8
    bool AddMessage(MessageType leMessageType, const char* lpcText,
                    f32 lfTimeToLive, s32 liBoostAmount);                  // 0x8242E7B8
    void Update(f32 lfTimeStep, bool lbShowtimeMode);                      // 0x8243E198

private:
    void UpdateStunts();                 // 0x8242ECD0
    void UpdateVehicleImpacts();         // 0x8242EA48
    void UpdateBarrelRolls();            // 0x8242EC20
    void UpdateHandBrake();              // 0x8242E900
    void UpdateAir();                    // 0x824378C8
    void HandleOnCompletedStunt(const GuiCompletedStuntEvent* lpEvent);   // 0x82411CC8
    void HandleOnInProgressStunt(const GuiInProgressStuntEvent* lpEvent); // 0x82411C70
    void UpdateOncoming();               // 0x8242EDC0
    void UpdateDrift();                  // 0x8242EFA8
    void UpdateSpin();                   // 0x8242F198
    void UpdateTailgating();             // 0x8242F2E0
    void UpdateChecking();               // 0x8242F828
    void UpdateStuntsJumpsAndSmashes();  // 0x8242F4C8
    void UpdateShowtime();               // 0x8242FAE0
    void UpdateNearMiss();               // 0x8242F958

    BoostMessageSlot* GetFreeSlot();                               // 0x82429088
    BoostMessageSlot* FindMessageSlot(MessageType leMessageType);  // 0x82428F80
    s32 GetNumActiveSlots() const;                                 // 0x82429100

private:
    s32  meCurrentBoostType;          // +0x0C (BrnWorld::EBoostType per DWARF; Construct
                                      //        seeds NOTHING -- first write RecvEvent 206)
    bool mbChecking;                  // +0x10
    u8   maPad11[3];                  // +0x11..+0x13
    s32  miCheckingCount;             // +0x14
    bool mbNearMiss;                  // +0x18
    bool mbIsCrashEscape;             // +0x19
    u8   maPad1A[2];                  // +0x1A..+0x1B
    s32  miNearMissCount;             // +0x1C
    bool mbStuntDone;                 // +0x20
    u8   maPad21[3];                  // +0x21..+0x23
    s32  miStuntChain;                // +0x24
    CgsID mSignatureStuntId;          // +0x28
    CgsID mShortcutId;                // +0x30
    f32  mfDriftingDist;              // +0x38
    f32  mfPrevDriftDist;             // +0x3C
    f32  mfSpinAngle;                 // +0x40
    f32  mfPrevSpinAngle;             // +0x44
    f32  mfOncomingDist;              // +0x48
    f32  mfPrevOncomingDist;          // +0x4C
    f32  mfAirTime;                   // +0x50 (UNSEEDED by Construct -- stale until
                                      //        the first RecvEvent 387; faithful)
    f32  mfPrevAirTime;               // +0x54 (UNSEEDED by Construct)
    f32  mfTailDist;                  // +0x58
    f32  mfPrevTailDist;              // +0x5C
    bool mbBarrelRollCompleted;       // +0x60
    u8   maPad61[3];                  // +0x61..+0x63
    s32  miNumberOfCompletedBarrelRolls; // +0x64
    f32  mfCompletedBarrelRollAngle;  // +0x68
    bool mbAirSpinInProgress;         // +0x6C
    u8   maPad6D[3];                  // +0x6D..+0x6F
    f32  mfInProgressAirSpinAngle;    // +0x70 (degrees)
    f32  mfPrevAirSpinAngle;          // +0x74
    bool mbHandBreakTurnCompleted;    // +0x78 (DWARF spelling; raised by
                                      //        HandleOnInProgressStunt bit2 as the
                                      //        handbrake-in-progress latch)
    u8   maPad79[3];                  // +0x79..+0x7B
    f32  mfHandBrakeTurnAngle;        // +0x7C (degrees)
    f32  mfPrevHandBrakeTurnAngle;    // +0x80
    bool mbCleanLandingComplete;      // +0x84
    bool mbSuccessfulLandingComplete; // +0x85
    bool mbImpactEventRecorded;       // +0x86
    u8   maPad87;                     // +0x87
    s32  meImpactType;                // +0x88 (BrnPhysics::Vehicle::EImpactType; 0 seeded)
    bool mbPlayerDoesTakedown;        // +0x8C
    bool mbJumpMessagePending;        // +0x8D
    u8   maPad8E[2];                  // +0x8E..+0x8F
    s32  miJumpCurrentCount;          // +0x90 (-1)
    s32  miJumpTotalCount;            // +0x94 (-1)
    bool mbSmashesMessagePending;     // +0x98
    u8   maPad99[3];                  // +0x99..+0x9B
    s32  miSmashesCurrentCount;       // +0x9C (-1)
    s32  miSmashesTotalCount;         // +0xA0 (-1)
    bool mbBillBoardsMessagePending;  // +0xA4
    u8   maPadA5[3];                  // +0xA5..+0xA7
    s32  miBillBoardsCurrentCount;    // +0xA8 (-1)
    s32  miBillBoardsTotalCount;      // +0xAC (-1)
    s32  meHitVehicleCategory;        // +0xB0 (BrnTraffic::VehicleScoreCategory; the
                                      //        Construct seed 8 == E_VEHICLESCORE_COUNT
                                      //        is UpdateShowtime's "no hit" sentinel)
    s32  miHitVehicleScore;           // +0xB4 (UNSEEDED by Construct)
    s32  miHitVehicleCount;           // +0xB8 (-1)
    bool mbHitOverheadSign;           // +0xBC
    u8   maPadBD[3];                  // +0xBD..+0xBF

    Array<BoostMessageSlot, 3> mSlots;// +0xC0 (elements 3*0x44 == 0xCC; count word at
                                      //        +0x18C, Append'd to 3 by Construct)
};

} // namespace BrnGui

#endif // BRN_BOOST_MESSAGE_MANAGER_H
