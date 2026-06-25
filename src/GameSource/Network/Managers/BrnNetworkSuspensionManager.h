#pragma once

#include "types.hpp"

namespace BrnNetwork
{
    class BrnNetworkManager;

    class SuspensionManager
    {
    public:
        enum ESuspensionType
        {
            E_SUSPENSION_TYPE_OFFLINE = 0,
            E_SUSPENSION_TYPE_IN_GAME,
            E_SUSPENSION_TYPE_COUNT
        };

        typedef void (*Callback)(bool lbSuccess, void* lpUserData);

        void Construct(BrnNetworkManager* lpNetworkManager);
        bool Prepare();
        bool Release();
        void Destruct();
        void Update();
        void Suspend(ESuspensionType leSuspensionType, Callback lpfCallback, void* lpUserData);
        void Resume(Callback lpfCallback, void* lpUserData);
        void Disconnected();
        bool IsSuspending() const;

    private:
        enum ESubState
        {
            E_SUBSTATE_SUSPENDING = 0,
            E_SUBSTATE_WAIT_TO_RESUME,
            E_SUBSTATE_RESUMING,
            E_SUBSTATE_COUNT
        };

        void Finished(bool lbSuccess)
        {
            Callback lpfCallback = mCallback;
            void* lpUserData = mpUserData;

            mCallback = nullptr;
            mpUserData = nullptr;
            meSubState = E_SUBSTATE_COUNT;

            lpfCallback(lbSuccess, lpUserData);
        }

        Callback mCallback;
        void* mpUserData;
        BrnNetworkManager* mpNetworkManager;
        ESuspensionType meSuspensionType;
        ESubState meSubState;
    };
}
