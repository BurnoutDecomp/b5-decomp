#include <cstdio>
#include "BrnCommonTypes.h"
#include "RaceCarResetHistoryFixture.h"
static int giChecks = 0, giFailures = 0;
struct RaceCarEntityModule
{
    RaceCarResetHistoryFixture mRCEntityModuleDebugComponent;
    void RecordResetTransform(const Matrix44Affine& lrTransform);
};
#include "race_car_reset_history.inc"
static void Check(bool lbValue, const char* lpcName)
{
    ++giChecks;
    if (!lbValue) { ++giFailures; std::printf("FAIL %s\n",lpcName); }
}
int main()
{
    RaceCarEntityModule lModule;
    Matrix44Affine lTransform;
    lTransform.xAxis = {1,2,3,4}; lTransform.yAxis = {5,6,7,8};
    lTransform.zAxis = {9,10,11,12}; lTransform.wAxis = {13,14,15,16};
    lModule.RecordResetTransform(lTransform);
    const auto& lrHistory = lModule.mRCEntityModuleDebugComponent.mLastResetTransform;
    Check(lrHistory.xAxis.x==1 && lrHistory.xAxis.w==4 && lrHistory.yAxis.y==6
          && lrHistory.yAxis.w==8 && lrHistory.zAxis.z==11 && lrHistory.zAxis.w==12
          && lrHistory.wAxis.x==13 && lrHistory.wAxis.w==16,
          "reset captures all four transform rows before checking active/attached state");
    lTransform.wAxis.x = 99;
    Check(lrHistory.wAxis.x==13, "history owns a copy rather than aliasing the caller's transform");
    std::printf("RaceCarResetHistory: %d checks, %d failures\n",giChecks,giFailures);
    return giFailures ? 1 : 0;
}
