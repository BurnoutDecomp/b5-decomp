#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#ifdef DrawText
#undef DrawText
#endif
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReader.h"
#include "GameShared/GameClasses/System/Threads/CgsThreadLayout.h"
#include "rw/core/debug/DebugCriticalSection.h"
#include <cstring>

namespace CgsDev
{
namespace Assert
{
    Manager gAssertManager;
    Debug2DImmediateRender* Manager::mpRender;
    MapFile::Reader* Manager::mpMapFileReader;
    const char* Manager::mpcMapFileName;
    CgsSystem::ThreadLayout* Manager::mpThreadLayout;
    extern rw::core::debug::detail::DebugCriticalSection gAssertMutex;

    // DecFIGS B1CA00 stores only these three fields; ARTIST independently pins
    // all offsets/roles and their zero initial values at global83019000.
    Manager::Manager()
    {
        mbGotAssert = false;
        mpAssertHandlerList = 0;
        mbInitialised = false;
    }

    // ARTIST82817568: the actual ordered virtual handler chain, no boot special case.
    void Manager::ExecuteAssertHandlers()
    {
        AssertHandler* lpHandler = mpAssertHandlerList;
        while (lpHandler)
        {
            lpHandler->OnAssert(mCurrentAssert);
            lpHandler = lpHandler->mpNext;
        }
    }

    // The other native readiness predicate is ICF82C296C8 (li r3,1;blr).
    // Both8282051C..44 and828206D0..F8 then test this exact device state.
    bool Manager::CanRenderAssert()
    {
        return renderengine::Device::GetState() == renderengine::Device::STATE_START;
    }

    // ARTIST82820578 export hole, recovered directly through82820754.
    void Manager::HandleAssert(const char* lpcMessage, const char* lpcFile, s32 liLine)
    {
        if (Message::gxMessageFilterFlags & 1)
            *Log::gpDebugPrint << "CGSASSERT: "
                << (lpcMessage ? lpcMessage : "<NULLSTRING>") << " in "
                << (lpcFile ? lpcFile : "<NULLSTRING>") << " line " << liLine << "\n";
        if (!mbGotAssert)
        {
            mCurrentAssert.mpcFile = lpcFile;
            mCurrentAssert.miLine = liLine;
            std::memset(mCurrentAssert.macAssertMessage, 0, KI_MESSAGEBUFFERSIZE);
            std::strncpy(mCurrentAssert.macAssertMessage, lpcMessage, KI_MESSAGEBUFFERSIZE);
            mbGotAssert = true;
            if (gAssertManager.mbInitialised && gAssertManager.CanRenderAssert())
                gAssertManager.DoAssert();
            if (Message::gxMessageFilterFlags & 1)
                *Log::gpDebugPrint << "...CACHED ASSERT\n";
        }
        else if (Message::gxMessageFilterFlags & 1)
            *Log::gpDebugPrint << "...IGNORED. ALREADY HAVE ANOTHER ASSERT\n";
    }

    // ARTIST82820338..5C, inlined from DecFIGS h157.
    void Manager::InitialiseMapFileReader()
    {
        if (mpMapFileReader)
        {
            mpMapFileReader->Prepare(mpcMapFileName, &mCurrentAssert.mStack);
            mCurrentAssert.mpMapReader = mpMapFileReader;
        }
    }

    // ARTIST82820338: log this captured stack, run handlers, interrupt the other
    // native threads, then keep rendering. There is no resume or release path.
    void Manager::DoAssert()
    {
        InitialiseMapFileReader();
        if (mCurrentAssert.mStack.GetNumStackAddresses() > 0)
        {
            if (Message::gxMessageFilterFlags & 1)
                *Log::gpDebugPrint << "\nCallstack:\n";
            for (s32 liIndex = 0; liIndex < mCurrentAssert.mStack.GetNumStackAddresses(); ++liIndex)
            {
                char lacBuffer[16];
                CgsCore::SPrintf(lacBuffer, sizeof(lacBuffer), "    0x%08X",
                    static_cast<u32>(mCurrentAssert.mStack.GetStackAddress(liIndex)));
                const char* lpcString = lacBuffer;
                if (mpMapFileReader)
                {
                    const char* const lpcDecoded = mpMapFileReader->GetStackEntryName(liIndex);
                    if (lpcDecoded)
                        lpcString = lpcDecoded;
                }
                if (Message::gxMessageFilterFlags & 1)
                    *Log::gpDebugPrint << (lpcString ? lpcString : "<NULLSTRING>") << "\n";
            }
            if (Message::gxMessageFilterFlags & 1)
                *Log::gpDebugPrint << "\nEndCallstack\n";
        }
        ExecuteAssertHandlers();
        if (mpThreadLayout)
            mpThreadLayout->InteruptThreadForAssert(&mCurrentAssert);
        for (;;)
            DisplayAssertScreen();
    }

    // ARTIST828204F8: the actual pending global assert handoff under its mutex.
    void Manager::RenderThreadAsserts()
    {
        if (gAssertManager.mbGotAssert && gAssertManager.CanRenderAssert())
        {
            gAssertMutex.Enter();
            gAssertManager.DoAssert();
        }
    }

    // ARTIST82820210: reset this font's renderer, clear the native target, draw,
    // increment the real device frame counter, present and update the map reader.
    void Manager::DisplayAssertScreen()
    {
        mVectorFont.SetRenderer(mpRender);
        miDrawPositionY = 40;
        renderengine::ClearColorParameters lColor;
        lColor.m_color.x = 0.5f;
        lColor.m_color.y = 0.0f;
        lColor.m_color.z = 0.0f;
        lColor.m_color.w = 0.0f;
        const renderengine::ClearDepthStencilParameters lDepthStencil = { 0x30, 1.0f, 0 };
        renderengine::Device::Clear(lColor, lDepthStencil,
            renderengine::RenderTargetState::TARGETID_COLOR_ALL);
        if (mpRender)
            mpRender->Begin();
        DisplayCurrentAssert();
        DisplayCallstack();
        if (mpRender)
            mpRender->End();
        ++renderengine::gDeviceFrameCounter83271624;
        renderengine::Device::ShowPixelBuffer(&renderengine::gOriginalColorBuffer);
        if (mpMapFileReader)
            mpMapFileReader->Update();
    }

    void Manager::DisplayCurrentAssert()
    {
        // ARTIST copies this literal; it does not query or invent a thread name.
        char lacThread[32];
        std::strcpy(lacThread, "unknown");
        char lacHeading[128];
        CgsCore::SPrintf(lacHeading, sizeof(lacHeading), "CGSASSERT: (thread %s)", lacThread);
        char lacLine[32];
        CgsCore::SPrintf(lacLine, sizeof(lacLine), "Line: %d\n", mCurrentAssert.miLine);
        DrawLine(lacHeading);
        DrawLines(mCurrentAssert.mpcFile);
        DrawLine(lacLine);
        DrawLines(mCurrentAssert.macAssertMessage);
        miDrawPositionY = static_cast<s16>(miDrawPositionY + 10);
    }

    void Manager::DisplayCallstack()
    {
        for (s32 liIndex = 0; liIndex < mCurrentAssert.mStack.GetNumStackAddresses(); ++liIndex)
        {
            char lacBuffer[16];
            CgsCore::SPrintf(lacBuffer, sizeof(lacBuffer), "    0x%08X",
                static_cast<u32>(mCurrentAssert.mStack.GetStackAddress(liIndex)));
            const char* lpcString = lacBuffer;
            if (mpMapFileReader)
            {
                const char* const lpcDecoded = mpMapFileReader->GetStackEntryName(liIndex);
                if (lpcDecoded)
                    lpcString = lpcDecoded;
            }
            DrawLine(lpcString);
        }
    }

    void Manager::DrawLine(const char* lpcMessage)
    {
        if (*lpcMessage)
        {
            DrawText(40, miDrawPositionY, lpcMessage);
            miDrawPositionY = static_cast<s16>(miDrawPositionY + 14);
        }
    }

    void Manager::DrawLines(const char* lpcMessage)
    {
        if (!lpcMessage)
            return;
        while (*lpcMessage)
        {
            s32 liLength = 0;
            while (lpcMessage[liLength] && lpcMessage[liLength] != '\n' && liLength < KI_MAX_LINE_LENGTH)
                ++liLength;
            char lacLine[KI_MAX_LINE_LENGTH + 1];
            std::strncpy(lacLine, lpcMessage, liLength);
            lacLine[liLength] = 0;
            DrawLine(lacLine);
            lpcMessage += liLength;
            if (*lpcMessage == '\n')
                ++lpcMessage;
        }
    }

    void Manager::DrawText(s32 liX, s32 liY, const char* lpcMessage)
    {
        if (mpRender)
            mVectorFont.Print(static_cast<f32>(liX), static_cast<f32>(liY), lpcMessage);
    }
}
}
