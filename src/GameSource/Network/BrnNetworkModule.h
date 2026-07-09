#ifndef BRN_NETWORK_MODULE_H
#define BRN_NETWORK_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue (output GUI queue)
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID, EActiveRaceCarIndex

// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModule.h
// ============================================================================
// BrnNetwork::BrnNetworkModule -- the engine's networking module. It is owned by value by
// BrnGame::BrnGameModule (BrnGameModule.hpp), brought up by BrnGameModule::BrnGameModule and
// driven each frame by the module update flow.
//
// DWARF (references/DecFIGS/dwarfdump/GameSource/Network/BrnNetworkModule.h, X360-gated):
//   struct BrnNetwork::BrnNetworkModule : public CgsModule::ModuleSingleBuffered { ... }
// The X360 constructor (0x827E5E10) confirms the base: it stores two vtables (off_820CE500 then
// off_820D1440 after the base ctor), constructs two EA::Thread::RWMutex sub-objects at +0x10 and
// +0x118 (the ModuleSingleBuffered input/output mutexes), then constructs the embedded
// BrnNetworkManager at +0x280 (640) and zero/-1 inits a handful of scalar fields.
//
// LAYOUT NOTE (mirrors the committed sibling GameSource/Network/BrnNetworkModuleIO.h):
// the real object is ~833 KB of large composite IO-interface members homed in other, not-yet-
// reconstructed modules. Only the members the functions in this TU touch are modelled, each
// pinned to its exact X360 byte offset (from the accessor `return this + offset` and the ctor
// stores) with explicit u8 storage filling the gaps. The interface member types are forward-
// declared incomplete classes; each touched member is given raw aligned storage of the right
// span at the right offset so the later full reconstruction can replace storage+padding with the
// real typed members without moving anything. Accessors return pointers, so incomplete
// declarations suffice.
//
// The leading region (vtable + base ModuleSingleBuffered, including the two RWMutexes) is modelled
// as one opaque pad up to mbIsUpdating @ +552 (X360 lbz 0x228), exactly as the prior committed
// slice of this header did -- the base's PC layout is not byte-pinned here. The constructor below
// does NOT call a base ctor (no formal base on this minimal slice); it value-initialises the
// modelled storage and sets the one non-zero default the ctor asm stores (the +790352 field = -1).

namespace BrnGameState
{
namespace GameStateModuleIO
{
    struct GameEventQueue
    {
        u8 maOpaque[20340];
    };
}
}

// ---- forward declarations of the interface member types (own TUs) -----------
// Full layouts live with their own reconstructions; the accessors return pointers, so incomplete
// declarations suffice. Replace the matching offset-pinned u8 storage with the real typed member
// when each interface's own TU lands (offsets must not move).
namespace BrnPhysics { namespace Vehicle { class VehicleDriverInputInterface; } }
namespace BrnTraffic { namespace BrnTrafficIO {
    class TrafficNetworkInputInterface;
    class TrafficNetworkOutputInterface;
} }
namespace BrnWorld {
    namespace CrashIO {
        class NetworkInputInterface;
        class NetworkOutputInterface;
    }
    class PlayerVehicleControls;
}

namespace BrnNetwork
{
    // The module's output GUI event queue is a VariableEventQueue<4096,16>: the X360
    // AddOutputGuiEvent<T> instances (e.g. @0x82595788) publish through
    // GetOutputGuiEventQueue()->AddEvent(event, T::GetEventType(), sizeof(T)) with the queue's
    // 4096-byte capacity (matching CgsGui::KI_SMALL_QUEUE_SIZE_KB). Modelled as the real queue so
    // AddEvent is callable; it is the trailing member so the pinned offsets above do not move.
    typedef CgsModule::VariableEventQueue<4096, 16> GuiEventQueueSmall;

    namespace BrnNetworkModuleIO
    {
        struct GameStateToNetworkInterface;   // pointer-only forward (own header)
        class  NetworkToGameStateInterface;   // pointer-only forward (own header)
        class  NetworkEventQueue;             // pointer-only forward (own header)

        // DWARF BrnNetworkModule.h:274/275 nest these queue types under InputBuffer.
        // Modelled here as pointer-only forwards; the accessors return opaque storage typed as
        // the (incomplete) queue so the real homing can drop in the typed member in place.
        namespace InputBuffer
        {
            class TakedownEventQueue;         // GetTakedownEventInputQueue() return
            class GuiEventQueue;              // GetInputGuiEventQueue() return
        }
    }

    class BrnNetworkModule
    {
    public:
        // X360 0x827E5E10 -- bring the module to its constructed state (called by
        // BrnGameModule::BrnGameModule). Value-initialises the modelled storage and applies the
        // ctor's single non-zero default.
        BrnNetworkModule();

        // ---- accessors that existed on the prior committed slice (kept) ----------------------
        BrnGameState::GameStateModuleIO::GameEventQueue* GetGameEventQueue()
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return reinterpret_cast<BrnGameState::GameStateModuleIO::GameEventQueue*>(&mGameEventQueueStorage);
        }

        BrnNetworkManager* GetNetworkManager()
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return reinterpret_cast<BrnNetworkManager*>(&mNetworkManagerStorage);
        }

        GuiEventQueueSmall* GetOutputGuiEventQueue()
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return reinterpret_cast<GuiEventQueueSmall*>(&mOutputGuiEventQueueStorage);
        }

        // ---- output GUI event publisher (X360-attested instances) -------------------------
        // Publish one GUI event onto the module's output GUI event queue. The X360 body (e.g.
        // AddOutputGuiEvent<BrnGui::GuiEventCamStatus> @0x82595788): asserts the module is
        // updating (BrnNetworkModule.h:479), then
        //   GetOutputGuiEventQueue()->AddEvent(&event, TEvent::GetEventType(), sizeof(TEvent))
        // on the VariableEventQueue<4096,16>. The event-type id and byte size fall out of TEvent,
        // so one body reproduces every per-T (id,size) instance store-for-store. Call sites build
        // the event on the stack and call AddOutputGuiEvent<TEvent>(&event) (e.g.
        // BrnNetwork::LoginManagerBase::AnswerAgreeTOS @0x82566478, ::UpdateDownloadingTOS
        // @0x82566320); only instantiated where TEvent is complete.
        template<typename TEvent>
        int AddOutputGuiEvent(const TEvent& lrEvent)
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return GetOutputGuiEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lrEvent),
                lrEvent.GetEventType(), static_cast<s32>(sizeof(TEvent)));
        }

        // The embedded GameState->Network IO interface (X360 GetGameStateToNetworkInterface @
        // 0x82542CA8, return this+818064). Body lands in the .cpp with the rest of this TU.
        BrnNetworkModuleIO::GameStateToNetworkInterface* GetGameStateToNetworkInterface();

        // Convenience forwards onto the GameState->Network mapping table (DWARF .cpp shows the
        // manager calling these directly). Declared-only (bodies land with the mapping-table TU).
        EActiveRaceCarIndex GetActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID);
        NetworkPlayerID     GetNetworkPlayerID(EActiveRaceCarIndex leActiveRaceCarIndex);

        // ---- accessors reconstructed in this TU ----------------------------------------------
        // X360 0x82581700 GetVehicleDriverInputInterface (return this+614704).
        BrnPhysics::Vehicle::VehicleDriverInputInterface* GetVehicleDriverInputInterface();

        // X360 0x82542818 GetTrafficInputInterface (return this+800320).
        BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface* GetTrafficInputInterface();

        // X360 0x825428C0 GetTrafficOutputInterface (const, return this+800432).
        const BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface* GetTrafficOutputInterface() const;

        // X360 0x82542968 GetCrashInputInterface (return this+800576).
        BrnWorld::CrashIO::NetworkInputInterface* GetCrashInputInterface();

        // X360 0x82542A10 GetCrashOutputInterface (const, return this+816080).
        const BrnWorld::CrashIO::NetworkOutputInterface* GetCrashOutputInterface() const;

        // X360 0x82542C00 GetNetworkToGameStateInterface (return this+818608).
        BrnNetworkModuleIO::NetworkToGameStateInterface* GetNetworkToGameStateInterface();

        // X360 0x825817A8 GetPlayerVehicleControls (return this+827880).
        BrnWorld::PlayerVehicleControls* GetPlayerVehicleControls();

        // X360 0x825426C8 GetTakedownEventInputQueue (const, return this+829496).
        const BrnNetworkModuleIO::InputBuffer::TakedownEventQueue* GetTakedownEventInputQueue() const;

        // X360 0x82581850 GetInputGuiEventQueue (return this+829832).
        BrnNetworkModuleIO::InputBuffer::GuiEventQueue* GetInputGuiEventQueue();

        // X360 0x82542D50 GetNetworkEventQueue (return this+852392).
        BrnNetworkModuleIO::NetworkEventQueue* GetNetworkEventQueue();

        // X360 0x825818F8 IsSendUpdateMessageToBeShown (const, lbz this+614664).
        bool IsSendUpdateMessageToBeShown() const;

        // X360 0x825819A0 IsRecvUpdateMessageToBeShown (const, lbz this+614665).
        bool IsRecvUpdateMessageToBeShown() const;

    private:
        // Offsets are absolute from `this`. Only the pinned anchors are load-bearing for this TU;
        // the intervening storage is opaque padding to be subdivided by the sibling member-type
        // TUs (the interface members all live in other modules).

        // [0 .. 552) : vtable + base CgsModule::ModuleSingleBuffered (two RWMutexes @ +0x10/+0x118).
        u8 maPadToIsUpdating[552];
        bool mbIsUpdating;                                          // @ +552 (X360 lbz 0x228 guard)

        // [553 .. 640) : remainder of the base / pre-manager fields.
        u8 maPadToManager[640 - 553];
        u8 mNetworkManagerStorage[614664 - 640];                    // BrnNetworkManager @ +640

        bool mbOutputSendUpdateMessages;                            // @ +614664 (IsSend... return)
        bool mbOutputRecvUpdateMessages;                            // @ +614665 (IsRecv... return)
        u8 mPadToVehicleDriver[614704 - 614666];                    // -> +614704
        // VehicleDriverInputInterface @ +614704. The ctor stores -1 into a 4-byte field at +790352
        // inside this region (X360: li r6,-1 ; stw r6,0x200(r8) with r8 = this+0xC0D50), so the blob
        // is split to expose that one grounded default by name; the field's real name lives with the
        // interface's own TU.
        u8  mVehicleDriverInputInterfaceStorage[790352 - 614704];   // VehicleDriverInputInterface @ +614704
        s32 mField790352;                                           // @ +790352 (ctor default -1)
        u8  mVehicleDriverInputInterfaceStorageTail[800320 - 790356]; // -> +800320

        u8 mTrafficNetworkInputInterfaceStorage[800432 - 800320];   // TrafficNetworkInputInterface @ +800320
        u8 mTrafficNetworkOutputInterfaceStorage[800576 - 800432];  // TrafficNetworkOutputInterface @ +800432
        u8 mCrashNetworkInputInterfaceStorage[816080 - 800576];     // NetworkInputInterface (crash) @ +800576
        u8 mCrashNetworkOutputInterfaceStorage[818064 - 816080];    // NetworkOutputInterface (crash) @ +816080
        u8 mGameStateToNetworkInterfaceStorage[818608 - 818064];    // GameStateToNetworkInterface @ +818064
        u8 mNetworkToGameStateInterfaceStorage[827880 - 818608];    // NetworkToGameStateInterface @ +818608
        u8 mPlayerVehicleControlsStorage[829496 - 827880];          // PlayerVehicleControls @ +827880
        u8 mTakedownEventInputQueueStorage[829832 - 829496];        // TakedownEventQueue @ +829496
        u8 mInputGuiEventQueueStorage[852392 - 829832];             // InputBuffer::GuiEventQueue @ +829832
        u8 mNetworkEventQueueStorage[16];                           // NetworkEventQueue @ +852392 (width placeholder)

        // Trailing members whose exact X360 offsets are not recoverable from this TU's data (the
        // GetGameEventQueue / GetOutputGuiEventQueue accessor addresses are not in this TU). Kept
        // as opaque storage so the kept inline accessors still compile and return the right type;
        // re-home to their pinned offsets when their accessor TUs land.
        BrnGameState::GameStateModuleIO::GameEventQueue mGameEventQueueStorage;
        GuiEventQueueSmall                              mOutputGuiEventQueueStorage;
    };
}

#endif
