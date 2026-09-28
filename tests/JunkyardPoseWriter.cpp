// L4 boot order (conductor 2026-09-28, follow-up (2)) -- the profile's saved car pose follows the junkyard the player
// was last in, because GameStateModule::PreWorldUpdate hands ProgressionManager::PreWorldUpdate the console's
// lbIsInJunkyard.
//
// PreWorldUpdate @0x823A5328, 0x823A5B48..0x823A5B80:
//     lwz    r11, var_6EC(r1)      var_6EC == this+0x2CDC0 (stored @0x823A5664) == mCarSelectManager.mJunkyardId
//     ld     r11, 0(r11)
//     cmpldi cr6, r11, 0 ; li r11, 1 ; bne -> 1
//     lbzx   r11, r31, 0x2CE34    mOnlineCarSelectManager (+0x2CE20) +0x14 == mbIsInOnlineCarSelect
//     ...    clrlwi r9, r11, 24   lbIsInJunkyard
// The PC passed a constant false, so the callee's player-car arm never saved the car pose into the profile
// (ProgressionManager::PreWorldUpdate @0x823A4F68: `lvx128 v0, rcs, 0x220 ; stvx128 v0, pm, 0x1A0` when
// lbIsInJunkyard and no online mode) -- and a save kept whatever pose it was created with.
//
// run_junkyard_start_pose.py extracts the PRODUCTION argument expression (the initializer of the variable the call
// passes, or the literal it passes) into junkyard_isinjunkyard.inc; this fixture evaluates it over the console's
// truth table, then replays the callee's pose arm (the two stores and the distance branch, transcribed from the
// console as the fixture's model of the callee) across a boot: junkyard -> exit -> drive.
#include <cstdint>
#include <cstdio>

typedef int32_t  s32;
typedef uint64_t u64;
typedef float    f32;
typedef u64      CgsID;

struct CarSelectManagerStandIn
{
    CgsID mJunkyardId = 0;
    CgsID GetJunkyardId() const { return mJunkyardId; }
};
struct OnlineCarSelectManagerStandIn
{
    bool mbIsInOnlineCarSelect = false;
    bool IsInOnlineCarSelect() const { return mbIsInOnlineCarSelect; }
};

static bool EvalIsInJunkyard(const CarSelectManagerStandIn& mCarSelectManager,
                             const OnlineCarSelectManagerStandIn& mOnlineCarSelectManager)
{
    (void)mCarSelectManager; (void)mOnlineCarSelectManager;
#include "junkyard_isinjunkyard.inc"       // `return <the production expression>;`
}

static s32 giChecks = 0, giFailures = 0;
static void Check(bool lbOk, const char* lpcName)
{
    ++giChecks;
    if (!lbOk) ++giFailures;
    std::printf("%s  %s\n", lbOk ? "PASS" : "FAIL", lpcName);
}

// The callee's player-car arm as the console runs it (BrnProgressionManager_PreWorldUpdate.cpp): in the junkyard
// (and not online) the pose is saved, otherwise the distance is integrated.
struct PoseModel
{
    f32 mafSavedPos[3] = {0.0f, 0.0f, 0.0f};
    f32 mfDistance = 0.0f;
    void Step(bool lbIsInJunkyard, const f32 lafCarPos[3], f32 lfMetres)
    {
        if (lbIsInJunkyard)
        {
            mafSavedPos[0] = lafCarPos[0]; mafSavedPos[1] = lafCarPos[1]; mafSavedPos[2] = lafCarPos[2];
        }
        else
        {
            mfDistance += lfMetres;
        }
    }
};

int main()
{
    CarSelectManagerStandIn lCarSelect;
    OnlineCarSelectManagerStandIn lOnline;

    Check(!EvalIsInJunkyard(lCarSelect, lOnline), "no junkyard id, no online car select -> false");
    lCarSelect.mJunkyardId = 312262;
    Check(EvalIsInJunkyard(lCarSelect, lOnline),
          "a junkyard id (mCarSelectManager.mJunkyardId, `ld r11, 0(this+0x2CDC0)`) -> true");
    lCarSelect.mJunkyardId = 0;
    lOnline.mbIsInOnlineCarSelect = true;
    Check(EvalIsInJunkyard(lCarSelect, lOnline),
          "the online car select (`lbzx r11, r31, 0x2CE34`) -> true");
    lCarSelect.mJunkyardId = 0x0000000100000000ull;    // only the HIGH word set: the console tests the whole u64
    lOnline.mbIsInOnlineCarSelect = false;
    Check(EvalIsInJunkyard(lCarSelect, lOnline), "the id is tested as a whole 64-bit word (`ld` / `cmpldi`)");

    // A boot on a save whose pose is (0,0,0): in junkyard 312262 the car sits at its spawn, the player drives out.
    PoseModel lModel;
    const f32 lafJunkyardCar[3] = {-381.583313f, 12.9f, 915.523804f};
    const f32 lafOutside[3] = {-250.0f, 8.0f, 930.0f};
    lCarSelect.mJunkyardId = 312262;
    for (s32 li = 0; li < 30; ++li)
        lModel.Step(EvalIsInJunkyard(lCarSelect, lOnline), lafJunkyardCar, 0.0f);
    lCarSelect.mJunkyardId = 0;                          // the junkyard exit clears the id
    for (s32 li = 0; li < 30; ++li)
        lModel.Step(EvalIsInJunkyard(lCarSelect, lOnline), lafOutside, 1.0f);
    Check(lModel.mafSavedPos[0] == lafJunkyardCar[0] && lModel.mafSavedPos[2] == lafJunkyardCar[2],
          "the saved pose is the car's pose in the junkyard the player was last in (the next boot enters it)");
    Check(lModel.mfDistance == 30.0f, "outside the junkyard the arm integrates distance and leaves the pose");

    std::printf("JunkyardPoseWriter: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
