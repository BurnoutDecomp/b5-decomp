#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkSuspensionManager.cpp"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/Network/BrnNetworkManager.h"

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkSuspensionManager.cpp".

namespace BrnNetwork
{
    void SuspensionManager::Construct(BrnNetworkManager* lpNetworkManager)
    {
        mCallback = 0;
        mpUserData = 0;
        mpNetworkManager = lpNetworkManager;
        meSuspensionType = E_SUSPENSION_TYPE_COUNT;
        meSubState = E_SUBSTATE_COUNT;
    }

    bool SuspensionManager::Prepare()
    {
        mCallback = 0;
        mpUserData = 0;
        meSuspensionType = E_SUSPENSION_TYPE_COUNT;
        meSubState = E_SUBSTATE_COUNT;
        return true;
    }

    bool SuspensionManager::Release()
    {
        mCallback = 0;
        mpUserData = 0;
        meSuspensionType = E_SUSPENSION_TYPE_COUNT;
        meSubState = E_SUBSTATE_COUNT;
        return true;
    }

    void SuspensionManager::Destruct()
    {
        mCallback = 0;
        mpUserData = 0;
        mpNetworkManager = 0;
        meSuspensionType = E_SUSPENSION_TYPE_COUNT;
        meSubState = E_SUBSTATE_COUNT;
    }

    void SuspensionManager::Resume(Callback lpfCallback, void* lpUserData)
    {
        CGS_ASSERT_AT(lpfCallback,
            "lCallback", CGS_ARTIST_FILE, 279 /* ARTIST 0x82544CB8 */);
        CGS_ASSERT_AT(meSubState == E_SUBSTATE_COUNT,
            "meSubState == E_SUBSTATE_COUNT", CGS_ARTIST_FILE, 280 /* ARTIST 0x82544CE0 */);


        mCallback = lpfCallback;
        mpUserData = lpUserData;
        meSubState = E_SUBSTATE_WAIT_TO_RESUME;
    }

    void SuspensionManager::Suspend(ESuspensionType leSuspensionType,
                                    Callback lpfCallback,
                                    void* lpUserData)
    {
        // Inlined into its callers (no out-of-line ARTIST copy), e.g. StateManager::SuspendToLeaveGame.
        CGS_ASSERT_AT(lpfCallback, "lCallback", CGS_ARTIST_FILE, 242 /* ARTIST 0x8256B350 */);
        CGS_ASSERT_AT(meSubState == E_SUBSTATE_COUNT, "meSubState == E_SUBSTATE_COUNT",
                      CGS_ARTIST_FILE, 243 /* ARTIST 0x8256B378 */);
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
        // ARTIST 0x82544D18..0x82544D20 stores +0x00, +0x10, then +0x04.
        mCallback = 0;
        meSubState = E_SUBSTATE_COUNT;
        mpUserData = 0;
    }

    bool SuspensionManager::IsSuspending() const
    {
        return meSubState != E_SUBSTATE_COUNT;
    }

    void SuspensionManager::Update()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        const BrnServerInterface::EStatus leStatus = lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_COUNT);

        switch (meSubState)
        {
        case E_SUBSTATE_SUSPENDING:
            if (leStatus == BrnServerInterface::E_STATUS_ERROR)
                Finished(false);
            else if (leStatus == BrnServerInterface::E_STATUS_IDLE)
                Finished(true);
            else
                CGS_ASSERT_AT(leStatus == BrnServerInterface::E_STATUS_BUSY,
                           "Invalid state returned by mpNetworkManager->GetServerInterface()->GetStatus() in SuspensionManager::Update", CGS_ARTIST_FILE, 212);
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
                CGS_ASSERT_AT(leStatus == BrnServerInterface::E_STATUS_BUSY,
                           "Invalid state returned by mpNetworkManager->GetServerInterface()->GetStatus() in SuspensionManager::Update", CGS_ARTIST_FILE, 144);
            }
            break;

        case E_SUBSTATE_COUNT:
        default:
            break;
        }
    }
}
