#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // KI_MESSAGEBUFFERSIZE + the BeginAssert/FireAssert/EndAssert front-end
#include "GameShared/GameClasses/Development/StackUnpick/CgsStackUnpick.h"   // StackUnpick (AssertData::mStack)
#include "GameShared/GameClasses/Development/VectorFont/CgsVectorFont.h"     // VectorFont (Manager::mVectorFont) + fwd Debug2DImmediateRender

namespace CgsDev
{
    namespace MapFile { struct Reader; }   // the function-map call-stack symbol resolver

namespace Assert
{
    struct AssertHandler;

    // CgsDev::Assert::AssertData (CgsAssertManager.h:58, DWARF) - the captured failing assert that the
    // on-screen renderer draws: the call-stack, the message, the file, and the line. mpMapReader is the
    // map-file symbol resolver (null until that follow-on lands; frames render as raw addresses).
    struct AssertData
    {
        StackUnpick      mStack;
        MapFile::Reader* mpMapReader;
        char             macAssertMessage[KI_MESSAGEBUFFERSIZE];
        const char*      mpcFile;
        s32              miLine;
    };

    // CgsDev::Assert::Manager (CgsAssertManager.h:102, DWARF) - the assert handler + on-screen renderer.
    // FAITHFUL HALT: HandleAssert -> DoAssert captures + logs the assert, runs the registered handlers,
    // (would halt the other threads), then loops on-screen in DisplayAssertScreen drawing the assert via
    // mpRender + mVectorFont. The X360 loops forever (while(1) DisplayAssertScreen); this build draws the
    // same dialog each frame, pumps the window so it stays alive, and RESUMES when END is pressed - so a
    // non-fatal assert freezes the game, shows the report, and lets you continue. The blocking happens on
    // the asserting thread (the only thread on the loading boot). InteruptThreadForAssert (halt the other
    // threads) and the map-file symbol resolver are the threading/map-file follow-on. Method names + the
    // member set come from the DWARF; draw bodies from the X360 (DrawText 0x8281FE90, DrawLines
    // 0x8281FEF0, DisplayCurrentAssert 0x8281FFF8, DisplayCallstack 0x828200F8, DoAssert 0x82820338,
    // DisplayAssertScreen 0x82820210).
    class Manager
    {
    public:
        Manager();

        void RegisterAssertHandler(AssertHandler* lpHandler);
        void SetRenderer(Debug2DImmediateRender* lpRenderer);   // CgsAssertManager.h:117
        void SetMapFileReader(MapFile::Reader* lpReader, const char* lpcMapFileName);  // CgsAssertManager.h:120
        void HandleAssert(const char* lpcMessage, const char* lpcFile, s32 liLine);

        // The pending assert (for DebugManager::RenderAssert, the on-screen overlay).
        bool              HasAssert() const     { return mbGotAssert; }
        const AssertData& GetAssertData() const { return mCurrentAssert; }

    private:
        void ExecuteAssertHandlers();        // CgsAssertManager.h:160
        void DoAssert();                     // CgsAssertManager.h:154 - the blocking on-screen halt loop
        void DisplayAssertScreen();          // CgsAssertManager.h:174 - render one frame of the dialog + present
        void DisplayCurrentAssert();         // CgsAssertManager.h:177
        void DisplayCallstack();             // CgsAssertManager.h:180
        void DrawLine(const char* lpcMessage);                  // CgsAssertManager.h:183
        void DrawLines(const char* lpcMessage);                 // CgsAssertManager.h:186
        void DrawText(s32 liX, s32 liY, const char* lpcMessage);// CgsAssertManager.h:189
        bool CanRenderAssert() const;        // CgsAssertManager.h:171
        void ClearCurrentAssert();           // CgsAssertManager.h:166

        Debug2DImmediateRender* mpRender;          // the 2D renderer (X360: static; wired by SetRenderer)
        MapFile::Reader*        mpMapFileReader;   // the call-stack symbol resolver (X360: static)
        const char*             mpcMapFileName;    // the map file the reader opens (X360: static)
        AssertHandler*          mpAssertHandlerList;
        AssertData              mCurrentAssert;
        VectorFont              mVectorFont;
        bool                    mbInitialised;
        bool                    mbGotAssert;
        bool                    mbInAssert;         // re-entrancy guard (X360 byte_83019201): inside the halt loop
        s16                     miDrawPositionY;    // running text Y, advanced per drawn line
        s32                     miAssertCount;      // total asserts seen (for the log)
    };

    extern Manager gAssertManager;
}
}
