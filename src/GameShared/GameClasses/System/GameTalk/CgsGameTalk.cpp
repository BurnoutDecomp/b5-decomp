#include "GameShared/GameClasses/System/GameTalk/CgsGameTalk.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"
#include "GameShared/GameClasses/System/GameTalk/CgsGameTalkDebugComponent.h"
// The AttribSys GameTalk message handler registered by Prepare is a static member of
// CgsAttribSys::AttribSysModule (now reconstructed); pull its class so the address-of below
// resolves to the real static-member symbol.
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModule.h"
// That header drags <windows.h> transitively, whose A/W macro rewrites the SendMessage
// identifier to SendMessageA -- which would clobber GameTalk::SendMessage below. Drop it.
#ifdef SendMessage
#undef SendMessage
#endif

// =====================================================================================
// CgsGameTalk::GameTalk -- game-side GameTalk front door.
//   Reconstructed from BURNOUT_X360_ARTIST.XEX. Source files (from the asm asserts):
//     ..\..\..\GameShared\GameClasses\System\GameTalk\CgsGameTalk.h   (inline methods)
//     d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\gametalk\CgsGameTalk.cpp
//
//     Construct              @ 0x82837648  (ran in the goal trace)
//     Prepare                @ 0x828394B8
//     RegisterMessageHandler @ 0x828248D0  (header-inline in the original)
//     SendMessage            @ 0x828DAEE8  (header-inline in the original)
//     Update                 @ 0x828376C0
//
//   All member offsets / constants were read directly off the asm; members are
//   accessed by name.
// =====================================================================================

namespace CgsGameTalk
{
    namespace
    {
        // The "Core/GameTalk" debug log window: a file-scope static in the X360 build
        // (the asm's off_82F32FD4). Construct sizes it to 20 lines and prepares it under
        // the "Core/GameTalk" menu path with the "Log" caption (off_82F32FBC / off_82F32FB8).
        CgsDev::DebugUI::LogWindow sLogWindow;

        const char* const KAC_LOG_WINDOW_PATH    = "Core/GameTalk"; // off_82F32FBC
        const char* const KAC_LOG_WINDOW_CAPTION = "Log";           // off_82F32FB8
        const s32         KI_LOG_WINDOW_LINES     = 20;             // li r4, 0x14

        // The GameTalk manager is created with a small fixed channel count and no flags
        // (asm `li r4, 0xA; li r5, 0`).
        const s32         KI_GAMETALK_MAX_CHANNELS = 10;            // li r4, 0xA

        // The channel name the AttribSys GameTalk handler is registered on
        // (off_82F32FB4 "AttribSys.xenon").
        const char* const KAC_ATTRIBSYS_CHANNEL = "AttribSys.xenon";

        // GameTalkDebugComponent's name/path, returned verbatim by GetName/GetPath. These are the
        // rodata const char* pointers the two accessors lwz through (off_82F30E90 -> "GameTalk",
        // off_82F30E94 -> "Core"); distinct from the KAC_LOG_WINDOW_* pair Prepare uses.
        const char* const KAC_DEBUG_COMPONENT_NAME = "GameTalk"; // off_82F30E90
        const char* const KAC_DEBUG_COMPONENT_PATH = "Core";     // off_82F30E94

        // The size OnActivate passes to Window::SetSize (flt_820DEDB8 / flt_820DEDBC).
        const f32 KF_DEBUG_WINDOW_WIDTH  = 250.0f; // flt_820DEDB8
        const f32 KF_DEBUG_WINDOW_HEIGHT = 60.0f;  // flt_820DEDBC
    }

    // X360 @0x82837648.
    void GameTalk::Construct()
    {
        mbPrepared        = false;          // stb 0, 0x4034
        mpGameTalkManager = nullptr;        // stw 0, 0x4030

        // Bring up the transport on the GameTalk listen port (asm passes 0x2694 in r5).
        mProtocol.Construct(true, KI_DEFAULT_GAMETALK_PORT);

        // Reset the debug component to its clean state (the asm's `...Destruct` symbol on
        // the +0x4024 subobject is the dedup-shared DebugComponent constructor body).
        mDebugComponent.Construct();

        // Bring up the "Core/GameTalk" log window.
        sLogWindow.Construct(KI_LOG_WINDOW_LINES);
        sLogWindow.Prepare(KAC_LOG_WINDOW_PATH, KAC_LOG_WINDOW_CAPTION, 0);
    }

    // X360 @0x828394B8.
    void GameTalk::Prepare(f32 lrInactivityTimeout, f32 lrUpdateTimeStep)
    {
        // Create the process-wide GameTalk singleton, then point mpGameTalkManager at it.
        EA::GameTalk::GameTalkManager::CreateInstance(this, KI_GAMETALK_MAX_CHANNELS, 0);
        mpGameTalkManager = EA::GameTalk::GameTalkManager::GetInstance();

        // Prepare the transport with the inactivity timeout / per-frame update step.
        mProtocol.Prepare(lrInactivityTimeout, lrUpdateTimeStep, mpGameTalkManager);

        // Register the AttribSys GameTalk handler on the AttribSys channel.
        EA::GameTalk::GameTalkManager::RegisterMessageHandler(
            mpGameTalkManager,
            &CgsAttribSys::AttribSysModule::AttribulatorGameTalkHandler,
            KAC_ATTRIBSYS_CHANNEL);

        // Register the debug component with the debug menu.
        mDebugComponent.Register();

        mbPrepared = true;                  // stb 1, 0x4034
    }

    // X360 @0x828248D0 (header-inline in the original CgsGameTalk.h:143-146).
    s32 GameTalk::RegisterMessageHandler(EA::GameTalk::MessageHandler lpHandlerFunc,
                                         const char* lpacFilter)
    {
        CGS_ASSERT(mbPrepared, "mbPrepared");
        CGS_ASSERT(lpHandlerFunc != nullptr, "lpHandlerFunc != NULL");
        CGS_ASSERT(lpacFilter != nullptr, "lpacFilter != NULL");
        CGS_ASSERT(mpGameTalkManager != nullptr, "mpGameTalkManager != NULL");

        return EA::GameTalk::GameTalkManager::RegisterMessageHandler(
            mpGameTalkManager, lpHandlerFunc, lpacFilter);
    }

    // X360 @0x828DAEE8 (header-inline in the original CgsGameTalk.h:178-179).
    s32 GameTalk::SendMessage(const char* lpTarget, EA::GameTalk::GameTalkMessage* lpMessage)
    {
        CGS_ASSERT(lpTarget != nullptr, "lpTarget");
        CGS_ASSERT(lpMessage != nullptr, "lpMessage");

        return EA::GameTalk::GameTalkManager::SendMessage(lpTarget, lpMessage);
    }

    // X360 @0x828376C0.
    void GameTalk::Update()
    {
        CGS_ASSERT(mbPrepared,
                   "Call GameTalk::Prepare() before calling GameTalk::Update()\n");
        CGS_ASSERT(mpGameTalkManager != nullptr, "mpGameTalkManager != NULL");

        // The asm pumps the process-wide singleton (off_8303577C) rather than the member
        // copy; both reference the same GameTalkManager.
        EA::GameTalk::GameTalkManager* lpManager = EA::GameTalk::GameTalkManager::GetInstance();
        if (lpManager != nullptr)
        {
            lpManager->Update();
        }
    }

    // X360 0x82836C20 -- chain up to the (empty) base activation (the bl folds onto the shared empty
    // thunk the disassembler mislabels BaseCollisionGenerator::Destruct), then size the already-
    // prepared "Core/GameTalk" log window (off_82F32FD4 == sLogWindow). Unlike the AttribSys sibling,
    // OnActivate does not Prepare the window -- GameTalk::Construct already did that.
    void GameTalkDebugComponent::OnActivate()
    {
        DebugComponent::OnActivate();

        sLogWindow.SetSize(KF_DEBUG_WINDOW_WIDTH, KF_DEBUG_WINDOW_HEIGHT);
    }

    // X360 0x827DB690 -- lwz r3, off_82F30E90 -> "GameTalk".
    const char* GameTalkDebugComponent::GetName() const
    {
        return KAC_DEBUG_COMPONENT_NAME;
    }

    // X360 0x827DB6A0 -- lwz r3, off_82F30E94 -> "Core".
    const char* GameTalkDebugComponent::GetPath() const
    {
        return KAC_DEBUG_COMPONENT_PATH;
    }
}
