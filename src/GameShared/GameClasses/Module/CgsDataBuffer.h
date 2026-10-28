#pragma once

#include "types.hpp"

#include <GameShared/GameClasses/Containers/CgsFlagSet.h>
#include "CgsDataStructure.h"
#include <eathread/eathread_rwmutex.h>

namespace CgsModule
{
    class DataBuffer
    {
        EA::Thread::RWMutex mMutex;
        DataStructure* mpDataStructure;

        typedef CgsContainers::FlagSet<int8_t> FlagSet8;
        FlagSet8 mxStatusFlags;

        bool mbMultiThreaded;

    public:
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();
        void SetDataStructure(DataStructure* pDataStructure);
        DataStructure* GetDataStructure() const { return mpDataStructure; }

        void SetMultiThreaded(bool isMultiThreaded) { mbMultiThreaded = isMultiThreaded; }

        void LockForWrite();
        void UnlockForWrite();
        void LockForRead();
        void UnlockForRead();

        bool IsBufferLocked();
    };
}
