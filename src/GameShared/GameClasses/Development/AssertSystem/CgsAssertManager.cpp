#include <Windows.h>   // VK_END / GetAsyncKeyState / MSG / PeekMessage / Translate/DispatchMessage (the assert
                        // message pump). Included FIRST so winuser.h is fully pulled in before any merged
                        // renderengine header (reached via device.h below) can suppress it with WIN32_LEAN_AND_MEAN.
#undef DrawText         // winuser.h defines DrawText as a macro (->DrawTextW); undef it so this file's own
                        // DrawText method (the assert-overlay text path) resolves. (device.h's later guarded
                        // re-include won't redefine it.)
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // mpRender Begin/End/HasRenderBuffer
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"               // DebugManager::RenderAssertOverlay (the ARTIST overlay)
#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReaderMinimalMemory.h"  // the call-stack symbol resolver
#include "GameShared/GameClasses/System/CgsHardwareInit.h"  // IsHardwareWantingToShutdown (the window-close request)
#include "pc/gcm/renderengine/device.h"   // renderengine::Device (FrameBegin/ShowPixelBuffer) + <Windows.h>
#undef DrawText                            // <Windows.h> (via device.h) #defines DrawText -> DrawTextA; keep our method name

#include <cstdio>    // snprintf (the X360 used CgsCore::SPrintf)
#include <cstdlib>   // getenv (the harness assert-release marker)
#include <cstring>   // strncpy / strrchr

namespace CgsDev
{
namespace Assert
{
    Manager gAssertManager;

    // The call-stack symbol resolver instance (X360: a static MinimalMemoryReader the bring-up wires via
    // SetMapFileReader). Lazily wired on the first assert below.
    static MapFile::MinimalMemoryReader gMinimalMemoryReader;

    // The map file sits next to the executable (BrnGame.exe -> BrnGame.cgsmap), like BrnGame.log. Computed
    // once from the module path (the X360 passes a fixed device path; the PC derives it from the exe).
    static const char* GetDefaultMapFilePath()
    {
        static char sacPath[MAX_PATH] = { 0 };
        if (sacPath[0] == '\0')
        {
            GetModuleFileNameA(nullptr, sacPath, MAX_PATH);
            char* lpcExt = std::strrchr(sacPath, '.');
            if (!lpcExt)
                lpcExt = sacPath + std::strlen(sacPath);
            std::strcpy(lpcExt, ".cgsmap");
        }
        return sacPath;
    }

    // The on-screen assert text colour (carved from the X360 DebugManager::RenderAssert text event,
    // dword_82F32268 = 0xFF32FFFF - a high-contrast light yellow, packed 0xAABBGGRR).
    static const u32 KU_ASSERT_TEXT_COLOUR = 0xFF32FFFFu;

    // Layout (X360 DrawLines/DrawLine/DisplayCallstack): text starts at x=40 and each drawn line
    // advances the running Y by 14; DrawLines wraps a string at 50 columns or a newline.
    static const s32 KI_TEXT_LEFT      = 40;
    static const s32 KI_LINE_ADVANCE   = 14;
    static const s32 KI_MAX_LINE_CHARS = 50;

    // X360 KF_VECTOR_FONT_SIZE (CgsAssertManager.cpp:56) is a SINGLE float, so the glyphs are SQUARE -
    // and it is paired with the 14px line advance above, so it is ~14 (the glyph cell is 8 and caps span
    // the full cell, so a glyph is about this many pixels tall). The exact constant flows through SIMD
    // (VPU) code the decompiler mangles, so this is the square size that fills the 14px line spacing; an
    // earlier {10,12} was a too-small, non-square guess (the reason the text rendered small + narrow).
    static const f32 KF_VECTOR_FONT_SIZE = 14.0f;

    Manager::Manager()
        : mpRender(nullptr)
        , mpMapFileReader(nullptr)
        , mpcMapFileName(nullptr)
        , mpAssertHandlerList(nullptr)
        , mbInitialised(false)
        , mbGotAssert(false)
        , mbInAssert(false)
        , miDrawPositionY(0)
        , miAssertCount(0)
    {
        mCurrentAssert.mpMapReader = nullptr;
        mCurrentAssert.mpcFile = nullptr;
        mCurrentAssert.miLine = 0;
        mCurrentAssert.macAssertMessage[0] = '\0';
    }

    // X360 SetRenderer (CgsAssertManager.h:117): the debug bring-up hands the assert manager the 2D
    // immediate renderer (DebugManager::ConstructRenderer). Wire it up + set up the assert's own vector
    // font (a separate instance from the HUD renderer's). The exact KF_VECTOR_FONT_SIZE is a data-section
    // follow-on; the size below matches the 14px line advance.
    void Manager::SetRenderer(Debug2DImmediateRender* lpRenderer)
    {
        mpRender = lpRenderer;
        mVectorFont.Construct();
        mVectorFont.SetRenderer(lpRenderer);
        Vector2 lSize = { KF_VECTOR_FONT_SIZE, KF_VECTOR_FONT_SIZE, 0.0f, 0.0f };
        mVectorFont.SetSize(lSize);
        mbInitialised = true;
    }

    // X360 SetMapFileReader (CgsAssertManager.h:120): hand the manager the function-map reader + the map
    // file it should open to resolve call-stack addresses to function names.
    void Manager::SetMapFileReader(MapFile::Reader* lpReader, const char* lpcMapFileName)
    {
        mpMapFileReader = lpReader;
        mpcMapFileName  = lpcMapFileName;
    }

    // X360 HandleAssert (CgsAssertManager.h:150) -> DoAssert (0x82820338): latch the FIRST failing assert
    // (the X360 halts on the first, so the first is the one shown), log every assert + its call-stack,
    // run the handlers, then halt on-screen. An assert before the renderer has a frame buffer can't draw,
    // so it logs + continues; the re-entrancy guard stops an assert raised while the dialog is up from
    // starting a nested halt.
    // ============================================================================================
    // [PC bring-up] REPEAT SUPPRESSION -- NOT CONSOLE BEHAVIOUR, AND IT CANNOT BE.
    // ============================================================================================
    // The X360 halts FOREVER on the FIRST assert (DoAssert 0x82820338 loops in DisplayAssertScreen),
    // so the console never reaches a second occurrence of anything and has no repeat policy to be
    // faithful to. This build's resume-on-END halt is already a documented PC divergence; what was
    // NOT documented is its cost when an assert fires EVERY FRAME.
    //
    // MEASURED (run scratch/flow_run/20260826_194302): once an event starts at a junction the
    // player is parked in, GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent posts action 201
    // every frame (console-faithful -- the mbAtJunctionWithEvent latch keeps it posting while the
    // car stands in the region), the bridge turns it into GUI 311, and SatNavRenderer::RecvEvent
    // fires FOUR asserts per frame because the event-start table was never published
    // (GameStateModule::SendSetUpAllEventStartsMessage is unreconstructed -- the FLAG at
    // BrnGameStateModule.cpp:1916). Each one resolved the whole .cgsmap and then BLOCKED the update
    // thread in the loop below until the harness pulsed Local\BurnoutPC_Assert_Release, which
    // boot_test.ps1's Settle() does every 200 ms. Cost: 559 asserts / 4 == 139 game frames in the
    // 191 s after the start, i.e. the simulation ran at 0.73 Hz. Everything downstream then looks
    // broken while being merely starved -- the started stunt mode sat in E_GMS_INTRO because the
    // offline-intro self-trigger integrates the fixed 1/60 s step and needs 360 of them for
    // StuntAttackMode::GetIntroDurationSeconds() == 6.0f, and got 139.
    //
    // POLICY, chosen so the assert-storm ORACLE is preserved exactly: the FIRST occurrence of each
    // distinct (file, line) keeps ALL of the old behaviour -- latched, logged, call-stack resolved,
    // halted. Every LATER occurrence of that same site is still counted and still gets its own
    // `[ASSERT n]` line (so `asserts=N` and every grep for a message still work), but skips the map
    // resolve and the blocking halt. No assert is hidden; only the repeat of one already reported in
    // full is made cheap. BRN_ASSERT_NO_SUPPRESS=1 restores the old halt-every-time behaviour, and
    // the table falling full also falls back to it rather than silently suppressing.
    // ============================================================================================
    namespace
    {
        struct AssertSite
        {
            const char* mpcFile;
            s32         miLine;
            s32         miCount;
        };

        const s32   KI_MAX_ASSERT_SITES = 256;
        AssertSite  gaAssertSites[KI_MAX_ASSERT_SITES];
        s32         giNumAssertSites = 0;

        // 0 == first time this (file,line) has been seen (or the table is full / suppression is
        // switched off, both of which take the full path); >0 == this is repeat number N.
        s32 NoteAssertSite(const char* lpcFile, s32 liLine)
        {
            static const bool sbSuppress = (std::getenv("BRN_ASSERT_NO_SUPPRESS") == nullptr);
            if (!sbSuppress)
                return 0;

            for (s32 liIndex = 0; liIndex < giNumAssertSites; ++liIndex)
            {
                if (gaAssertSites[liIndex].miLine == liLine &&
                    std::strcmp(gaAssertSites[liIndex].mpcFile, lpcFile) == 0)
                {
                    return ++gaAssertSites[liIndex].miCount;
                }
            }

            if (giNumAssertSites >= KI_MAX_ASSERT_SITES)
                return 0;   // table full -- fall back to the old full-report-every-time behaviour

            gaAssertSites[giNumAssertSites].mpcFile = lpcFile;
            gaAssertSites[giNumAssertSites].miLine  = liLine;
            gaAssertSites[giNumAssertSites].miCount = 0;
            ++giNumAssertSites;
            return 0;
        }
    }

    void Manager::HandleAssert(const char* lpcMessage, const char* lpcFile, s32 liLine)
    {
        if (!lpcMessage)
            lpcMessage = "<no expression>";
        if (!lpcFile)
            lpcFile = "<no file>";
        ++miAssertCount;

        // See the banner above. Counted and logged, but not re-resolved and not re-halted.
        const s32 liRepeat = NoteAssertSite(lpcFile, liLine);
        if (liRepeat > 0)
        {
            *Log::gpDebugPrint << "[ASSERT " << miAssertCount << "] " << lpcMessage
                               << " (" << lpcFile << ":" << liLine << ") [repeat " << liRepeat
                               << " -- first occurrence carries the callstack]\n";
            return;
        }

        StackUnpick lStack;
        lStack.Prepare();

        if (!mbGotAssert)
        {
            std::strncpy(mCurrentAssert.macAssertMessage, lpcMessage, KI_MESSAGEBUFFERSIZE - 1);
            mCurrentAssert.macAssertMessage[KI_MESSAGEBUFFERSIZE - 1] = '\0';
            mCurrentAssert.mpcFile     = lpcFile;
            mCurrentAssert.miLine      = liLine;
            mCurrentAssert.mpMapReader = nullptr;
            mCurrentAssert.mStack      = lStack;
            mbGotAssert = true;
        }

        *Log::gpDebugPrint << "[ASSERT " << miAssertCount << "] " << lpcMessage
                           << " (" << lpcFile << ":" << liLine << ")\npress END to continue";

        // A nested assert raised while the dialog is already up is logged, but does not start a second
        // halt (X360 byte_83019201 gate).
        if (mbInAssert)
            return;

        DoAssert();
    }

    void Manager::RegisterAssertHandler(AssertHandler*)
    {
        // The handler list (AssertHandler::OnAssert chain) is the assert-handler-interface follow-on;
        // the boot path registers none.
    }

    void Manager::ExecuteAssertHandlers()
    {
        // No handlers registered on the boot path (see RegisterAssertHandler).
    }

    // X360 CanRenderAssert (CgsAssertManager.h:171): the dialog may draw only once a renderer is wired,
    // it has a frame buffer to render through (set by the first DebugManager::Render), and an assert is
    // pending.
    bool Manager::CanRenderAssert() const
    {
        return mbInitialised && mbGotAssert && mpRender != nullptr && mpRender->HasRenderBuffer();
    }

    // X360 ClearCurrentAssert (CgsAssertManager.h:166): dismiss the pending assert (END pressed -> resume).
    void Manager::ClearCurrentAssert()
    {
        mbGotAssert = false;
    }

    // X360 DoAssert 0x82820338 (the halt): the X360 interrupts the other threads then loops forever in
    // DisplayAssertScreen. This build draws the same dialog every frame and resumes when END is pressed,
    // so the game freezes on a non-fatal assert, shows the report, and can continue. The blocking runs on
    // the asserting thread (the only thread on the loading boot); InteruptThreadForAssert is the
    // threading follow-on.
    void Manager::DoAssert()
    {
        mbInAssert = true;

        // Resolve the captured call-stack against the function map (X360 DoAssert: open + read the map via
        // the reader, store it on the current assert, then dump the resolved call-stack to the log). The
        // reader is wired lazily on first use; it opens the map next to the executable.
        if (!mpMapFileReader)
            SetMapFileReader(&gMinimalMemoryReader, GetDefaultMapFilePath());
        if (mpMapFileReader)
        {
            mpMapFileReader->Prepare(mpcMapFileName, &mCurrentAssert.mStack);
            mCurrentAssert.mpMapReader = mpMapFileReader;
        }

        Log::DebugPrint* lpLog = Log::gpDebugPrint;
        *lpLog << "  Callstack:\n";
        for (s32 liIndex = 0; liIndex < mCurrentAssert.mStack.GetNumStackAddresses(); ++liIndex)
        {
            const char* lpcName = mpMapFileReader ? mpMapFileReader->GetStackEntryName(liIndex) : nullptr;
            if (lpcName)
                *lpLog << "    " << lpcName << "\n";
            else
                *lpLog << "    " << reinterpret_cast<void*>(mCurrentAssert.mStack.GetStackAddress(liIndex)) << "\n";
        }
        *lpLog << "  EndCallstack\n";

        ExecuteAssertHandlers();

        // InteruptThreadForAssert (halt the other threads) is the threading follow-on. The X360 then loops
        // forever in DisplayAssertScreen; this build halts on-screen + resumes on END, but only once the
        // renderer can draw - an early assert (before the first rendered frame) has logged + resolved
        // above and continues.
        if (CanRenderAssert())
        {
            // FLAG PC-platform leaf: the unattended boot harness drives the game through
            // named auto-reset events only (see CgsInputPadsPC::ConsumeHarnessAction) and
            // cannot press END; with the same BRN_INPUT_ALLOW_BACKGROUND marker set,
            // "Local\BurnoutPC_Assert_Release" releases the assert screen exactly like END.
            struct HarnessRelease
            {
                static bool Signalled()
                {
                    static const bool sbEnabled =
                        std::getenv("BRN_INPUT_ALLOW_BACKGROUND") != nullptr;
                    if (!sbEnabled)
                        return false;
                    static HANDLE shEvent = nullptr;
                    if (shEvent == nullptr)
                        shEvent = OpenEventA(0x00100000u /*SYNCHRONIZE*/, FALSE,
                                             "Local\\BurnoutPC_Assert_Release");
                    return shEvent != nullptr &&
                           WaitForSingleObject(shEvent, 0) == WAIT_OBJECT_0;
                }
            };

            for (;;)
            {
                // The host has been asked to shut down (the user closed the window): abandon
                // the halt so this frame unwinds and EngineUpdate can reach the release path.
                // Checked BEFORE the draw, so the same assert re-firing on the way out returns
                // immediately instead of re-entering the halt every frame.
                if (CgsSystem::HardwareInit::IsHardwareWantingToShutdown())
                    break;
                DisplayAssertScreen();
                if (GetAsyncKeyState(VK_END) & 0x8000)
                    break;
                if (HarnessRelease::Signalled())
                    break;
                Sleep(1);
            }
            // Wait for END to be released so the keypress doesn't carry into the resumed game.
            while ((GetAsyncKeyState(VK_END) & 0x8000) &&
                   !CgsSystem::HardwareInit::IsHardwareWantingToShutdown())
                Sleep(1);
            ClearCurrentAssert();
        }

        mbInAssert = false;
    }

    // X360 DisplayAssertScreen 0x82820210: Begin the renderer -> DisplayCurrentAssert + DisplayCallstack
    // -> End -> ShowPixelBuffer (present). The PC adds the message pump (keep the window alive) and opens
    // the scene WITHOUT clearing (FrameBeginNoClear) so the dialog overlays the frozen game frame
    // (preserved by the COPY swap), matching the X360 which never clears. If a scene was already open
    // (the assert fired mid-render) BeginScene fails, so that frame is presented and a fresh scene begun.
    void Manager::DisplayAssertScreen()
    {
        MSG lMsg;
        while (PeekMessage(&lMsg, nullptr, 0, 0, PM_REMOVE))
        {
            // Do NOT swallow WM_QUIT. While the assert screen is up this is the ONLY pump
            // running, so a close request is dispatched from here (WM_CLOSE -> DestroyWindow ->
            // WM_DESTROY -> PostQuitMessage) and its WM_QUIT lands in THIS queue on the very
            // next drain. Discarding it left EngineUpdate's `while (message != WM_QUIT)` with
            // nothing to see -- the window died while the loop, and the XAudio2 voice with it,
            // ran on headless forever. Put it back for the outer loop and stop draining.
            if (lMsg.message == WM_QUIT)
            {
                PostQuitMessage(static_cast<int>(lMsg.wParam));
                break;
            }
            TranslateMessage(&lMsg);
            DispatchMessage(&lMsg);
        }

        // Advance the incremental map load (X360 DisplayAssertScreen calls the reader's Update each frame;
        // synchronous mode has already read the whole map in Prepare, so this is then a no-op).
        if (mpMapFileReader)
            mpMapFileReader->Update();

        if (!renderengine::Device::FrameBeginNoClear())
        {
            renderengine::Device::ShowPixelBuffer();
            if (!renderengine::Device::FrameBeginNoClear())
                return;
        }

        // Paint the RenderAssert OVERLAY - the look the real ARTIST build shows ("line:file" + the failed
        // expression + the map-resolved call-stack). On the X360 the display-owning thread paints
        // DebugManager::RenderAssert while the asserting thread parks; on this single-threaded boot the one
        // (frozen) thread paints it here. (Manager::DisplayCurrentAssert below is the asserting-thread's own
        // "CGSASSERT" dialog - the faithful path for when threading lands, currently not the active display.)
        if (DebugManager* lpDebugManager = DebugManager::ThreadSafeAquire())
        {
            lpDebugManager->RenderAssertOverlay();
            DebugManager::ThreadSafeRelease(lpDebugManager);
        }

        renderengine::Device::ShowPixelBuffer();
    }

    // X360 DisplayCurrentAssert 0x8281FFF8: heading "CGSASSERT: (thread <name>)", then the file, then
    // "Line: <n>", then the assert message (each file/message wrapped). The thread name needs the
    // threading system, so it is "Main" here.
    void Manager::DisplayCurrentAssert()
    {
        char lacHeading[128];
        std::snprintf(lacHeading, sizeof(lacHeading), "CGSASSERT: (thread %s)", "Main");
        char lacLine[32];
        std::snprintf(lacLine, sizeof(lacLine), "Line: %d", mCurrentAssert.miLine);

        if (lacHeading[0])
            DrawLine(lacHeading);
        DrawLines(mCurrentAssert.mpcFile);
        if (lacLine[0])
            DrawLine(lacLine);
        DrawLines(mCurrentAssert.macAssertMessage);
        miDrawPositionY = static_cast<s16>(miDrawPositionY + 10);
    }

    // X360 DisplayCallstack 0x828200F8: one line per captured frame - the function name resolved through
    // the map-file reader (off_83018F20) if available, otherwise the raw "    0x........" address.
    void Manager::DisplayCallstack()
    {
        const s32 liCount = mCurrentAssert.mStack.GetNumStackAddresses();
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            const char* lpcName = mpMapFileReader ? mpMapFileReader->GetStackEntryName(liIndex) : nullptr;
            if (lpcName)
            {
                DrawLine(lpcName);
            }
            else
            {
                char lacBuffer[24];
                std::snprintf(lacBuffer, sizeof(lacBuffer), "    0x%p",
                              reinterpret_cast<void*>(mCurrentAssert.mStack.GetStackAddress(liIndex)));
                DrawLine(lacBuffer);
            }
        }
    }

    // X360 DrawLines 0x8281FEF0: split lpcMessage into runs of at most KI_MAX_LINE_CHARS, breaking on
    // newlines, and DrawLine each non-empty run.
    void Manager::DrawLines(const char* lpcMessage)
    {
        if (!lpcMessage)
            return;

        const char* lpc = lpcMessage;
        while (*lpc)
        {
            s32 liLength = 0;
            while (lpc[liLength] && lpc[liLength] != '\n' && liLength < KI_MAX_LINE_CHARS)
                ++liLength;

            char lacLine[KI_MAX_LINE_CHARS + 1];
            std::strncpy(lacLine, lpc, static_cast<size_t>(liLength));
            lacLine[liLength] = '\0';
            if (lacLine[0])
                DrawLine(lacLine);

            lpc += liLength;
            if (*lpc == '\n')
                ++lpc;
        }
    }

    // X360 DrawLine: draw one line at the left margin + running Y, then advance Y.
    void Manager::DrawLine(const char* lpcMessage)
    {
        DrawText(KI_TEXT_LEFT, miDrawPositionY, lpcMessage);
        miDrawPositionY = static_cast<s16>(miDrawPositionY + KI_LINE_ADVANCE);
    }

    // X360 DrawText 0x8281FE90: if a renderer is wired, draw lpcMessage at (liX, liY) via the vector font.
    void Manager::DrawText(s32 liX, s32 liY, const char* lpcMessage)
    {
        if (mpRender)
            mVectorFont.Print(static_cast<f32>(liX), static_cast<f32>(liY), lpcMessage, KU_ASSERT_TEXT_COLOUR);
    }
}
}
