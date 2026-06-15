#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnDirector
{
struct Moment
{
    struct Parameters
    {
    };
};

struct MomentHardStop
{
    struct Parameters : Moment::Parameters
    {
        f32 mfDistance;
        f32 mfTime;
    };
};

struct MomentBystanderSeesAction
{
    struct Parameters : Moment::Parameters
    {
        bool mbClose;
        bool mbTakedown;
        bool mbCrash;
    };
};

struct MomentTumbling
{
    struct Parameters : Moment::Parameters
    {
        s32  meCameraType;
        bool mbTakedown;
        bool mbCrash;
        u8   mPad6[2];
    };
};

class MomentParameterBank
{
public:
    enum EMomentParamID
    {
        E_PARAM_NONE = 0,
        E_PARAM_HARD_STOP_DEFAULT = 1,
        E_PARAM_BYSTANDER_CLOSE_TAKEDOWN_ONLY = 2,
        E_PARAM_BYSTANDER_FAR_CRASH_ONLY = 3,
        E_PARAM_TUMBLING_TRUCKING_SIDE_CRASH_ONLY = 4,
        E_PARAM_TUMBLING_TRUCKING_SIDE_TAKEDOWN_ONLY = 5,
        E_PARAM_TUMBLING_TRUCKING_FRONT_CRASH_ONLY = 6,
        E_PARAM_TUMBLING_FOLLOW_CRASH_ONLY = 7,
        E_PARAM_TUMBLING_LEAD_CRASH_ONLY = 8,
        E_PARAM_TUMBLING_LEAD_TAKEDOWN_ONLY = 9,
        E_PARAM_TUMBLING_SIDE_CRASH_ONLY = 10
    };

    void Construct();
    Moment::Parameters* GetParameters(EMomentParamID leMomentParams);

private:
    MomentHardStop::Parameters             mParamsHardStopDefault;
    MomentBystanderSeesAction::Parameters mParamsBystanderCloseTakedownOnly;
    MomentBystanderSeesAction::Parameters mParamsBystanderFarCrashOnly;
    u8                                     mPadE[2];
    MomentTumbling::Parameters            mParamsTumblingTruckingSideCrashOnly;
    MomentTumbling::Parameters            mParamsTumblingTruckingSideTakedownOnly;
    MomentTumbling::Parameters            mParamsTumblingTruckingFrontCrashOnly;
    MomentTumbling::Parameters            mParamsTumblingFollowCrashOnly;
    MomentTumbling::Parameters            mParamsTumblingLeadCrashOnly;
    MomentTumbling::Parameters            mParamsTumblingLeadTakedownOnly;
    MomentTumbling::Parameters            mParamsTumblingSideCrashOnly;
};

static_assert(sizeof(MomentHardStop::Parameters) == 8, "MomentHardStop::Parameters layout drift");
static_assert(sizeof(MomentBystanderSeesAction::Parameters) == 3, "MomentBystanderSeesAction::Parameters layout drift");
static_assert(sizeof(MomentTumbling::Parameters) == 8, "MomentTumbling::Parameters layout drift");
static_assert(sizeof(MomentParameterBank) == 72, "MomentParameterBank layout drift");

void MomentParameterBank::Construct()
{
    mParamsHardStopDefault.mfDistance = 3.0f;
    mParamsHardStopDefault.mfTime = 5.0f;

    mParamsBystanderCloseTakedownOnly.mbClose = true;
    mParamsBystanderCloseTakedownOnly.mbTakedown = false;
    mParamsBystanderCloseTakedownOnly.mbCrash = true;

    mParamsBystanderFarCrashOnly.mbClose = true;
    mParamsBystanderFarCrashOnly.mbTakedown = true;
    mParamsBystanderFarCrashOnly.mbCrash = false;

    mParamsTumblingTruckingSideCrashOnly.meCameraType = 0;
    mParamsTumblingTruckingSideCrashOnly.mbTakedown = true;
    mParamsTumblingTruckingSideCrashOnly.mbCrash = false;

    mParamsTumblingTruckingSideTakedownOnly.meCameraType = 0;
    mParamsTumblingTruckingSideTakedownOnly.mbTakedown = true;
    mParamsTumblingTruckingSideTakedownOnly.mbCrash = true;

    mParamsTumblingTruckingFrontCrashOnly.meCameraType = 1;
    mParamsTumblingTruckingFrontCrashOnly.mbTakedown = true;
    mParamsTumblingTruckingFrontCrashOnly.mbCrash = false;

    mParamsTumblingFollowCrashOnly.meCameraType = 2;
    mParamsTumblingFollowCrashOnly.mbTakedown = true;
    mParamsTumblingFollowCrashOnly.mbCrash = false;

    mParamsTumblingLeadCrashOnly.meCameraType = 3;
    mParamsTumblingLeadCrashOnly.mbTakedown = true;
    mParamsTumblingLeadCrashOnly.mbCrash = false;

    mParamsTumblingLeadTakedownOnly.meCameraType = 3;
    mParamsTumblingLeadTakedownOnly.mbTakedown = false;
    mParamsTumblingLeadTakedownOnly.mbCrash = true;

    mParamsTumblingSideCrashOnly.meCameraType = 4;
    mParamsTumblingSideCrashOnly.mbTakedown = true;
    mParamsTumblingSideCrashOnly.mbCrash = false;
}

Moment::Parameters* MomentParameterBank::GetParameters(EMomentParamID leMomentParams)
{
    switch (leMomentParams)
    {
        case E_PARAM_NONE:
            return 0;
        case E_PARAM_HARD_STOP_DEFAULT:
            return &mParamsHardStopDefault;
        case E_PARAM_BYSTANDER_CLOSE_TAKEDOWN_ONLY:
            return &mParamsBystanderCloseTakedownOnly;
        case E_PARAM_BYSTANDER_FAR_CRASH_ONLY:
            return &mParamsBystanderFarCrashOnly;
        case E_PARAM_TUMBLING_TRUCKING_SIDE_CRASH_ONLY:
            return &mParamsTumblingTruckingSideCrashOnly;
        case E_PARAM_TUMBLING_TRUCKING_SIDE_TAKEDOWN_ONLY:
            return &mParamsTumblingTruckingSideTakedownOnly;
        case E_PARAM_TUMBLING_TRUCKING_FRONT_CRASH_ONLY:
            return &mParamsTumblingTruckingFrontCrashOnly;
        case E_PARAM_TUMBLING_FOLLOW_CRASH_ONLY:
            return &mParamsTumblingFollowCrashOnly;
        case E_PARAM_TUMBLING_LEAD_CRASH_ONLY:
            return &mParamsTumblingLeadCrashOnly;
        case E_PARAM_TUMBLING_LEAD_TAKEDOWN_ONLY:
            return &mParamsTumblingLeadTakedownOnly;
        case E_PARAM_TUMBLING_SIDE_CRASH_ONLY:
            return &mParamsTumblingSideCrashOnly;
        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "unhandled parameter type",
                "..\\..\\..\\GameSource\\Director/MomentController/BrnMomentParameterBank.cpp",
                195);
            CgsDev::Assert::EndAssert();
            return 0;
    }
}
}
