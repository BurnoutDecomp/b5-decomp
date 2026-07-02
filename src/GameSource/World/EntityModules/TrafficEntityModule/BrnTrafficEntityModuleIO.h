#pragma once

// BrnTraffic::BrnTrafficIO IO buffers (TrafficEntityModule shared IO). Reconstructed from the
// DecFIGS DWARF (GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h)
// with member OFFSETS pinned by the X360 retail XEX.
//
// This header currently homes OutputBuffer_PostScene (the post-scene producer buffer the
// traffic module fills with AI-visible traffic + the traffic->race-car interface). The other
// BrnTrafficIO buffers in the DWARF (OutputBuffer_Prepare/PrePhysics/..., InputBuffer_*) are
// reconstructed by their own TUs; only the pieces this TU needs are declared here.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                                   // CgsModule::IOBuffer
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h" // TrafficAIInterface

// The world-module timer-status payload the pre-scene input setter latches (pointer-only use;
// home GameSource/World/BrnWorldModuleIO.h).
namespace BrnWorldIO { struct TimerStatusInterface; }

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    struct TrafficNetworkInputInterface;   // SharedIO/BrnTrafficNetworkInterfaces.h (pointer-only use)

    // ============================================================================
    // InputBuffer_PreScene  (ADDITIVE GROW: WorldBridgeInputToEntityModules TU)
    // ============================================================================
    // MINIMAL SLICE -- only the two setters WorldModule::BridgeInputToEntityModules
    // @0x827ADF88 calls are declared (both real out-of-line X360 symbols,
    // BrnTraffic::BrnTrafficIO::InputBuffer_PreScene::SetTimerStatusInterface /
    // ::SetTrafficNetworkInputInterface, their own ledger functions); the buffer
    // payload is owned by the traffic IO TUs.
    class InputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        void SetTimerStatusInterface(const BrnWorldIO::TimerStatusInterface* lpTimerStatusInterface);
        void SetTrafficNetworkInputInterface(const TrafficNetworkInputInterface* lpTrafficNetworkInputInterface);
    };
    // ============================================================================
    // OutputBuffer_PostScene  (DWARF :291; X360 Construct @ 0x82761830)
    // ============================================================================
    // X360 member offsets (from Construct @ 0x82761830 store displacements):
    //   +0      IOBuffer status flag (*a1 = 1)
    //   +4      mSceneCoarseQueryQueue  (VariableEventQueue<16384,16>::Construct(a1+4))
    //   +16416  mTrafficAIInterface     (GetTrafficAIInterface returns a1+16416; count zeroed)
    //   +61488  mTrafficToRaceCarInterface_PostScene (RivalInTrafficUpdateEvent,34 not here --
    //           it lives inside mTrafficAIInterface @ +45072; this trailing interface is the
    //           post-scene traffic->race-car interface)
    //
    // mSceneCoarseQueryQueue is the SceneManager coarse-query input queue
    // (InputBuffer_Query::InSmCoarseQueryQueue == InCoarseQueryQueue<16384>, a
    // VariableEventQueue<16384,16> subclass that adds NO data members). The X360 places it at
    // offset 4 (4-aligned, right after the 1-byte IOBuffer status), so it is modelled as a
    // 4-ALIGNED 16400-byte sized blob -- NOT the alignas(16) SceneCoarseQueryQueue slice, which
    // would force it to offset 16. mTrafficAIInterface (alignas 16) then lands at 16416 (16400
    // queue ends at 16404, padded up to the next 16-boundary), matching the X360.
    struct OutputBuffer_PostScene : public CgsModule::IOBuffer
    {
        // 4-aligned sized blob for the coarse-query queue (sizeof(VariableEventQueue<16384,16>)
        // == 1 + 16384 + 12 -> round to 4 == 16400). The full queue layout/methods belong to
        // the SceneCoarseQueryQueue TU; this buffer only takes &mSceneCoarseQueryQueue.
        struct SceneCoarseQueryQueue { unsigned char maReserved[16400]; };

        // DWARF :187 -- the trailing traffic->race-car post-scene interface. The DWARF spells
        // it as a 1-byte placeholder (muDUMMY); the X360 zeroes it in Construct. Modelled as the
        // DWARF 1-byte struct (this TU only takes its address, never its interior).
        struct TrafficToRaceCarInterface_PostScene { u8 muDUMMY; };

        void Construct();                                                                  // :249
        const SceneCoarseQueryQueue* GetSceneCoarseQueryQueue() const;                     // :252
        SceneCoarseQueryQueue*       GetSceneCoarseQueryQueue();                            // :253
        const TrafficAIInterface*    GetTrafficAIInterface() const;                        // :255
        TrafficAIInterface*          GetTrafficAIInterface();                              // :256 W (0x827111C0)
        const TrafficToRaceCarInterface_PostScene* GetTrafficToRaceCarInterface_PostScene() const; // :258
        TrafficToRaceCarInterface_PostScene*       GetTrafficToRaceCarInterface_PostScene();        // :259

    private:
        SceneCoarseQueryQueue               mSceneCoarseQueryQueue;                // :263 (offset 4)
        TrafficAIInterface                  mTrafficAIInterface;                   // :264 (offset 16416)
        TrafficToRaceCarInterface_PostScene mTrafficToRaceCarInterface_PostScene;  // :265
    };

    // ============================================================================
    // OutputBuffer_PostPhysics  (DWARF BrnTrafficEntityModuleIO.h, post-physics output buffer)
    // ============================================================================
    // ADDITIVE GROW: this slice homes the post-physics output buffer's four X360-emitted
    // lock-guarded handle accessors (the producer/consumer is TrafficEntityModule::
    // PostPhysicsUpdate / ::GenerateVehicleCrashedEvents, drained by WorldModule::
    // BridgeEntityModulesToOutput_PostPhysics):
    //
    //   <write member @ +8>      @ 0x82711A48  write-lock (bit 3) -> this + 8       (asm-line 392)
    //   <write member @ +834784> @ 0x82711D90  write-lock (bit 3) -> this + 834784  (asm-line 407)
    //   <read  handle  @ +834828>@ 0x827A0E18  read-lock  (bit 4) -> this + 834828  (asm-line 418)
    //   <write handle  @ +834828>@ 0x82712030  write-lock (bit 3) -> this + 834828  (asm-line 419)
    //
    // The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1), `extrwi r11,r11,1,27`);
    // the three non-const (write) handles test the write-lock bit (((*a1 >> 3) & 1),
    // `extrwi r11,r11,1,28`) -- matching CgsModule::IOBuffer's IsBufferLockedForReading()/
    // IsBufferLockedForWriting(). The X360 asserts the lock state (streaming "Not locked for
    // reading/writing\n", a non-gating tripwire at asm-lines 392/407/418/419), then returns the
    // member address via `addi this,8` (+8) / `addis this,0xD; addi this,-0x4320` (+834784) /
    // `addis this,0xD; addi this,-0x42F4` (+834828).
    //
    // The +834828 read-lock and write-lock handles return the SAME member (the const/non-const
    // overload pair, adjacent asm-lines 418/419 in the same header). The +8 and +834784 write-lock
    // handles return two distinct earlier members.
    //
    // FLAG (truncated names / foreign types / opaque interior): the Hex-Rays method names were
    // truncated (G / Ge / GetGuiE / GetR) and not fully recoverable; the lock bit + return offset
    // pin each accessor's identity, so they are named here by offset role with the recoverable name
    // stems documented. The returned members are foreign interface types with their own owning
    // homes elsewhere and are NOT reconstructed here; the storage up to each pinned offset is
    // modelled as correctly-sized opaque storage so the three pinned return offsets (+8, +834784,
    // +834828) are exact. Adopt the named interface types additively when their homes land.
    //
    // PS3-RECONCILE SKIP (branch divergence): the PS3 DecFIGS (B5_FIGS) OutputBuffer_PostPhysics
    // has 9 named members (mCrashTrafficInputInterface, mNetworkInterface, mTrafficSoundOutput-
    // Interface, mTrafficDirectorOutputInterface, mGameEventQueue, mSceneInputInterface,
    // mTrafficTypeResponseQueue, mResourceRequestInterface, mGuiEventQueue) while the Feb-2007
    // b5_main leak (same branch as our X360 target, path d:/P4/B5_main/) has only 6 (and NO
    // Gui queue). Our X360 (a later b5_main) emits accessors for only 3 members and the recovered
    // return offsets (+8, +834784, +834828 -- the last two only 44 bytes apart) do NOT reconcile
    // with the PS3 member sizes/order. The branch layouts diverge, so PS3 names cannot be safely
    // mapped onto these three offsets -- the only X360-clear fact is GetGuiE @ +834828 returns a
    // GuiEventQueue. Names left as offset-role placeholders per the "X360 target wins / do not
    // blind-apply PS3" rule.
    class OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
    public:
        // Opaque foreign-type storages (see FLAG above).
        struct InterfaceAt8Storage      { unsigned char maBytes[1]; };
        struct InterfaceAt834784Storage { unsigned char maBytes[1]; };
        struct InterfaceAt834828Storage { unsigned char maBytes[1]; };

        // X360 0x82711A48 (asm-line 392): write-lock handle, returns this + 8.
        InterfaceAt8Storage*      GetWriteInterfaceAt8();
        // X360 0x82711D90 (asm-line 407): write-lock handle, returns this + 834784. (name stem "GetR...")
        InterfaceAt834784Storage* GetWriteInterfaceAt834784();
        // X360 0x827A0E18 (asm-line 418): read-lock handle, returns this + 834828.
        const InterfaceAt834828Storage* GetReadInterfaceAt834828() const;
        // X360 0x82712030 (asm-line 419): write-lock handle, returns this + 834828. (name stem "GetGui...")
        InterfaceAt834828Storage* GetWriteInterfaceAt834828();

        static void _AssertLayout();

    private:
        // The IOBuffer base subobject's 1-byte status sits at +0; the first handle's member is at
        // +8, so 7 bytes follow the status (the X360 places this member 4-aligned/8-aligned right
        // after the status, matching `addi this,8`). The members between the pinned offsets are
        // folded into correctly-sized opaque storage (see FLAG).
        u8                        maStatusPad[7];                          // +1..+7 (force +8 placement)
        InterfaceAt8Storage       mInterfaceAt8;                           // +8
        unsigned char             maPad8To834784[834784 - 8 - 1];          // span +9..+834783
        InterfaceAt834784Storage  mInterfaceAt834784;                      // +834784
        unsigned char             maPad834784To834828[834828 - 834784 - 1];// span up to +834828
        InterfaceAt834828Storage  mInterfaceAt834828;                      // +834828
    };
}
}
