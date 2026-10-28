#pragma once

#include "CgsModule.h"
#include "CgsDataBuffer.h"
#include <eathread/eathread_rwmutex.h>

namespace CgsModule
{
    class ModuleSingleBuffered : public Module
    {
        enum  EManagerPrepareStage {
            E_MANAGERPREPARESTAGE_START = 0,
            E_MANAGERPREPARESTAGE_INPUT = 1,
            E_MANAGERPREPARESTAGE_ALLOCINPUT = 2,
            E_MANAGERPREPARESTAGE_OUTPUT = 3,
            E_MANAGERPREPARESTAGE_ALLOCOUTPUT = 4,
            E_MANAGERPREPARESTAGE_DATASTRUCTURES = 5,
            E_MANAGERPREPARESTAGE_DONE = 6,
        };

        enum  EManagerReleaseStage {
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

        DataBuffer mInputBuffer;
        DataBuffer mOutputBuffer;

        DataStructure* mpInputStructure;
        DataStructure* mpOutputStructure;

    public:
        void Construct() ;
        bool Prepare() ;
        bool Release() ;
        void Destruct() ;
        void Update() ;
        void SetMultiThreaded(bool isMultiThreaded) ;

        DataStructure* GetOutputStructure() const;

        void LockForInput() ;
        void UnlockForInput() ;
        void LockForOutput() ;
        void UnlockForOutput() ;

    protected:
        virtual DataStructure* CreateInputDataStructure();
        virtual DataStructure* CreateOutputDataStructure();
        bool DestroyInputDataStructure(DataStructure* pDataStructure) ;
        bool DestroyOutputDataStructure(DataStructure* pDataStructure) ;
        bool PrepareDataStructures(DataStructure* pInputDataStructure, DataStructure* pOutputDataStructure) ;
        bool ReleaseDataStructures(DataStructure* pInputDataStructure, DataStructure* pOutputDataStructure) ;

        DataStructure* LockInputForRead();
        void UnlockInputForRead();

        DataStructure* LockOutputForWrite();
        void UnlockOutputForWrite();

        DataStructure* GetInputStructure() const;

    private:
        DataStructure* LockInputForWrite();
        void UnlockInputForWrite();

        DataStructure* LockOutputForRead();
        void UnlockOutputForRead();

    };
}
