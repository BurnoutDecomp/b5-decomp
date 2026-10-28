#pragma once

#include "CgsDataStructure.h"

namespace CgsModule
{
    // ARTIST 0x825AE9E0 dispatches Construct at vtable slot zero.
    // This interface has no virtual destructor; lifetime uses Destruct().
    class Module
    {
    public:
        //Module();
        //Module(const Module& other);

        virtual void Construct() = 0;
        virtual bool Prepare() = 0;
        virtual bool Release() = 0;
        virtual void Destruct() = 0;
        virtual void Update() = 0;
        virtual void SetMultiThreaded(bool isMultiThreaded) = 0;
        virtual void LockForInput() = 0;
        virtual void UnlockForInput() = 0;
        virtual void LockForOutput() = 0;
        virtual void UnlockForOutput() = 0;

    protected:
        bool mbIsNewModule;

        virtual bool DestroyInputDataStructure(DataStructure* pDataStructure) = 0;
        virtual bool DestroyOutputDataStructure(DataStructure* pDataStructure) = 0;
        virtual bool PrepareDataStructures(DataStructure* pInputDataStructure, DataStructure* pOutputDataStructure) = 0;
        virtual bool ReleaseDataStructures(DataStructure* pInputDataStructure, DataStructure* pOutputDataStructure) = 0;
    };
}
