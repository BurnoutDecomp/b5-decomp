#ifndef GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_MODULE_DEBUG_PRINTER_H
#define GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_MODULE_DEBUG_PRINTER_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"        // Array<s32,20> (mStringIndices)
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"   // CgsContainers::ObjectPool<LogString,20,int> (mStringPool)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h" // CgsDev::DebugRender::Justification, RGBA

// ============================================================================
// GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h
//
// The Director's two on-screen debug-text helpers. Recovered from the DecFIGS DWARF
// (BrnDirectorModuleDebugPrinter.h) for declaration SHAPE, with member layout/offsets and
// behaviour pinned against the X360 ARTIST asm (Construct 0x821F7108, ActualPrint 0x821F71D8,
// DebugLog::Print 0x8221BAC8).
//
//   DebugPrinter -- a single self-advancing text cursor. Construct() seeds its size/line-height
//   and X/Y from the debug-UI Metrics (screen-width*0.25, screen-height*0.10) and a fixed colour
//   palette (white / green / red); ActualPrint() draws one justified line through the buffered
//   debug renderer and steps the Y cursor down by one line height.
//
//   DebugLog -- a fixed pool of 20 timed colour strings (LogString); Print() replays every live
//   entry through a DebugPrinter; Update()/ActualAppend() age and recycle the entries. This is the
//   SINGLE definition of DebugLog (its nested LogString included); BrnDebugLogString.h is a thin
//   shim that includes this header so the ObjectPool<LogString,...> instantiation TU keeps working.
//
// Member offsets verified against the asm:
//   DebugPrinter::mDebugPrinterInfo @ +0x00 (8x 4-byte fields), mbEnabled @ +0x20.
//   DebugLog::mfStringDuration @ +0x00, mStringPool @ +0x08 (ObjectPool 8-byte aligned by its
//   trailing BitArray<20> u64), mStringIndices @ +0x608 (its count word read at +0x608+0x50).
// ============================================================================

namespace BrnDirector
{
    class Moment;   // PrintName / AppendName take a const Moment& (full type not needed here)

    // ------------------------------------------------------------------------
    // DebugPrinter -- one self-advancing on-screen text cursor.
    // ------------------------------------------------------------------------
    struct DebugPrinter
    {
        // The cursor's render parameters (the X360 mDebugPrinterInfo block, +0x00..+0x1F).
        struct DebugPrinterInfo
        {
            f32                            mfSize;            // +0x00 text size
            f32                            mfLineSize;        // +0x04 vertical advance per line
            f32                            mfX;               // +0x08 current cursor X
            f32                            mfY;               // +0x0C current cursor Y (advances down)
            CgsDev::RGBA                   muColour;          // +0x10 default text colour
            CgsDev::RGBA                   muActiveColour;    // +0x14 "active" text colour
            CgsDev::RGBA                   muInactiveColour;  // +0x18 "inactive" text colour
            CgsDev::DebugRender::Justification mJustification; // +0x1C text justification
        };

        // DebugLog::Print drives a DebugPrinter through the private ActualPrint path.
        friend struct DebugLog;

        // X360 0x821F7108: seed mDebugPrinterInfo from the debug-UI Metrics + fixed palette,
        // and enable the printer.
        void Construct();

        // Public draw entry points. The X360 build inlined these one-line forwarders into their
        // callers (no standalone addresses), so ActualPrint is the out-of-line worker and each
        // forwarder shows up only as its own colour argument at the call site. DWARF-attested
        // shapes (BrnDirectorModuleDebugPrinter.h).
        //
        // BODIED as the inline forwarder the console emits (2026-08-01). Recovered from the
        // COLOUR REGISTER at all twelve X360 call sites of ActualPrint @0x821F71D8: the ones that
        // spell the default-colour Print load r5 from the printer's OWN +0x10, i.e.
        // mDebugPrinterInfo.muColour -- `mr r3, printer; lwz r5, 0x10(r3); bl ActualPrint`. Eight
        // sites do exactly that: Tweaker::Render @0x8220BE88 (x4), ValidityAccount::Print
        // @0x82220E68, ArbStateCrashing::{SelectNormalCrashCamera @0x82254FB0 (x4, mixed with
        // literals), Update @0x8226BFB0}, ArbStateTestbed::Update @0x8226B638.
        //
        // ⓘ FOR WHOEVER NEEDS PrintInactive: the same survey shows THREE sites loading r5 from
        // +0x18 == mDebugPrinterInfo.muInactiveColour (MomentController::UpdateAllMoments
        // @0x82239DE8, BehaviourManager::UpdateAllBehaviours @0x82251960, MainDirector::Update
        // @0x82274070) -- that is PrintInactive, and its body is the same forwarder over
        // muInactiveColour. Left declaration-only because nothing in the tree calls it yet and an
        // unused body cannot be link-verified. NO call site in the export set loads +0x14, so
        // PrintActive has no attestation at all -- do not guess it by symmetry.
        void Print(const char* lpcMessage) { ActualPrint(lpcMessage, mDebugPrinterInfo.muColour); }

        // BODIED as the inline forwarder the console actually emits (2026-08-01). Every X360 call
        // site of the explicit-colour Print -- BehaviourIceAnim::Update @0x82247108's two
        // "Can/Can't see player" lines among them -- shows a DIRECT call to ActualPrint
        // @0x821F71D8 with (r3 = the printer, r4 = the text, r5 = the packed RGBA); i.e. the
        // forwarder inlined away, which is exactly this body. Reproducing it here is what lets a
        // caller spell the operation the way the source did instead of reaching the PRIVATE
        // worker. (See BrnBehaviourIceAnim.cpp, whose fabricated
        // `static void ActualPrint(void*, const char*, s32)` slice this retires.)
        void Print(const char* lpcMessage, CgsDev::RGBA luColour) { ActualPrint(lpcMessage, luColour); }
        void PrintName(const Moment& lrMoment, CgsDev::RGBA luColour);
        void PrintActive(const char* lpcMessage);
        void PrintInactive(const char* lpcMessage);

        const DebugPrinterInfo& GetDebugPrinterInfo() const { return mDebugPrinterInfo; }
        void SetDebugPrinterInfo(const DebugPrinterInfo& lrInfo) { mDebugPrinterInfo = lrInfo; }
        void SetEnabled()  { mbEnabled = true; }
        void SetDisabled() { mbEnabled = false; }

    private:
        // X360 0x821F71D8: if enabled, draw one justified line of lpcMessage at the current
        // cursor through the buffered debug renderer, then advance mfY by mfLineSize.
        void ActualPrint(const char* lpcMessage, CgsDev::RGBA luColour);
        void ActualPrintName(const Moment& lrMoment, CgsDev::RGBA luColour);

        DebugPrinterInfo mDebugPrinterInfo;   // +0x00
        bool             mbEnabled;           // +0x20
    };

    // ------------------------------------------------------------------------
    // DebugLog -- a fixed pool of 20 timed colour strings, replayed each frame.
    // ------------------------------------------------------------------------
    struct DebugLog
    {
        // One pooled, timed log line (X360 BrnDirectorModuleDebugPrinter.h:176). Layout/order/types
        // verbatim from the DecFIGS DWARF: +0x00 mfTimeLeft, +0x04 mRGBA, +0x08 macString[64];
        // sizeof == 72 (0x48) == the ObjectPool<LogString,20,int> slot stride.
        struct LogString
        {
            f32          mfTimeLeft;     // +0x00  remaining on-screen time (seconds)
            CgsDev::RGBA mRGBA;          // +0x04  packed RGBA8 text colour
            char         macString[64];  // +0x08  the log text
        };

        void Construct();
        void Clear();
        void Update(f32 lfTimestep);

        // X360 0x8221BAC8: replay every live pool entry, in mStringIndices order, through
        // lrDebugPrinter.ActualPrint(text, colour).
        void Print(DebugPrinter& lrDebugPrinter);

        void Append(const char* lpcString, CgsDev::RGBA lRGBA);
        void AppendName(const Moment& lrMoment, CgsDev::RGBA lRGBA);

    private:
        void ActualAppend(const char* lpcString, CgsDev::RGBA lRGBA);
        void ActualAppendName(const Moment& lrMoment, CgsDev::RGBA lRGBA);

        f32 mfStringDuration;   // +0x00  on-screen lifetime handed to each new entry
        // (+0x04 implicit pad: mStringPool is 8-byte aligned by its trailing BitArray<20> u64)
        CgsContainers::ObjectPool<LogString, 20, int> mStringPool;   // +0x08
        Array<s32, 20>                                mStringIndices; // +0x608 (live order, count @ +0x50)
    };
}

#endif // GAMESOURCE_DIRECTOR_DIRECTORMODULE_BRN_DIRECTOR_MODULE_DEBUG_PRINTER_H
