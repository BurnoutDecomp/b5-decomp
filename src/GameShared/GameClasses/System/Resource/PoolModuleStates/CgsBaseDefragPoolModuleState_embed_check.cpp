// Embed-check for GameShared/.../PoolModuleStates/CgsBaseDefragPoolModuleState.cpp.
// Forces a full instantiation of CgsResource::BaseDefragPoolModuleState (incl. its vtable)
// via a trivial concrete subclass, and references the three reconstructed members so the gate
// links the whole TU. Not shipped; lives next to the home.
#include "GameShared/GameClasses/System/Resource/PoolModuleStates/CgsBaseDefragPoolModuleState.h"

namespace
{
    struct ConcreteDefragState : public CgsResource::BaseDefragPoolModuleState
    {
        bool RunDefragAlgorithm(CgsResource::AllocListSet*, CgsResource::LinearHeapNode*, s32, s32) override { return false; }
        void RunPoolDefragmentation(CgsResource::RelocateRequest*, CgsResource::RelocateSource*, u32, s32) override {}
    };
}

int CgsBaseDefragPoolModuleState_EmbedCheck(CgsResource::BaseDefragParams* lpParams, s32 liStartNode)
{
    ConcreteDefragState kState;
    kState.Begin(lpParams);
    int liSum = kState.FindNextFreeNode(liStartNode);
    liSum += kState.BuildFinalRelocationData();
    return liSum;
}
