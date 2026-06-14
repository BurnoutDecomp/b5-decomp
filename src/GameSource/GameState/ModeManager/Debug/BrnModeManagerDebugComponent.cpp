#include "types.hpp"

namespace CgsDev
{
class DebugComponent
{
public:
    static int RegisterVariable(void* pComponent, void* pVariable, const char* lpcGroup, const char* lpcName);
};
}

extern int sub_8282D800(void* pComponent, void* pVariable, const char* lpcName);
extern int sub_8282D560(void* pComponent, void* pVariable, const char* lpcGroup, const char* lpcName);
extern int sub_8282D720(void* pComponent, void* pVariable, const char* lpcName);
extern int sub_8282F598(void* pComponent, void* pVariable, int liMin, int liMax);

namespace BrnGameState
{
class ModeManagerDebugComponent;
}

extern int sub_8282F720(
    void* pComponent,
    int (BrnGameState::ModeManagerDebugComponent::*pCallback)(),
    void* pContext,
    const char* lpcName);

namespace BrnGameState
{
extern int giMarkedManMinOpponentCount;
extern int giMarkedManMaxOpponentCount;
extern float gfMarkedManMinRampTime;
extern float gfMarkedManMaxRampTime;

class ModeManagerDebugComponent
{
public:
    int FinshMode();
    const char* GetName();
    int OnActivate();

private:
    struct ModeManager
    {
        u8   mPad0[13244];
        bool mbEndlessStuntRun;
        u8   mPad13245[24890];
        bool mbFinishCurrentEvent;
        u8   mPad38136;
        bool mbWinIfSecond;
        u8   mPad38138[30];
        int  miFinishPosition;
    };

    u8    mPad0[12];
    ModeManager* mpModeManager;
    bool  mbShowModeInfo;
    bool  mbInfiniteLives;
    bool  mbPad18;
    u8    mPad19;
    int   miFinishPosition;
};

int ModeManagerDebugComponent::FinshMode()
{
    mpModeManager->mbFinishCurrentEvent = true;
    mpModeManager->miFinishPosition = miFinishPosition;
    return static_cast<int>(reinterpret_cast<intptr_t>(this));
}

const char* ModeManagerDebugComponent::GetName()
{
    return "Mode Manager";
}

int ModeManagerDebugComponent::OnActivate()
{
    sub_8282D800(this, &mpModeManager->mbEndlessStuntRun, "Endless Stunt Run");
    sub_8282D800(this, &mbInfiniteLives, "Infinite lives");
    sub_8282D800(this, &mbShowModeInfo, "Show mode info");
    sub_8282D800(this, &mpModeManager->mbWinIfSecond, "Win if second");
    sub_8282D560(this, &giMarkedManMinOpponentCount, "Marked man tweaks", "Min opponent count");
    sub_8282D560(this, &giMarkedManMaxOpponentCount, "Marked man tweaks", "Max opponent count");
    CgsDev::DebugComponent::RegisterVariable(this, &gfMarkedManMinRampTime, "Marked man tweaks", "Min ramp time");
    CgsDev::DebugComponent::RegisterVariable(this, &gfMarkedManMaxRampTime, "Marked man tweaks", "Max ramp time");
    sub_8282D720(this, &miFinishPosition, "Finish Position");
    sub_8282F598(this, &miFinishPosition, 1, 8);
    return sub_8282F720(this, &ModeManagerDebugComponent::FinshMode, this, "End Current Event");
}
}
