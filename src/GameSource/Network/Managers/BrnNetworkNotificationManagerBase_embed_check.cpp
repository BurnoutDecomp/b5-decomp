// Compile/link exercise for BrnNetwork::NetworkNotificationManagerBase::Update @ 0x825487C8.
// The base declares a pure-virtual ProcessNotifications(), so we drive Update() through a
// minimal concrete subclass to prove the virtual forward dispatches correctly.
#include "GameSource/Network/Managers/BrnNetworkNotificationManagerBase.h"

namespace
{
    using BrnNetwork::NetworkNotificationManagerBase;

    class TestNotificationManager : public NetworkNotificationManagerBase
    {
    public:
        int miProcessed;
        TestNotificationManager() : miProcessed(0) {}
    private:
        // vtable idx 2 -- this is what Update() must forward to.
        void ProcessNotifications() /*override*/ { ++miProcessed; }
        void Connect() /*override*/ {}
        void Disconnect() /*override*/ {}
    };

    int ExerciseUpdate()
    {
        TestNotificationManager lManager;
        lManager.Update();          // -> ProcessNotifications() -> miProcessed == 1
        lManager.Update();          // -> miProcessed == 2
        return lManager.miProcessed; // expected 2
    }

    // Keep the exerciser referenced so the TU pulls in Update()'s definition.
    volatile int gNotificationManagerProbe = ExerciseUpdate();
}
