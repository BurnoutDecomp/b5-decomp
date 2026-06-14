#include "types.hpp"

namespace BrnGameState
{
class GameMode
{
public:
    int Construct();
    int SetCurrentState(int liState);
};

class ModeManager
{
public:
    static int TellGuiToShowOnlineFinalStandings(void* pModeManager);
};

class OnlineGameMode : public GameMode
{
public:
    int Construct();
    int SendEvent(int liEvent);

private:
    u8    mPad0[40];
    int   miCurrentState;
    u8    mPad44[116];
    void* mpModeManager;
    u8    mPad164[8];
    bool  mbConstructed;
    u8    mPad173[2];
    bool  mbFinalStandingsShown;
};

int OnlineGameMode::Construct()
{
    int liResult = GameMode::Construct();
    mbConstructed = true;
    return liResult;
}

int OnlineGameMode::SendEvent(int liEvent)
{
    if (liEvent == 2)
        return SetCurrentState(5);
    if (liEvent == 0)
        return SetCurrentState(6);

    switch (miCurrentState)
    {
        case 0:
            if (liEvent == 1)
                return SetCurrentState(2);
            break;
        case 1:
            if (liEvent == 1)
                return SetCurrentState(7);
            if (liEvent == 3)
                return SetCurrentState(0);
            break;
        case 2:
            if (liEvent == 1)
            {
                int liResult = SetCurrentState(3);
                mbFinalStandingsShown = true;
                return liResult;
            }
            break;
        case 3:
            if (liEvent == 1)
            {
                SetCurrentState(4);
                return ModeManager::TellGuiToShowOnlineFinalStandings(mpModeManager);
            }
            break;
        case 4:
            if (liEvent == 3)
                return SetCurrentState(5);
            break;
        case 6:
            if (liEvent == 1)
                return SetCurrentState(liEvent);
            break;
        case 7:
            if (liEvent == 1)
                return SetCurrentState(0);
            break;
        default:
            break;
    }

    return reinterpret_cast<int>(this);
}
}
