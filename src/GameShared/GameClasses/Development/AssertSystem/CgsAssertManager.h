#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"
#include "GameShared/GameClasses/Development/VectorFont/CgsVectorFont.h"

namespace CgsSystem { struct ThreadLayout; }

namespace CgsDev
{
    namespace MapFile { struct Reader; }
namespace Assert
{
    struct AssertData
    {
        StackUnpick mStack;
        MapFile::Reader* mpMapReader;
        char macAssertMessage[KI_MESSAGEBUFFERSIZE];
        const char* mpcFile;
        s32 miLine;
    };

    class Manager;
    // DecFIGS h77..87; ARTIST8281757C..A4 dispatches slot0 and reads next+4.
    struct AssertHandler
    {
        AssertHandler();
        virtual void OnAssert(AssertData& lrAssertData);
    private:
        friend class Manager;
        AssertHandler* mpNext;
    };

    // ARTIST instance: handlers+0, AssertData+4, font+1E0, flags/Y+200/201/202.
    // The four shared homes are independent of the instance; there is no padding proxy.
    class Manager
    {
    public:
        Manager();
        static void RenderThreadAsserts();
        void RegisterAssertHandler(AssertHandler* lpHandler);
        static void SetRenderer(Debug2DImmediateRender* lpRenderer) { mpRender = lpRenderer; }
        static void SetMapFileReader(MapFile::Reader* lpReader, const char* lpcMapFileName)
        { mpMapFileReader = lpReader; mpcMapFileName = lpcMapFileName; }
        static void SetThreadLayout(CgsSystem::ThreadLayout* lpThreadLayout)
        { mpThreadLayout = lpThreadLayout; }

    protected:
        friend void Construct();
        friend void FireAssert(const char*, const char*, int);
        void HandleAssert(const char* lpcMessage, const char* lpcFile, s32 liLine);
        void DoAssert();
        void InitialiseMapFileReader();
        void ExecuteAssertHandlers();
        bool CanRenderAssert();
        void DisplayAssertScreen();
        void DisplayCurrentAssert();
        void DisplayCallstack();
        void DrawLine(const char* lpcMessage);
        void DrawLines(const char* lpcMessage);
        void DrawText(s32 liX, s32 liY, const char* lpcMessage);

        static Debug2DImmediateRender* mpRender;       // ARTIST83018F1C
        static MapFile::Reader* mpMapFileReader;       // ARTIST83018F20
        static const char* mpcMapFileName;             // ARTIST83018F24
        static CgsSystem::ThreadLayout* mpThreadLayout;// ARTIST83018F18
        static const s32 KI_MAX_LINE_LENGTH = 50;

        AssertHandler* mpAssertHandlerList;
        AssertData mCurrentAssert;
        VectorFont mVectorFont;
        bool mbInitialised;
        bool mbGotAssert;
        s16 miDrawPositionY;
    };

    // Existing source spelling for the actual Manager object at ARTIST83019000
    // (DecFIGS names the same object gManager).
    extern Manager gAssertManager;
}
}

#if defined(_XBOX)
CGS_STATIC_ASSERT((sizeof(CgsDev::Assert::AssertData) == 0x1D8), "ARTIST AssertData size");
CGS_STATIC_ASSERT((sizeof(CgsDev::Assert::Manager) == 0x210), "ARTIST assert Manager size");
#endif
