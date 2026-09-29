#include <cstdio>
#include <cmath>
#include "GameSource/World/AI/BrnAIHarnessCombatPC.h"
int main() {
    int checks=0, failures=0;
    auto check = [&](bool ok,const char* label) { ++checks; if(!ok){++failures;std::printf("FAIL %s\n",label);} };
    using BrnAI::AimHarnessCombatPC;
    const auto right = AimHarnessCombatPC(3,8,0,1,0,35,40);
    const auto left = AimHarnessCombatPC(-3,8,0,1,0,35,40);
    check(right.mbCommit && right.mfSteering < 0,"positive-X target uses the engine's negative XZ signed angle");
    check(left.mbCommit && left.mfSteering == -right.mfSteering,"mirror left/right steering");
    const auto rotated = AimHarnessCombatPC(8,-3,1,0,35,0,40);
    check(rotated.mbCommit && std::fabs(rotated.mfSteering-right.mfSteering)<1e-6f,"world heading does not change the local intercept");
    check(!AimHarnessCombatPC(0,-10,0,1,0,35,40).mbCommit,"do not turn back after a passed rival");
    check(!AimHarnessCombatPC(0,60,0,1,0,35,40).mbCommit,"retain road navigation for distant targets");
    check(!AimHarnessCombatPC(20,8,0,1,0,35,40).mbCommit,"do not cut across distant lanes");
    check(!AimHarnessCombatPC(3,8,0,1,0,0,0).mbCommit,"retain AI recovery when stopped");
    const auto side = AimHarnessCombatPC(10,0,0,1,0,30,50);
    check(side.mbCommit && std::fabs(side.mfSteering) <= 0.55f,"bound steering at high speed while attacking alongside");
    std::printf("PCHarnessCombat: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
