#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // mpRender Begin/End/HasRenderBuffer
#include "pc/gcm/renderengine/device.h"   // renderengine::Device (FrameBegin/ShowPixelBuffer) + <Windows.h>
#undef DrawText                            // <Windows.h> (via device.h) #defines DrawText -> DrawTextA; keep our method name

#include <cstdio>    // snprintf (the X360 used CgsCore::SPrintf)
#include <cstring>   // strncpy

namespace CgsDev
{
namespace Assert
{
    Manager gAssertManager;

    // The on-screen assert text colour (carved from the X360 DebugManager::RenderAssert text event,
    // dword_82F32268 = 0xFF32FFFF - a high-contrast light yellow, packed 0xAABBGGRR).
    static const u32 KU_ASSERT_TEXT_COLOUR = 0xFF32FFFFu;

    // Layout (X360 DrawLines/DrawLine/DisplayCallstack): text starts at x=40 and each drawn line
    // advances the running Y by 14; DrawLines wraps a string at 50 columns or a newline.
    static const s32 KI_TEXT_LEFT      = 40;
    static const s32 KI_LINE_ADVANCE   = 14;
    static const s32 KI_MAX_LINE_CHARS = 50;

    Manager::Manager()
        : mpRender(nullptr)
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
        Vector2 lSize = { 10.0f, 12.0f, 0.0f, 0.0f };
        mVectorFont.SetSize(lSize);
        mbInitialised = true;
    }

    // X360 HandleAssert (CgsAssertManager.h:150) -> DoAssert (0x82820338): latch the FIRST failing assert
    // (the X360 halts on the first, so the first is the one shown), log every assert + its call-stack,
    // run the handlers, then halt on-screen. An assert before the renderer has a frame buffer can't draw,
    // so it logs + continues; the re-entrancy guard stops an assert raised while the dialog is up from
    // starting a nested halt.
    void Manager::HandleAssert(const char* lpcMessage, const char* lpcFile, s32 liLine)
    {
        if (!lpcMessage)
            lpcMessage = "<no expression>";
        if (!lpcFile)
            lpcFile = "<no file>";
        ++miAssertCount;

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

        Log::DebugPrint* lpLog = Log::gpDebugPrint;
        *lpLog << "[ASSERT " << miAssertCount << "] " << lpcMessage
               << " (" << lpcFile << ":" << liLine << ")\n";
        *lpLog << "  Callstack:\n";
        for (s32 liIndex = 0; liIndex < lStack.GetNumStackAddresses(); ++liIndex)
            *lpLog << "    " << lStack.GetStackAddress(liIndex) << "\n";
        *lpLog << "  EndCallstack\n";

        ExecuteAssertHandlers();

        if (!mbInAssert && CanRenderAssert())
        {
            mbInAssert = true;
            DoAssert();
            mbInAssert = false;
        }
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
        for (;;)
        {
            DisplayAssertScreen();
            if (GetAsyncKeyState(VK_END) & 0x8000)
                break;
            Sleep(1);
        }

        // Wait for END to be released so the keypress doesn't carry into the resumed game.
        while (GetAsyncKeyState(VK_END) & 0x8000)
            Sleep(1);

        ClearCurrentAssert();
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
            TranslateMessage(&lMsg);
            DispatchMessage(&lMsg);
        }

        if (!renderengine::Device::FrameBeginNoClear())
        {
            renderengine::Device::ShowPixelBuffer();
            if (!renderengine::Device::FrameBeginNoClear())
                return;
        }

        if (mpRender)
        {
            mpRender->Begin();
            miDrawPositionY = static_cast<s16>(48);
            DisplayCurrentAssert();
            DisplayCallstack();
            miDrawPositionY = static_cast<s16>(miDrawPositionY + 10);
            DrawLine("Press END to resume");
            mpRender->End();
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

    // X360 DisplayCallstack 0x828200F8: one line per captured frame, "    0x........". The X360 resolves
    // each frame through the map-file reader (off_83018F20) to a symbol name; that is the MapFile
    // follow-on, so until then the raw return address is printed.
    void Manager::DisplayCallstack()
    {
        const s32 liCount = mCurrentAssert.mStack.GetNumStackAddresses();
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            char lacBuffer[24];
            std::snprintf(lacBuffer, sizeof(lacBuffer), "    0x%p",
                          mCurrentAssert.mStack.GetStackAddress(liIndex));
            DrawLine(lacBuffer);
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
