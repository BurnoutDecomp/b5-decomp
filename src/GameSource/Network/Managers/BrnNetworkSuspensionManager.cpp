#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/BrnNetworkManager.h"

namespace BrnNetwork
{
    void SuspensionManager::Construct(BrnNetworkManager* lpNetworkManager)
    {
        mCallback = nullptr;
        mpUserData = nullptr;
        mpNetworkManager = lpNetworkManager;
        meSuspensionType = E_SUSPENSION_TYPE_COUNT;
        meSubState = E_SUBSTATE_COUNT;
    }

    bool SuspensionManager::Prepare()
    {
        mCallback = nullptr;
        mpUserData = nullptr;
        meSuspensionType = E_SUSPENSION_TYPE_COUNT;
        meSubState = E_SUBSTATE_COUNT;
        return true;
    }

    bool SuspensionManager::Release()
    {
        mCallback = nullptr;
        mpUserData = nullptr;
        meSuspensionType = E_SUSPENSION_TYPE_COUNT;
        meSubState = E_SUBSTATE_COUNT;
        return true;
    }

    void SuspensionManager::Destruct()
    {
        mCallback = nullptr;
        mpUserData = nullptr;
        mpNetworkManager = nullptr;
        meSuspensionType = E_SUSPENSION_TYPE_COUNT;
        meSubState = E_SUBSTATE_COUNT;
    }

    void SuspensionManager::Resume(Callback lpfCallback, void* lpUserData)
    {
        CGS_ASSERT(lpfCallback, "lCallback");
        CGS_ASSERT(meSubState == E_SUBSTATE_COUNT, "meSubState == E_SUBSTATE_COUNT");

        mCallback = lpfCallback;
        mpUserData = lpUserData;
        meSubState = E_SUBSTATE_WAIT_TO_RESUME;
    }

    void SuspensionManager::Suspend(ESuspensionType leSuspensionType,
                                    Callback lpfCallback,
                                    void* lpUserData)
    {
        CGS_ASSERT(lpfCallback, "lCallback");
        CGS_ASSERT(meSubState == E_SUBSTATE_COUNT,
                   "meSubState == E_SUBSTATE_COUNT");
        CGS_ASSERT(leSuspensionType >= E_SUSPENSION_TYPE_OFFLINE
                       && leSuspensionType < E_SUSPENSION_TYPE_COUNT,
                   "(leSuspensionType >= E_SUSPENSION_TYPE_OFFLINE) && "
                   "(leSuspensionType < E_SUSPENSION_TYPE_COUNT)");

        mCallback = lpfCallback;
        mpUserData = lpUserData;

        s32 liSuspensionFlags = 0;
        if (leSuspensionType == E_SUSPENSION_TYPE_OFFLINE
            || leSuspensionType == E_SUSPENSION_TYPE_IN_GAME)
        {
            liSuspensionFlags = 16;
        }

        mpNetworkManager->GetServerInterface()->Suspend(liSuspensionFlags);
        meSubState = E_SUBSTATE_SUSPENDING;
    }

    void SuspensionManager::Disconnected()
    {
        mCallback = nullptr;
        mpUserData = nullptr;
        meSubState = E_SUBSTATE_COUNT;
    }

    bool SuspensionManager::IsSuspending() const
    {
        return meSubState != E_SUBSTATE_COUNT;
    }

    void SuspensionManager::Update()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        const BrnServerInterface::EStatus leStatus = lpServerInterface->GetStatus(12);

        switch (meSubState)
        {
        case E_SUBSTATE_SUSPENDING:
            if (leStatus == BrnServerInterface::E_STATUS_ERROR)
                Finished(false);
            else if (leStatus == BrnServerInterface::E_STATUS_IDLE)
                Finished(true);
            else
                CGS_ASSERT(leStatus == BrnServerInterface::E_STATUS_BUSY,
                           "Invalid state returned by mpNetworkManager->GetServerInterface()->GetStatus() in SuspensionManager::Update");
            break;

        case E_SUBSTATE_WAIT_TO_RESUME:
            if (leStatus == BrnServerInterface::E_STATUS_IDLE)
            {
                if (lpServerInterface->IsSuspended())
                {
                    lpServerInterface->Resume();
                    meSubState = E_SUBSTATE_RESUMING;
                }
                else
                {
                    Finished(true);
                }
            }
            break;

        case E_SUBSTATE_RESUMING:
            if (leStatus == BrnServerInterface::E_STATUS_ERROR)
            {
                Finished(false);
            }
            else if (leStatus == BrnServerInterface::E_STATUS_IDLE)
            {
                Finished(lpServerInterface->GetConnectionComponent()->IsLoggedIn());
            }
            else
            {
                CGS_ASSERT(leStatus == BrnServerInterface::E_STATUS_BUSY,
                           "Invalid state returned by mpNetworkManager->GetServerInterface()->GetStatus() in SuspensionManager::Update");
            }
            break;

        case E_SUBSTATE_COUNT:
        default:
            break;
        }
    }
}
