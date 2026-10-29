#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/MapFile/Reader/CgsMapFileReader.h"
#include "GameShared/GameClasses/Development/MapFile/CgsMapFile.h"
#include <cstddef>
#include <cstdio>

namespace CgsDev
{
namespace MapFile
{
    // Native ARTIST reader: asynchronous by default, with ten resolved-name
    // slots following the 4096-byte streaming buffer at this+0x11.
    struct MinimalMemoryReader : public Reader
    {
        MinimalMemoryReader();
        virtual void Prepare(const char* lpcMapFileName, StackUnpickBase* lpCallstack);
        virtual void Update();
        virtual const char* GetStackEntryName(s32 liIndex);
        void SetAsyncronousMode(bool lbAsynchronous);

    protected:
        void ReadRecords();
        static const s32 KI_BUFFER_SIZE = 4096;
        static const s32 KI_MAX_STACK_RESULTS = 10;

        bool mbAsycronous;
        std::FILE* pFileHandle;
        bool mbFinished;
        u8 maReadBuffer[KI_BUFFER_SIZE];
        char maacStackNames[KI_MAX_STACK_RESULTS][KI_NAME_LENGTH];

        void CheckLayout()
        {
            CGS_STATIC_ASSERT(offsetof(MinimalMemoryReader, mbAsycronous) == 8, "ARTIST async flag");
            CGS_STATIC_ASSERT(offsetof(MinimalMemoryReader, pFileHandle) == 12, "ARTIST file handle");
            CGS_STATIC_ASSERT(offsetof(MinimalMemoryReader, mbFinished) == 16, "ARTIST completion flag");
            CGS_STATIC_ASSERT(offsetof(MinimalMemoryReader, maReadBuffer) == 17, "ARTIST streaming buffer");
            CGS_STATIC_ASSERT(offsetof(MinimalMemoryReader, maacStackNames) == 0x1011, "ARTIST name cache");
        }
    };
    CGS_STATIC_ASSERT(sizeof(MinimalMemoryReader) == 0x14C4, "ARTIST native map reader");
}
}
