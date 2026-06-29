#pragma once

// ===================================================================================
// BrnNetwork::LocalEventScoreUploadData -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnEventScoresManager.h
//
// One pending event-score upload record. The profile keeps a fixed
// Array<LocalEventScoreUploadData, 49> of these (the X360 stores the array at Profile +0x7B8;
// BrnNetwork::EventScoresManager and BrnProgression::Profile add/erase entries via the
// generic Array<T,N>::Append/Erase/EraseFast bodies).
//
// LAYOUT (16-byte record; X360-AUTHORITATIVE -- every Array<...,49> body strides 0x10 and the
// container's live-count word lands at +0x310 == 49 * 0x10; the Array::Append copy is two
// 64-bit `std` over the whole record):
//   +0x00 (8)  mu64EventID    the event's id
//   +0x08 (4)  muScore        the player's score for the event
//   +0x0C (4)  meGameMode     game mode (BrnGameState::GameStateModuleIO::EGameModeType;
//                             a committed s32 enum -- modelled here as its s32 underlying
//                             type so this stays a plain aggregate with no operator==, which
//                             the X360 never emits for this element. FLAG: re-type to the
//                             committed EGameModeType when its home can be included here
//                             without a cycle; the 4-byte size/offset must not change.)
//
// A plain aggregate (no user-declared copy-assignment), so its element copy is the
// compiler-generated 16-byte member-wise move -- matching the two-`std` block move the X360
// Array<...,49>::Append/Erase/EraseFast emit.
// ===================================================================================

#include "types.hpp"

namespace CgsNetwork { struct ServerInterfaceDirtySock; }

namespace BrnNetwork
{
    struct LocalEventScoreUploadData
    {
        u64 mu64EventID;   // +0x00
        u32 muScore;       // +0x08
        s32 meGameMode;    // +0x0C  (EGameModeType underlying s32)
    };

    // ===============================================================================
    // BrnNetwork::EventScoresManager
    //   Minimal reconstructed surface for the online event-scores upload manager. The
    //   full manager (the pending-upload Array<LocalEventScoreUploadData,49>, the score
    //   queue and the server callbacks) is reconstructed in its own (class-sourced) TU
    //   and extends this header; only what the debug component reaches is declared here.
    //   Declarations + the one pinned member are enough for the per-TU `cl /c` gate.
    //
    //   LAYOUT pinned by EventScoresManagerDebugComponent::SetCalvalryBurningRouteBest
    //   @ 0x82591D00: the component reaches mpServerInterface at +0x32C (lwz r11,0x32C),
    //   then calls mpServerInterface->GetCustomCommandsComponent() (component at +0x4C).
    //   Earlier members are unknown, so a padding buffer reserves space up to +0x32C.
    // ===============================================================================
    class BrnServerInterfaceBase;   // mpServerInterface points at the Brn server interface

    class EventScoresManager
    {
    public:
        // The LobbyApi completion trampoline handed to ServerInterfaceCustomCommands::
        // UploadEventScoreData as the upload callback (matches CustomCommandCallback:
        // void(void* lpData, void* lpResult, bool lbSuccess)).
        static void _UploadEventScoreCallback( void* lpData, void* lpResult, bool lbSuccess );

        // The debug component reaches the server interface (+0x32C) to post the upload; the X360
        // inlines this trivial getter at the call site.
        BrnServerInterfaceBase* GetServerInterface() const { return mpServerInterface; }

    private:
        u8                    mPad00[0x32C];   // members before mpServerInterface (other TU)
        BrnServerInterfaceBase* mpServerInterface;   // +0x32C
    };
} // namespace BrnNetwork
