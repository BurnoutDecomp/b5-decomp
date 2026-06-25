#pragma once

#include "CgsModule.h"
#include "CgsDataBuffer.h"
#include <eathread/eathread_rwmutex.h>

namespace CgsModule
{
    class ModuleSingleBuffered : public Module
    {
        enum class EManagerPrepareStage {
            E_MANAGERPREPARESTAGE_START = 0,
            E_MANAGERPREPARESTAGE_INPUT = 1,
            E_MANAGERPREPARESTAGE_ALLOCINPUT = 2,
            E_MANAGERPREPARESTAGE_OUTPUT = 3,
            E_MANAGERPREPARESTAGE_ALLOCOUTPUT = 4,
            E_MANAGERPREPARESTAGE_DATASTRUCTURES = 5,
            E_MANAGERPREPARESTAGE_DONE = 6,
        };

        enum class EManagerReleaseStage {
            E_MANAGERRELEASESTAGE_START = 0,
            E_MANAGERRELEASESTAGE_DATASTRUCTURES = 1,
            E_MANAGERRELEASESTAGE_FREEOUTPUT = 2,
            E_MANAGERRELEASESTAGE_OUTPUT = 3,
            E_MANAGERRELEASESTAGE_FREEINPUT = 4,
            E_MANAGERRELEASESTAGE_INPUT = 5,
            E_MANAGERRELEASESTAGE_DONE = 6,
        };

        EManagerPrepareStage mePrepareStage;
        EManagerReleaseStage meReleaseStage;

        EA::Thread::RWMutex mInputMutex;
        EA::Thread::RWMutex mOutputMutex;

        DataBuffer mInputBuffer;
        DataBuffer mOutputBuffer;

        DataStructure* mpInputStructure;
        DataStructure* mpOutputStructure;

    public:
        void Construct() override;
        bool Prepare() override;
        bool Release() override;
        void Destruct() override;
        void Update() override;
        void SetMultiThreaded(bool isMultiThreaded) override;

        DataStructure* GetOutputStructure() const { return mpOutputStructure; }

        void LockForInput() override;
        void UnlockForInput() override;
        void LockForOutput() override;
        void UnlockForOutput() override;

    protected:
        virtual DataStructure* CreateInputDataStructure();
        virtual DataStructure* CreateOutputDataStructure();
        bool DestroyInputDataStructure(DataStructure* pDataStructure) override;
        bool DestroyOutputDataStructure(DataStructure* pDataStructure) override;
        bool PrepareDataStructures(DataStructure* pInputDataStructure, DataStructure* pOutputDataStructure) override;
        bool ReleaseDataStructures(DataStructure* pInputDataStructure, DataStructure* pOutputDataStructure) override;

        DataStructure* LockInputForRead();
        void UnlockInputForRead();

        DataStructure* LockOutputForWrite();
        void UnlockOutputForWrite();

        DataStructure* GetInputStructure() const { return mpInputStructure; }

    private:
        DataStructure* LockInputForWrite();
        void UnlockInputForWrite();

        DataStructure* LockOutputForRead();
        void UnlockOutputForRead();

    };
}
