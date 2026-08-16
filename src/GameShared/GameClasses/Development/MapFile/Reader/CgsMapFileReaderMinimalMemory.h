#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReader.h"   // Reader base
#include "GameShared/GameClasses/Development/MapFile/CgsMapFile.h"                // KI_NAME_LENGTH

#include <cstdio>   // std::FILE

// CgsDev::MapFile::MinimalMemoryReader - the function-map reader the assert manager uses (the X360
// off_83018F20 concrete type). It never loads the whole map: it streams the record array a 4 KB buffer
// at a time, and for each record checks whether any not-yet-resolved call-stack frame's address lies in
// that record's [mAddress, mAddress+muSize) range, caching the matched function name. Update() reads one
// buffer per call (incremental); synchronous mode reads the whole map up front in Prepare. Decompiled
// from the X360 (Prepare 0x828271C8, Update 0x8281AEE8, ReadRecords 0x8281A5A8,
// GetStackEntryName 0x82820C70).

namespace CgsDev
{
namespace MapFile
{
    struct MinimalMemoryReader : public Reader
    {
        MinimalMemoryReader();

        virtual void        Prepare(const char* lpcMapFileName, StackUnpickBase* lpCallstack);
        virtual void        Update();
        virtual const char* GetStackEntryName(s32 liIndex);

        void SetAsyncronousMode(bool lbAsynchronous);

        // ⭐ ADDITIVE (2026-08-16), NON-VIRTUAL so the console-attested vtable is untouched.
        // The byte offset of frame liIndex INSIDE the function GetStackEntryName just named,
        // or 0 when the frame did not resolve. A bare function name locates a crash to a
        // 7 KB function; "Name + 0x2F1C" locates it to a statement once the matching build's
        // .map is in hand. WriteReport prints it; nothing else consumes it.
        u32 GetStackEntryOffset(s32 liIndex) const;

    protected:
        void ReadRecords();   // read + match one buffer's worth of records

        static const s32 KI_BUFFER_SIZE      = 4096;

        // ⭐⭐ RAISED 10 -> 25 (2026-08-16). A DELIBERATE, PC-ONLY DIVERGENCE, and the
        // evidence is two crash reports.
        //
        // StackUnpick CAPTURES KI_MAX_STACK_ADDRESSES == 25 frames (CgsStackUnpick.h:23) and
        // CgsCrashHandlerPC's WriteReport PRINTS all 25 -- but this cache could only ever hold
        // names for the first 10, and GetStackEntryName returns null past that, so
        // WriteReport falls back to printing the raw RVA. Frames 10..24 were UNNAMEABLE no
        // matter what the map contained.
        //
        // On the console that was cheap and sufficient: the assert path spends only 2 frames
        // (HandleAssert + FireAssert) before the first game frame, so 10 results named 8 real
        // frames. THE PC CRASH PATH DOES NOT: the filter runs on the faulting thread, so the
        // capture begins inside the reporter and burns EIGHT frames -- WriteReport,
        // CrashFilter, and the six ntdll/KERNELBASE exception-dispatch frames -- before the
        // first game frame appears. That left exactly TWO game frames nameable.
        //
        // MEASURED, not reasoned, on a real player's report (BrnGame.log, 2026-08-15): the
        // callstack named frames 0..9 and then printed 0x16B8B6 / 0x16950A / 0x15A63 / 0x1651C
        // raw -- and those four ARE public symbols (MainDirector::PreSceneQueryUpdate, its
        // caller, BrnGameModule::DoUpdate_Director, BrnGameModule::GameMain). The cut is at
        // index 10 exactly. Our own assert blocks show the same cut in the same place: ten
        // names, then raw hex. The published build ships no .map, so those RVAs could not be
        // resolved after the fact either -- the report was undiagnosable BY THIS LIMIT ALONE.
        //
        // Cost is 15 * KI_NAME_LENGTH == 1800 bytes on a single global reader. Keep this
        // >= StackUnpickBase::KI_MAX_STACK_ADDRESSES or frames go unnamed again.
        static const s32 KI_MAX_STACK_RESULTS = 25;

        bool       mbAsycronous;
        std::FILE* pFileHandle;
        bool       mbFinished;
        u8         maReadBuffer[KI_BUFFER_SIZE];
        char       maacStackNames[KI_MAX_STACK_RESULTS][KI_NAME_LENGTH];
        u32        maauStackOffsets[KI_MAX_STACK_RESULTS];   // frame addr - record base, per resolved frame
    };
}
}
