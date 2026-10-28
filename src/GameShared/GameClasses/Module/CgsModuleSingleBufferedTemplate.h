#pragma once

#include "CgsModuleSingleBuffered.h"
#include <GameShared/GameClasses/Core/CgsAssert.h>

namespace CgsModule
{
    template<class TInputBuffer, class TOutputBuffer>
    class ModuleSingleBufferedTemplate : public ModuleSingleBuffered
    {
    public:
        const TInputBuffer* SafeLockInputForRead()
        {
            const auto* lpBuffer = reinterpret_cast<const TInputBuffer*>(LockInputForRead());
            CGS_ASSERT(lpBuffer, "lpBuffer != NULL");
            return lpBuffer;
        }

        TOutputBuffer* SafeLockOutputForWrite()
        {
            auto* lpBuffer = reinterpret_cast<TOutputBuffer*>(LockOutputForWrite());
            CGS_ASSERT(lpBuffer, "lpBuffer != NULL");
            return lpBuffer;
        }

        TInputBuffer* SafeGetInputStructure() const
        {
            auto* lpBuffer = reinterpret_cast<TInputBuffer*>(GetInputStructure());
            CGS_ASSERT(lpBuffer, "lpBuffer != NULL");
            return lpBuffer;
        }

        TOutputBuffer* SafeGetOutputStructure() const
        {
            auto* lpBuffer = reinterpret_cast<TOutputBuffer*>(GetOutputStructure());
            CGS_ASSERT(lpBuffer, "lpBuffer != NULL");
            return lpBuffer;
        }

        void ClearInputBuffer()
        {
            auto* lpBuffer = reinterpret_cast<TInputBuffer*>(GetInputStructure());
            CGS_ASSERT(lpBuffer, "lpBuffer != NULL");
            lpBuffer->Clear();
        }

        void ClearOutputBuffer()
        {
            auto* lpBuffer = reinterpret_cast<TOutputBuffer*>(GetOutputStructure());
            CGS_ASSERT(lpBuffer, "lpBuffer != NULL");
            lpBuffer->Clear();
        }

    protected:

        CgsModule::DataStructure* CreateInputDataStructure()
        {
            TInputBuffer* lpInput = (TInputBuffer*)&mInputBuffer;
            lpInput->Construct();
            return lpInput;
        }

        CgsModule::DataStructure* CreateOutputDataStructure()
        {
            TOutputBuffer* lpOutput = (TOutputBuffer*)&mOutputBuffer;
            lpOutput->Construct();
            return lpOutput;
        }

        bool DestroyInputDataStructure(DataStructure *lpDataStructure)
        {
            TInputBuffer* lpInput = (TInputBuffer*)lpDataStructure;
            lpInput->Destruct();

            return true;
        }

        bool DestroyOutputDataStructure(DataStructure *lpDataStructure)
        {
            TOutputBuffer* lpOutput = (TOutputBuffer*)lpDataStructure;
            lpOutput->Destruct();

            return true;
        }

        bool PrepareDataStructures(DataStructure *lpInputDataStructure, DataStructure *lpOutputDataStructure)
        {
            TInputBuffer* lpInput = (TInputBuffer*)lpInputDataStructure;
            TOutputBuffer* lpOutput = (TOutputBuffer*)lpOutputDataStructure;

            bool lbDone = true;
            lbDone = lpInput->Prepare() && lbDone;
            lbDone = lpOutput->Prepare() && lbDone;
            return lbDone;
        }

        bool ReleaseDataStructures(DataStructure *lpInputDataStructure, DataStructure *lpOutputDataStructure)
        {
            TInputBuffer* lpInput = (TInputBuffer*)lpInputDataStructure;
            TOutputBuffer* lpOutput = (TOutputBuffer*)lpOutputDataStructure;

            bool lbDone = true;
            lbDone = lpInput->Release() && lbDone;
            lbDone = lpOutput->Release() && lbDone;
            return lbDone;
        }

    private:
        TInputBuffer mInputBuffer;
        TOutputBuffer mOutputBuffer;
    };
}
