#include "CgsModuleSingleBuffered.h"

#include <GameShared/GameClasses/Core/CgsAssert.h>

void CgsModule::ModuleSingleBuffered::Construct()
{
    this->mePrepareStage = EManagerPrepareStage::E_MANAGERPREPARESTAGE_START;
    this->meReleaseStage = EManagerReleaseStage::E_MANAGERRELEASESTAGE_START;

    this->mbIsNewModule = false;

    mInputBuffer.Construct();
    mOutputBuffer.Construct();

    this->mpInputStructure = nullptr;
    this->mpOutputStructure = nullptr;
}

bool CgsModule::ModuleSingleBuffered::Prepare()
{
    switch (mePrepareStage)
    {
        case EManagerPrepareStage::E_MANAGERPREPARESTAGE_START:
            mePrepareStage = EManagerPrepareStage::E_MANAGERPREPARESTAGE_START;
            mpInputStructure  = nullptr;
            mpOutputStructure = nullptr;
        case EManagerPrepareStage::E_MANAGERPREPARESTAGE_INPUT:
            mePrepareStage = EManagerPrepareStage::E_MANAGERPREPARESTAGE_INPUT;
            if (!mbIsNewModule)
            {
                mInputBuffer.Prepare();
            }
        case EManagerPrepareStage::E_MANAGERPREPARESTAGE_ALLOCINPUT:
            // FLAG: ARTIST Prepare @ 0x8286E824 sets the stage to ALLOCINPUT(2) here (li r11,2;
            // stw r11,8(r31)) before CreateInputDataStructure (DecFIGS LABEL_5: v1[2]=2). Was
            // mistakenly ALLOCOUTPUT(4), which corrupted the resume point on a partial Prepare.
            mePrepareStage = EManagerPrepareStage::E_MANAGERPREPARESTAGE_ALLOCINPUT;
            if (!mbIsNewModule)
            {
                CgsModule::DataStructure *lpDataStructure = CreateInputDataStructure();
                if (!lpDataStructure)
                    return false;
                mInputBuffer.SetDataStructure(lpDataStructure);
            }
        case EManagerPrepareStage::E_MANAGERPREPARESTAGE_OUTPUT:
            mePrepareStage = EManagerPrepareStage::E_MANAGERPREPARESTAGE_OUTPUT;
            if (!mbIsNewModule)
            {
                mOutputBuffer.Prepare();
            }
        case EManagerPrepareStage::E_MANAGERPREPARESTAGE_ALLOCOUTPUT:
            mePrepareStage = EManagerPrepareStage::E_MANAGERPREPARESTAGE_ALLOCOUTPUT;
            if (!mbIsNewModule)
            {
                CgsModule::DataStructure * dsOut = CreateOutputDataStructure();
                if (!dsOut)
                    return false;
                mOutputBuffer.SetDataStructure(dsOut);
            }
        case EManagerPrepareStage::E_MANAGERPREPARESTAGE_DATASTRUCTURES:
            mePrepareStage = EManagerPrepareStage::E_MANAGERPREPARESTAGE_DATASTRUCTURES;
            if (!mbIsNewModule)
            {
                if (!mpInputStructure)
                    LockInputForWrite();

                if (!mpOutputStructure)
                    LockOutputForWrite();

                if (!PrepareDataStructures(mpInputStructure, mpOutputStructure))
                    return false;

                UnlockInputForWrite();
                UnlockOutputForWrite();
            }
        case EManagerPrepareStage::E_MANAGERPREPARESTAGE_DONE:
            this->meReleaseStage = EManagerReleaseStage::E_MANAGERRELEASESTAGE_START;
            this->mePrepareStage = EManagerPrepareStage::E_MANAGERPREPARESTAGE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Unknown prepare stage");

            return false;
    }
}

bool CgsModule::ModuleSingleBuffered::Release()
{
    switch (meReleaseStage)
    {
        case EManagerReleaseStage::E_MANAGERRELEASESTAGE_START:
            meReleaseStage = EManagerReleaseStage::E_MANAGERRELEASESTAGE_START;
            mpInputStructure  = nullptr;
            mpOutputStructure = nullptr;
        case EManagerReleaseStage::E_MANAGERRELEASESTAGE_DATASTRUCTURES:
            meReleaseStage = EManagerReleaseStage::E_MANAGERRELEASESTAGE_DATASTRUCTURES;
            if (!mbIsNewModule)
            {
                if (!mpInputStructure)
                    LockInputForWrite();
                if (!mpOutputStructure)
                    LockOutputForWrite();

                if (!ReleaseDataStructures(mpInputStructure, mpOutputStructure))
                    return false;

                UnlockInputForWrite();
                UnlockOutputForWrite();
            }
        case EManagerReleaseStage::E_MANAGERRELEASESTAGE_FREEINPUT:
            meReleaseStage = EManagerReleaseStage::E_MANAGERRELEASESTAGE_FREEINPUT;
            if (!mbIsNewModule)
            {
                CgsModule::DataStructure* lpDataStructure = mInputBuffer.GetDataStructure();
                if (!DestroyInputDataStructure(lpDataStructure))
                    return false;
                mInputBuffer.SetDataStructure(nullptr);
            }
        case EManagerReleaseStage::E_MANAGERRELEASESTAGE_INPUT:
            meReleaseStage = EManagerReleaseStage::E_MANAGERRELEASESTAGE_INPUT;
            if (!mbIsNewModule)
            {
                mInputBuffer.Release();
            }
        case EManagerReleaseStage::E_MANAGERRELEASESTAGE_FREEOUTPUT:
            meReleaseStage = EManagerReleaseStage::E_MANAGERRELEASESTAGE_FREEOUTPUT;
            if (!mbIsNewModule)
            {
                CgsModule::DataStructure *lpDataStructure = mOutputBuffer.GetDataStructure();
                if (!DestroyOutputDataStructure(lpDataStructure))
                    return false;
                mOutputBuffer.SetDataStructure(nullptr);
            }
        case EManagerReleaseStage::E_MANAGERRELEASESTAGE_OUTPUT:
            meReleaseStage = EManagerReleaseStage::E_MANAGERRELEASESTAGE_OUTPUT;
            if (!mbIsNewModule)
            {
                mOutputBuffer.Release();
            }
        case EManagerReleaseStage::E_MANAGERRELEASESTAGE_DONE:
            mePrepareStage = EManagerPrepareStage::E_MANAGERPREPARESTAGE_START;
            meReleaseStage = EManagerReleaseStage::E_MANAGERRELEASESTAGE_DONE;
            return true;
        default:
            CGS_ASSERT(false, "Unknown release stage");

            return false;
    }
}

void CgsModule::ModuleSingleBuffered::Destruct()
{
    this->mpInputStructure = nullptr;
    this->mpOutputStructure = nullptr;
    mInputBuffer.Destruct();
    mOutputBuffer.Destruct();
}

void CgsModule::ModuleSingleBuffered::Update()
{
    // Empty on purpose as this is for derived classes to implement
}

void CgsModule::ModuleSingleBuffered::SetMultiThreaded(bool isMultiThreaded)
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mInputBuffer.SetMultiThreaded(isMultiThreaded);
    mOutputBuffer.SetMultiThreaded(isMultiThreaded);
}

void CgsModule::ModuleSingleBuffered::LockForInput()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mInputBuffer.LockForWrite();
    mpInputStructure = mInputBuffer.GetDataStructure();
}

void CgsModule::ModuleSingleBuffered::UnlockForInput()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mInputBuffer.UnlockForWrite();
    mpInputStructure = nullptr;
}

void CgsModule::ModuleSingleBuffered::LockForOutput()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mOutputBuffer.LockForRead();
    mpOutputStructure = mOutputBuffer.GetDataStructure();
}

void CgsModule::ModuleSingleBuffered::UnlockForOutput()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mOutputBuffer.UnlockForRead();
    mpOutputStructure = nullptr;
}

CgsModule::DataStructure* CgsModule::ModuleSingleBuffered::GetInputStructure() const
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    return mpInputStructure;
}

CgsModule::DataStructure* CgsModule::ModuleSingleBuffered::GetOutputStructure() const
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    return mpOutputStructure;
}

CgsModule::DataStructure* CgsModule::ModuleSingleBuffered::CreateInputDataStructure()
{
    CGS_ASSERT(mbIsNewModule, "This is a new module type - can't lock/unlock etc etc\n");

    return nullptr; // Placeholder for derived classes to implement
}

CgsModule::DataStructure* CgsModule::ModuleSingleBuffered::CreateOutputDataStructure()
{
    CGS_ASSERT(mbIsNewModule, "This is a new module type - can't lock/unlock etc etc\n");

    return nullptr; // Placeholder for derived classes to implement
}

bool CgsModule::ModuleSingleBuffered::DestroyInputDataStructure(DataStructure* lpDataStructure)
{
    CGS_ASSERT(mbIsNewModule, "This is a new module type - can't lock/unlock etc etc\n");

    return false; // Placeholder for derived classes to implement
}

bool CgsModule::ModuleSingleBuffered::DestroyOutputDataStructure(DataStructure* lpDataStructure)
{
    CGS_ASSERT(mbIsNewModule, "This is a new module type - can't lock/unlock etc etc\n");

    return false; // Placeholder for derived classes to implement
}

bool CgsModule::ModuleSingleBuffered::PrepareDataStructures(DataStructure* lpInputDataStructure, DataStructure* lpOutputDataStructure)
{
    CGS_ASSERT(mbIsNewModule, "This is a new module type - can't lock/unlock etc etc\n");

    return false; // Placeholder for derived classes to implement
}

bool CgsModule::ModuleSingleBuffered::ReleaseDataStructures(DataStructure* lpInputDataStructure, DataStructure* lpOutputDataStructure)
{
    CGS_ASSERT(mbIsNewModule, "This is a new module type - can't lock/unlock etc etc\n");

    return false; // Placeholder for derived classes to implement
}

CgsModule::DataStructure* CgsModule::ModuleSingleBuffered::LockInputForRead()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mInputBuffer.LockForRead();
    mpInputStructure = mInputBuffer.GetDataStructure();
    return mpInputStructure;
}

void CgsModule::ModuleSingleBuffered::UnlockInputForRead()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mInputBuffer.UnlockForRead();
    mpInputStructure = nullptr;
}

CgsModule::DataStructure *CgsModule::ModuleSingleBuffered::LockOutputForWrite()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mOutputBuffer.LockForWrite();
    mpOutputStructure = mOutputBuffer.GetDataStructure();
    return mpOutputStructure;
}

void CgsModule::ModuleSingleBuffered::UnlockOutputForWrite()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mOutputBuffer.UnlockForWrite();
    mpOutputStructure = nullptr;
}

CgsModule::DataStructure *CgsModule::ModuleSingleBuffered::LockInputForWrite()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mInputBuffer.LockForWrite();
    mpInputStructure = mInputBuffer.GetDataStructure();
    return mpInputStructure;
}

void CgsModule::ModuleSingleBuffered::UnlockInputForWrite()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mInputBuffer.UnlockForWrite();
    mpInputStructure = nullptr;
}

CgsModule::DataStructure* CgsModule::ModuleSingleBuffered::LockOutputForRead()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mOutputBuffer.LockForRead();
    mpOutputStructure = mOutputBuffer.GetDataStructure();
    return mpOutputStructure;
}

void CgsModule::ModuleSingleBuffered::UnlockOutputForRead()
{
    CGS_ASSERT(!mbIsNewModule, "This is an old module type - must use old lock/unlock etc etc\n");
    mOutputBuffer.UnlockForRead();
    mpOutputStructure = nullptr;
}
