// ============================================================================
// b5-decomp/src/GameSource/GameState/InviteManager/BrnGameStateInviteManager.cpp
//
// The Xbox-Live invite/join controller-rebind state machine for the GameState module.
// This slice reconstructs the six functions owned by this TU (store-for-store against the
// X360 ARTIST build):
//   CheckPreparedForInvite   0x82363F78  -- once every module reports prepared, advance to perform
//   ProcessBindResults       0x82391F18  -- drain the bind-result queue (advance / fail on result)
//   ProcessGameStateActions  0x8236DA50  -- drain GameState actions (prepared / cancel-invite)
//   ProcessUnbindResults     0x82391E88  -- drain the unbind-result queue (advance / fail on result)
//   RenderDebugText          0x82357DA8  -- draw a line of on-screen debug text for the invite UI
//   UpdatePrepareForInvite   0x823980B0  -- the prepare-for-invite sub-state machine tick
//
// Members are accessed BY NAME against the layout pinned in the header. The two debug
// "invalid index" / "unknown state" assert tripwires preserve the X360 Begin/Fire/End assert
// sequence with the verbatim baked file/line.
// ============================================================================

#include "GameSource/GameState/InviteManager/BrnGameStateInviteManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                     // CgsDev::Assert::Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"                           // CgsDev::StrStream (dynamic assert message)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h" // CgsDev::DebugInterface
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"       // CgsDev::DebugRender::Draw2DText / RGBA
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"        // CgsDev::DebugManager::ThreadSafeRelease

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // File-scope constants (DWARF BrnGameStateInviteManager.cpp:27-42).
    // ------------------------------------------------------------------------

    // Invite-overlay debug text style. KF_INVITE_MANAGER_TEXT_SIZE is the X360 immediate
    // value (flt_8202AC78 == 26.0f) passed as the Draw2DText scale; the colour is the
    // X360 data-resident packed RGBA (dword_82CDBA34) used as the text colour.
    // FLAG: the exact RGBA byte value of KU_INVITE_MANAGER_TEXT_COLOUR is a data global
    //       (dword_82CDBA34) not present in this TU's pseudocode; modelled as opaque white
    //       (0xFFFFFFFF) -- it only affects the on-screen colour of dev-only invite text.
    static const CgsDev::RGBA KU_INVITE_MANAGER_TEXT_COLOUR = 0xFFFFFFFFu;
    static const f32          KF_INVITE_MANAGER_TEXT_SIZE   = 26.0f;

    // The two re-bind controller-event player indices (DWARF :41/:42). The unbind request
    // targets player 0 with the "no port" sentinel (-1); the bind request targets player 0
    // and is given the invited user's controller port from the invite params.
    static const s32 KI_PLAYER_TO_UNBIND = 0;
    static const s32 KI_PLAYER_TO_BIND   = 0;

    // The verbatim X360-baked assert source path for this file (preserved exactly).
    static const char* const KAC_INVITE_MANAGER_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/InviteManager/BrnGameStateInviteManager.cpp";

    // ========================================================================
    // CheckPreparedForInvite @ 0x82363F78
    //
    // Walk the per-module prepared-bit array looking for the first ZERO bit (a module that
    // has NOT yet reported ready). The X360 inlines BitArray<2>::FindFirstZero: it iterates
    // bit indices 0..1, and for each reads the 64-bit field, builds the (1 << bit) mask, and
    // stops at the first field where the masked bit is clear. If every module is prepared
    // (no zero found -> index == capacity, or the not-found sentinel -1) AND the prepare
    // sub-state has reached DONE, or the local-user-change flag is set, advance the top-level
    // invite state to START_PERFORM_INVITE.
    // ========================================================================
    void GameStateInviteManager::CheckPreparedForInvite()
    {
        const u32 luCapacity = 2;
        u32 luIndex = 0;
        for (; luIndex < luCapacity; ++luIndex)
        {
            // FindFirstZero bounds tripwire (X360 inlines BitArray's assert): only ever fires
            // if the search ran off the end, which it cannot on first entry. Preserved as the
            // dynamic "invalid index" assert message the X360 builds.
            if (luIndex >= luCapacity)
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "invalid index : " << luIndex << " < " << luCapacity;
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(),
                                           "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsBitArray.h", 203);
                CgsDev::Assert::EndAssert();
            }

            // First field whose (1 << bit) mask is clear == first not-yet-prepared module.
            if (!mModulePreparedBitArray.IsBitSet(luIndex))
            {
                break;
            }
        }

        const bool lbAllPrepared = (luIndex == luCapacity)
                                || (luIndex == static_cast<u32>(CgsContainers::BitArray<2>::KI_INVALID_BITINDEX));

        if ((lbAllPrepared && mePrepareInviteSubstate == E_PREPARE_INVITE_SUBSTATE_DONE)
            || mInviteOrJoinParams.mbIsLocalUserChangeNeeded)
        {
            meInviteState = E_INVITE_STATE_START_PERFORM_INVITE;
        }
    }

    // ========================================================================
    // ProcessBindResults @ 0x82391F18
    //
    // Drain the input bind-result queue. For each result whose player index is 0 (the local
    // re-bind), inspect the result code: a non-zero code means the bind FAILED, so queue an
    // invite-failed game event (type 62); a zero code means the bind SUCCEEDED, so advance the
    // prepare sub-state to DONE.
    // ========================================================================
    void GameStateInviteManager::ProcessBindResults()
    {
        for (s32 liIndex = 0; liIndex < mInputBindResultQueue.GetLength(); ++liIndex)
        {
            const CgsInput::InputIO::BindResult& lBindResult = mInputBindResultQueue.GetEvent(liIndex);
            if (lBindResult.miPlayer == 0)
            {
                if (lBindResult.meResultCode != 0)
                {
                    // Bind failed -- raise an invite-failed event into the game event queue.
                    u8 laEvent[1] = { 0 };
                    mGameEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(laEvent), 62, 1);
                }
                else
                {
                    mePrepareInviteSubstate = E_PREPARE_INVITE_SUBSTATE_DONE;
                }
            }
        }
    }

    // ========================================================================
    // ProcessGameStateActions @ 0x8236DA50
    //
    // Walk the per-frame GameState action queue (a VariableEventQueue<13312,16>). Two action
    // types are handled: a "module prepared for invite" action (type 94 / 0x5E) marks the
    // sending module ready via SetModulePreparedForInvite; a "cancel invite" action (type 96 /
    // 0x60) clears the prepared-bit array and forces the invite to its terminal states
    // (top-level PERFORM_INVITE-done, prepare sub-state COUNT).
    // ========================================================================
    void GameStateInviteManager::ProcessGameStateActions(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
    {
        typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;
        GameActionQueueImpl* lpQueueImpl = reinterpret_cast<GameActionQueueImpl*>(lpGameActionQueue);

        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liType = lpQueueImpl->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != nullptr)
        {
            if (liType == 94)
            {
                SetModulePreparedForInvite(lpEvent);
            }
            else if (liType == 96)
            {
                mModulePreparedBitArray.UnSetAll();
                meInviteState           = static_cast<EInviteState>(E_INVITE_STATE_COUNT);
                mePrepareInviteSubstate = static_cast<EPrepareInviteSubState>(E_PREPARE_INVITE_SUBSTATE_COUNT);
            }
            liType = lpQueueImpl->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // ========================================================================
    // ProcessUnbindResults @ 0x82391E88
    //
    // Drain the input unbind-result queue (mirror of ProcessBindResults). For each result whose
    // player index is 0: a non-zero result code means the unbind FAILED, so queue an
    // invite-failed game event (type 62); a zero code means the unbind SUCCEEDED, so advance the
    // prepare sub-state to BIND_NEW_CONTROLLER.
    // ========================================================================
    void GameStateInviteManager::ProcessUnbindResults()
    {
        for (s32 liIndex = 0; liIndex < mInputUnBindResultQueue.GetLength(); ++liIndex)
        {
            const CgsInput::InputIO::UnBindResult& lUnBindResult = mInputUnBindResultQueue.GetEvent(liIndex);
            if (lUnBindResult.miPlayer == 0)
            {
                if (lUnBindResult.meResultCode != 0)
                {
                    // Unbind failed -- raise an invite-failed event into the game event queue.
                    u8 laEvent[1] = { 0 };
                    mGameEventQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(laEvent), 62, 1);
                }
                else
                {
                    mePrepareInviteSubstate = E_PREPARE_INVITE_SUBSTATE_BIND_NEW_CONTROLLER;
                }
            }
        }
    }

    // ========================================================================
    // RenderDebugText @ 0x82357DA8
    //
    // Draw one line of on-screen debug text (the invite-state overlay) through the CgsDev debug
    // system: construct a stack DebugInterface (acquires the debug manager), grab its buffered
    // renderer, queue a 2D text draw at lTextPosition in the invite text colour/size, then
    // thread-safe-release the manager.
    // ========================================================================
    void GameStateInviteManager::RenderDebugText(const char* lpcDebugText, Vector2 lTextPosition)
    {
        CgsDev::DebugInterface lDebugInterface;
        lDebugInterface.GetRender().Draw2DText(lpcDebugText, lTextPosition,
                                               KF_INVITE_MANAGER_TEXT_SIZE, KU_INVITE_MANAGER_TEXT_COLOUR);
        CgsDev::DebugManager::ThreadSafeRelease(&lDebugInterface.GetDebugManager());
    }

    // ========================================================================
    // UpdatePrepareForInvite @ 0x823980B0
    //
    // The prepare-for-invite controller re-bind sub-state machine, ticked once per frame while
    // an invite is in progress:
    //   UNBIND_EXISTING_CONTROLLER  -> queue an unbind request (player 0, no port), advance to WAIT_UNBIND
    //   WAIT_UNBIND_..._RESULT      -> drain unbind results
    //   BIND_NEW_CONTROLLER         -> queue a bind request (player 0, invited user's port), advance to WAIT_BIND
    //   WAIT_BIND_..._RESULT        -> drain bind results
    //   DONE / COUNT                -> nothing
    //   default                     -> "unknown state" assert (verbatim file/line 296)
    // ========================================================================
    void GameStateInviteManager::UpdatePrepareForInvite()
    {
        switch (mePrepareInviteSubstate)
        {
            case E_PREPARE_INVITE_SUBSTATE_UNBIND_EXISTING_CONTROLLER:
            {
                CgsInput::InputIO::BaseInputEvent lUnbindRequest;
                lUnbindRequest.miPlayer = KI_PLAYER_TO_UNBIND;   // 0
                lUnbindRequest.miPort   = -1;                    // KI_PLAYER_TO_UNBIND port sentinel
                mOutputUnBindRequestQueue.AddEvent(lUnbindRequest);
                mePrepareInviteSubstate = E_PREPARE_INVITE_SUBSTATE_WAIT_UNBIND_EXISTING_CONTROLLER_RESULT;
                break;
            }

            case E_PREPARE_INVITE_SUBSTATE_WAIT_UNBIND_EXISTING_CONTROLLER_RESULT:
            {
                ProcessUnbindResults();
                break;
            }

            case E_PREPARE_INVITE_SUBSTATE_BIND_NEW_CONTROLLER:
            {
                CgsInput::InputIO::BaseInputEvent lBindRequest;
                lBindRequest.miPlayer = KI_PLAYER_TO_BIND;       // 0
                lBindRequest.miPort   = mInviteOrJoinParams.miUserControllerPort;
                mOutputBindRequestQueue.AddEvent(lBindRequest);
                mePrepareInviteSubstate = E_PREPARE_INVITE_SUBSTATE_WAIT_BIND_NEW_CONTROLLER_RESULT;
                break;
            }

            case E_PREPARE_INVITE_SUBSTATE_WAIT_BIND_NEW_CONTROLLER_RESULT:
            {
                ProcessBindResults();
                break;
            }

            case E_PREPARE_INVITE_SUBSTATE_DONE:
            case E_PREPARE_INVITE_SUBSTATE_COUNT:
                break;

            default:
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unknown state in GameStateInviteManager::UpdatePrepareForInvite: "
                           << static_cast<s32>(mePrepareInviteSubstate) << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), KAC_INVITE_MANAGER_FILE, 296);
                CgsDev::Assert::EndAssert();
                break;
            }
        }
    }
}
