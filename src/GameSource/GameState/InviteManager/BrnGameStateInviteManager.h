// ============================================================================
// b5-decomp/src/GameSource/GameState/InviteManager/BrnGameStateInviteManager.h
//
// BrnGameState::GameStateInviteManager -- the Xbox-Live invite/join state machine that
// lives inside the GameState module. When the player accepts an invite (or chooses to
// join a buddy), this manager drives the controller re-bind dance required before the
// session can be torn down and re-created: it unbinds the existing controller, binds the
// invited user's controller, waits for each bind/unbind result, and only then signals the
// rest of GameState that it is prepared to perform the invite.
//
// LAYOUT (X360-authoritative; pinned by the .cpp bodies):
//   +0x000  mGameEventQueue           VariableEventQueue<1536,16>   (== GameStateModuleIO::GameEventQueue)
//   +0x610  mOutputBindRequestQueue   EventQueue<BaseInputEvent,8>  (PostWorldInputBuffer::BindRequestQueue)
//   +0x65C  mOutputUnBindRequestQueue EventQueue<BaseInputEvent,8>  (PostWorldInputBuffer::UnBindRequestQueue)
//   +0x6A8  mInputBindResultQueue     EventQueue<BindResult,8>      (OutputBuffer::BindResultQueue)
//   +0x714  mInputUnBindResultQueue   EventQueue<UnBindResult,8>    (OutputBuffer::UnBindResultQueue)
//   +0x780  mModulePreparedBitArray   BitArray<2>                   (one prepared-bit per GameState module)
//   +0x788  mInviteOrJoinParams       BrnNetwork::...::InviteOrJoinParams (0x9C: port @ +0x14, change-flag @ +0x98)
//   +0x824  meInviteState             EInviteState
//   +0x828  mePrepareInviteSubstate   EPrepareInviteSubState
//
// Member names/types are DWARF-authoritative (BrnGameStateInviteManager.h:97-175); the
// byte offsets are confirmed against the X360 asm of the bodies in the .cpp (e.g.
// mOutputBindRequestQueue @ +0x610, mInputBindResultQueue @ +0x6A8, the bit-array @ +0x780,
// meInviteState @ +0x824, mePrepareInviteSubstate @ +0x828). Members are accessed BY NAME.
//
// This slice bodies the six functions owned by this TU (CheckPreparedForInvite,
// ProcessBindResults, ProcessGameStateActions, ProcessUnbindResults, RenderDebugText,
// UpdatePrepareForInvite). The remaining members are declared (for the correct layout)
// but bodied by their own slices.
// ============================================================================

#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector2 (rw::math::vpu::Vector2)

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                  // CgsModule::VariableEventQueue<1536,16>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                        // CgsContainers::BitArray<2>
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"                 // CgsInput::InputIO::{BaseInputEvent,BindResult,UnBindResult}
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                       // BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams
#include "GameSource/GameState/BrnGameStateModuleIO.h"                            // GameStateModuleIO::{OutputBuffer, GameActionQueue, GameEventQueue}

namespace CgsModule { struct Event; }

namespace BrnGameState
{
    struct GameStateInviteManager
    {
        // DWARF BrnGameStateInviteManager.h:97 -- the top-level invite state.
        enum EInviteState
        {
            E_INVITE_STATE_START_INVITE         = 0,
            E_INVITE_STATE_PREPARE_FOR_INVITE   = 1,
            E_INVITE_STATE_START_PERFORM_INVITE = 2,
            E_INVITE_STATE_PERFORM_INVITE       = 3,
            E_INVITE_STATE_COUNT                = 4,
        };

        // DWARF BrnGameStateInviteManager.h:107 -- the controller re-bind sub-state machine.
        enum EPrepareInviteSubState
        {
            E_PREPARE_INVITE_SUBSTATE_UNBIND_EXISTING_CONTROLLER         = 0,
            E_PREPARE_INVITE_SUBSTATE_WAIT_UNBIND_EXISTING_CONTROLLER_RESULT = 1,
            E_PREPARE_INVITE_SUBSTATE_BIND_NEW_CONTROLLER               = 2,
            E_PREPARE_INVITE_SUBSTATE_WAIT_BIND_NEW_CONTROLLER_RESULT   = 3,
            E_PREPARE_INVITE_SUBSTATE_DONE                              = 4,
            E_PREPARE_INVITE_SUBSTATE_COUNT                            = 5,
        };

        // ---- public lifecycle / per-frame API (declared-only here; bodied by other slices) ----
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();
        void Update(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                    GameStateModuleIO::GameActionQueue* lpGameActionQueue);
        GameStateModuleIO::GameEventQueue* GetGameEventQueue();
        void AppendFromPreWorldInputData(const GameStateModuleIO::PreWorldInputBuffer* lpInputBuffer);
        void RequestInvite(BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams lParams);
        void StartPrepareForInvite();
        bool IsInviteInProgress();

    private:
        // ---- private helpers ----
        void CheckPreparedForInvite();                                            // 0x82363F78 (this TU)
        void ProcessGameStateActions(GameStateModuleIO::GameActionQueue* lpGameActionQueue); // 0x8236DA50 (this TU)
        void SetModulePreparedForInvite(const CgsModule::Event* lpAction);        // own slice
        void UpdatePrepareForInvite();                                            // 0x823980B0 (this TU)
        void UnbindExistingController();                                          // own slice
        void ProcessUnbindResults();                                              // 0x82391E88 (this TU)
        void BindNewController();                                                 // own slice
        void ProcessBindResults();                                               // 0x82391F18 (this TU)
        void InviteFailed();                                                      // own slice
        void WriteDataToOuputBuffer(GameStateModuleIO::OutputBuffer* lpOutputBuffer); // own slice
        void RenderDebugText(const char* lpcDebugText, Vector2 lTextPosition);    // 0x82357DA8 (this TU)

        // ---- members (X360 layout; offsets pinned in the file banner) ----
        // mGameEventQueue is GameStateModuleIO::GameEventQueue, which IS
        // CgsModule::VariableEventQueue<1536,16>; embedded by value as the concrete impl so
        // its 0x610-byte footprint and AddEvent are available directly.
        CgsModule::VariableEventQueue<1536, 16> mGameEventQueue;                        // +0x000
        CgsInput::InputIO::PostWorldInputBuffer::BindRequestQueue   mOutputBindRequestQueue;   // +0x610
        CgsInput::InputIO::PostWorldInputBuffer::UnBindRequestQueue mOutputUnBindRequestQueue; // +0x65C
        CgsInput::InputIO::OutputBuffer::BindResultQueue            mInputBindResultQueue;      // +0x6A8
        CgsInput::InputIO::OutputBuffer::UnBindResultQueue          mInputUnBindResultQueue;    // +0x714
        CgsContainers::BitArray<2>                                  mModulePreparedBitArray;    // +0x780
        BrnNetwork::BrnNetworkModuleIO::InviteOrJoinParams          mInviteOrJoinParams;        // +0x788
        EInviteState                                               meInviteState;              // +0x824
        EPrepareInviteSubState                                     mePrepareInviteSubstate;    // +0x828
    };
}
