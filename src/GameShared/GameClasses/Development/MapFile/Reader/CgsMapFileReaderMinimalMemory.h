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

    protected:
        void ReadRecords();   // read + match one buffer's worth of records

        static const s32 KI_BUFFER_SIZE      = 4096;
        static const s32 KI_MAX_STACK_RESULTS = 10;

        bool       mbAsycronous;
        std::FILE* pFileHandle;
        bool       mbFinished;
        u8         maReadBuffer[KI_BUFFER_SIZE];
        char       maacStackNames[KI_MAX_STACK_RESULTS][KI_NAME_LENGTH];
    };
}
}
