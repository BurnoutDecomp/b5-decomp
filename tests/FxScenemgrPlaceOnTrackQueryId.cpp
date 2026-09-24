// FX-SCENEMGR (crash parity 2026-09-24, item 3a): BrnWorld::PlaceOnTrackManager::PrePhysicsUpdate (ARTIST
// 0x822F6DF8) decodes the fine-line-test result's query id BY VALUE:
//   0x822F6F6C cmpwi r3, 1                  the event type (E_OUT_EVENT_LINE_TEST_FINE_RESULT)
//   0x822F6F74 lwz r11, 0(r22)              the SceneQueryId word
//   0x822F6F78 extrwi r10, r11, 8, 8        owner = (id >> 16) & 0xFF
//   0x822F6F7C cmplwi r10, 5                KI_PLACE_ON_TRACK_LINE_TEST_OWNER
//   0x822F6F84 clrlwi r20, r11, 16          slot  = id & 0xFFFF
// and its producer PostSceneUpdate (0x822D3168) builds the id as `clrlwi r8, slot, 16 ; oris r8, r8, 5`.
// The body and CgsSceneQueryId.h's struct are extracted VERBATIM by run_fxscenemgr_place_on_track_query_id.py;
// the pre-fix body read the big-endian memory bytes [1] / [0] of the id, which on this host are bits
// [8..15] / [0..7].
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace CgsModule { struct Event {}; }

namespace CgsSceneManager {
#include "fxsm_qid_struct.inc"   // struct SceneQueryId { ... }; (from the revision under test)
}

#include "fxsm_qid_form.inc"     // FXSM_QID_ACCESSORS (does that SceneQueryId have Set/GetOwner/GetIndex?)

namespace BrnWorld {
struct PlaceOnTrackCandidate { Vector4 mPosition; Vector4 mNormal; u8 maReserved20[16]; u16 muFlags; u8 maReserved32[14]; };
struct PlaceOnTrackCandidateList
{
    CgsSceneManager::SceneQueryId mQueryId;   // +0x00
    s32                   muNumCandidates;    // +0x04
    u8                    maReserved08[8];
    PlaceOnTrackCandidate maCandidates[1];    // +0x10
};
#include "fxsm_qid_consts.inc"   // KI_OUT_EVENT_LINE_TEST_FINE_RESULT / KI_PLACE_ON_TRACK_LINE_TEST_OWNER (from the sources)

namespace RaceCarEntityModuleIO {
struct SceneResultQueue
{
    struct Record { s32 miType; const CgsModule::Event* mpEvent; };
    std::vector<Record> maRecords;
    s32 GetFirstEvent(const CgsModule::Event** lppEvent, s32* lpiSize) const
    {
        *lpiSize = 0;
        if (maRecords.empty()) { *lppEvent = nullptr; return 0; }
        *lppEvent = maRecords[0].mpEvent;
        return maRecords[0].miType;
    }
    s32 GetNextEvent(const CgsModule::Event* lpCurrent, const CgsModule::Event** lppEvent, s32* lpiSize) const
    {
        *lpiSize = 0;
        for (size_t i = 0; i + 1 < maRecords.size(); ++i)
            if (maRecords[i].mpEvent == lpCurrent) { *lppEvent = maRecords[i + 1].mpEvent; return maRecords[i + 1].miType; }
        *lppEvent = nullptr;
        return 0;
    }
};
struct InputBuffer_PrePhysics { SceneResultQueue mQueue; const SceneResultQueue* GetSceneResultQueue() const { return &mQueue; } };
struct OutputBuffer_PrePhysics {};
}

class ActiveRaceCar
{
public:
    Vector3 mPlaceOnTrackPosition = {};
    const Vector3& GetPlaceOnTrackPosition() const { return mPlaceOnTrackPosition; }
};

class RaceCarEntityModule
{
public:
    ActiveRaceCar maCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    bool mbInCarSelectScreen = false;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) { return &maCars[leIndex]; }
    bool IsInCarSelectScreen() const { return mbInCarSelectScreen; }
};

struct Placed { s32 miSlot; const PlaceOnTrackCandidate* mpBest; };
struct Ranked { const PlaceOnTrackCandidateList* mpList; Vector4 mQuery; bool mbIgnoreFatal; };

class PlaceOnTrackManager
{
public:
    void PrePhysicsUpdate(const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
                          RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);

    const PlaceOnTrackCandidate* ComputeBestPlaceOnT(const PlaceOnTrackCandidateList& lrList, const Vector4& lrQuery,
                                                     bool lbIgnoreFatal) const
    {
        maRanked.push_back(Ranked{ &lrList, lrQuery, lbIgnoreFatal });
        return lrList.muNumCandidates > 0 ? &lrList.maCandidates[0] : nullptr;
    }
    void PlaceCarOnTrack(EActiveRaceCarIndex leIndex, const PlaceOnTrackCandidate* lpBest,
                         RaceCarEntityModuleIO::OutputBuffer_PrePhysics*)
    {
        maPlaced.push_back(Placed{ static_cast<s32>(leIndex), lpBest });
    }
    void ApplyPendingRequestsWithoutSceneQueryBringUp(RaceCarEntityModuleIO::OutputBuffer_PrePhysics*) { ++miBringUps; }
    void ArmCarTeleportBringUp() {}
    void ArmCrashSweepBringUp() {}

    RaceCarEntityModule* mpRaceCarEntityModule = nullptr;
    mutable std::vector<Ranked> maRanked;
    std::vector<Placed> maPlaced;
    int miBringUps = 0;
};
#include "fxsm_qid_prephysics.inc"
}   // namespace BrnWorld

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

// One result record, 16-aligned, with a raw 32-bit query id word (the producer's `stw`).
struct alignas(16) ResultStorage { BrnWorld::PlaceOnTrackCandidateList mList; BrnWorld::PlaceOnTrackCandidate mExtra[3]; };
static void MakeResult(ResultStorage& lrStorage, u32 luQueryIdWord, s32 liCount)
{
    std::memset(&lrStorage, 0, sizeof(lrStorage));
    std::memcpy(&lrStorage.mList.mQueryId, &luQueryIdWord, 4);
    lrStorage.mList.muNumCandidates = liCount;
}

int main()
{
    using namespace BrnWorld;

#if FXSM_QID_ACCESSORS
    {
        CgsSceneManager::SceneQueryId lId;
        lId.Set(5, 3);
        Check(lId.mId == 0x00050003u, "Set(5, 3) == `clrlwi 16 ; oris 5` == 0x00050003 (PostSceneUpdate 0x822D3254/64)");
        Check(lId.GetOwner() == 5 && lId.GetIndex() == 3, "GetOwner / GetIndex read bits [16..23] / [0..15]");
        lId.Set(0xAB, 0xCDEF);
        Check(lId.mId == 0x00ABCDEFu && lId.GetOwner() == 0xAB && lId.GetIndex() == 0xCDEF,
              "a full owner byte and a full index round-trip");
        lId.mId = 0x7F050009u;
        Check(lId.GetOwner() == 5 && lId.GetIndex() == 9, "bits [24..31] are ignored by both getters (extrwi 8,8 / clrlwi 16)");
    }
#else
    Check(false, "SceneQueryId has the DWARF Set / GetOwner / GetIndex (CgsSceneQueryId.h:54/:57/:60)");
    Check(false, "(accessor semantics not checkable without them)");
    Check(false, "(accessor semantics not checkable without them)");
    Check(false, "(accessor semantics not checkable without them)");
#endif

    RaceCarEntityModule lModule;
    for (int i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i)
        lModule.maCars[i].mPlaceOnTrackPosition = Vector3{ 100.0f + i, 2.0f * i, -50.0f - i, 7.0f };
    PlaceOnTrackManager lManager;
    lManager.mpRaceCarEntityModule = &lModule;

    static ResultStorage sSlot3, sDecoyBits8, sWrongType, sOtherOwner, sSlot6;
    MakeResult(sSlot3,      0x00050003u, 2);   // Set(5, 3): slot 3's answer
    MakeResult(sDecoyBits8, 0x00000503u, 1);   // 5 in bits [8..15], owner 0: not a place-on-track answer
    MakeResult(sWrongType,  0x00050001u, 1);   // owner 5 but type 2
    MakeResult(sOtherOwner, 0x00030006u, 1);   // owner 3
    MakeResult(sSlot6,      0x00050006u, 0);   // Set(5, 6) with no intersection

    RaceCarEntityModuleIO::InputBuffer_PrePhysics lInput;
    lInput.mQueue.maRecords = {
        { 1, reinterpret_cast<const CgsModule::Event*>(&sSlot3) },
        { 1, reinterpret_cast<const CgsModule::Event*>(&sDecoyBits8) },
        { 2, reinterpret_cast<const CgsModule::Event*>(&sWrongType) },
        { 1, reinterpret_cast<const CgsModule::Event*>(&sOtherOwner) },
        { 1, reinterpret_cast<const CgsModule::Event*>(&sSlot6) },
    };
    RaceCarEntityModuleIO::OutputBuffer_PrePhysics lOutput;
    lModule.mbInCarSelectScreen = true;
    lManager.PrePhysicsUpdate(&lInput, &lOutput);

    std::printf("placed %d car(s):", static_cast<int>(lManager.maPlaced.size()));
    for (const Placed& p : lManager.maPlaced) std::printf(" slot %d", p.miSlot);
    std::printf("\n");

    Check(lManager.maPlaced.size() == 2, "exactly the two owner-5, type-1 results place a car");
    Check(lManager.maPlaced.size() == 2 && lManager.maPlaced[0].miSlot == 3 && lManager.maPlaced[1].miSlot == 6,
          "the slot is the id's low half (clrlwi 16): slot 3 then slot 6, queue order");
    Check(lManager.maRanked.size() == 2 && lManager.maRanked[0].mpList == &sSlot3.mList
          && lManager.maRanked[1].mpList == &sSlot6.mList,
          "each answer is ranked from its OWN result record");
    Check(lManager.maPlaced.size() == 2 && lManager.maPlaced[0].mpBest == &sSlot3.mList.maCandidates[0]
          && lManager.maPlaced[1].mpBest == nullptr,
          "the ranked pick reaches PlaceCarOnTrack (NULL for the empty answer)");
    bool lbNoDecoy = true;
    for (const Ranked& r : lManager.maRanked)
        lbNoDecoy = lbNoDecoy && r.mpList != &sDecoyBits8.mList && r.mpList != &sOtherOwner.mList && r.mpList != &sWrongType.mList;
    Check(lbNoDecoy, "a 5 in bits [8..15], another owner, or another event type is never taken for an answer");
    Check(lManager.maRanked.size() == 2 && !lManager.maRanked[0].mbIgnoreFatal,
          "lbIgnoreFatal = !IsInCarSelectScreen() (lbzx +0x186C9 ; cntlzw)");
    Check(lManager.maRanked.size() == 2 && lManager.maRanked[0].mQuery.x == 103.0f && lManager.maRanked[0].mQuery.y == 6.0f
          && lManager.maRanked[0].mQuery.z == -53.0f && lManager.maRanked[0].mQuery.w == 0.0f,
          "the query point is the asking car's GetPlaceOnTrackPosition, w = 0");
    Check(lManager.miBringUps == 1, "the frame's tail still runs once");

    std::printf("FxScenemgrPlaceOnTrackQueryId: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
