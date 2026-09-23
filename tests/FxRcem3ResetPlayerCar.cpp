// FX-RCEM3 (crash-parity 2026-09-23): the production RaceCarEntityModule::HandleResetPlayerCarAction
// (ARTIST 0x82304FE8, game action 0), extracted VERBATIM from BrnRaceCarEntityModule.cpp by
// run_fxrcem3_reset_player_car.py and run against a fixture module.
//   G68-D1  re-spawn arm: the player's slot is read ONCE before the old car is removed
//           (`lwz r27, 0(r17)` @0x823051CC, r17 = this + 0x182F8) and the new car is attached to
//           THAT slot (`mr r5, r27` @0x82305364 -> AttachActiveRaceCar @0x82305370).
//   G68-D2  teleport arm (car id 0): ActiveRaceCar::RequestPlaceOnTrack(v1 = transform +0x30,
//           v2 = transform +0x20, f1 = flt_82001CC0 = 0.0f) @0x82305534..0x82305544.
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace Fixture {


struct Vector3 { f32 x, y, z, w; };
struct Matrix44Affine { Vector3 xAxis, yAxis, zAxis, wAxis; };
typedef u64 CgsID;
const CgsID KU_CGSID_NULL = 0;

namespace BrnMath {
inline bool IsNormal(const Vector3&) { return true; }
inline void BuildTransform(Matrix44Affine& lrOut, const Vector3& lPos, const Vector3& lDir, const Vector3& lUp) {
    lrOut.xAxis = { 1.0f, 0.0f, 0.0f, 0.0f }; lrOut.yAxis = lUp; lrOut.zAxis = lDir; lrOut.wAxis = lPos;
}
}
namespace BrnPhysics { namespace Deformation { enum DeformationResetType : s32 { E_DEFORMATION_RESET_NONE = 0 }; } }
namespace BrnGameState { namespace GameStateModuleIO {
enum EPlayerScoringIndex : s32 { E_PLAYER_SCORING_INDEX_0 = 0, E_PLAYER_SCORING_INDEX_COUNT = 8 };
struct ResetPlayerCarAction {
    Vector3 mPosition; Vector3 mDirection;
    CgsID mCarModelId; CgsID mWheelModelId;
    EPlayerScoringIndex mePlayerScoringIndex;
    f32 mfDeformationAmount; s32 miBaseDeformationType;
    enum CarSelectType { E_CAR_SELECT_DONT_DROP = 0 };
    CarSelectType meCarSelectType;
    bool mbInCarSelectScreen, mbCarSelectDontStreamAudio, mbKeepResetSection;
    bool mbChangeLocation;
    bool HasToChangeLocation() const { return mbChangeLocation; }
};
} }
namespace BrnResource {
struct VehicleListEntry { const char* GetDefaultWheelName() const { return "wheel"; } };
struct WheelListEntry { CgsID mID; };
struct VehicleList { const VehicleListEntry mEntry = {}; const VehicleListEntry* GetVehicleData(CgsID) const { return &mEntry; } };
struct WheelList { WheelListEntry mEntry = { 77 }; s32 FindWheelIndexFromName(const char*) const { return 0; } const WheelListEntry* GetWheelData(s32) const { return &mEntry; } };
}
namespace BrnAI { namespace AIModuleIO { struct RaceCarAIInterface {}; } }
enum ERaceCarTypeFixture : s32 { E_RACE_CAR_TYPE_PLAYER = 1 };

struct ActiveRaceCar;
struct RaceCar {
    EGlobalRaceCarIndex meGlobal = E_GLOBAL_RACE_CAR_INDEX_INVALID;
    ActiveRaceCar* mpActive = nullptr;
    CgsID muModel = 0; s32 miPalette = 0, miColour = 0;
    bool IsInCurrentGameMode() const { return false; }
    CgsID GetModelId() const { return muModel; }
    s32 GetColourPalette() const { return miPalette; }
    s32 GetColourIndex() const { return miColour; }
    EGlobalRaceCarIndex GetGlobalRaceCarIndex() const { return meGlobal; }
    void SetColourIndex(s32 li) { miColour = li; }
    void SetColourPalette(s32 li) { miPalette = li; }
    void SetInCurrentGameMode(bool, bool) {}
    ActiveRaceCar* GetActiveRaceCar() const { return mpActive; }
};
struct ActiveRaceCar {
    EActiveRaceCarIndex meIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    RaceCar* mpRaceCar = nullptr;
    Matrix44Affine mTransform = { { 1,0,0,0 }, { 0,1,0,0 }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 5.0f, 6.0f, 7.0f, 1.0f } };
    int miPlaceRequests = 0; Vector3 mPlacePosition = {}, mPlaceDirection = {}; f32 mfPlaceSpeed = -99.0f;
    RaceCar* GetGlobalRaceCar() const { return mpRaceCar; }
    Matrix44Affine GetTransform() const { return mTransform; }
    EActiveRaceCarIndex GetActiveRaceCarIndex() const { return meIndex; }
    void RequestPlaceOnTrack(const Vector3& lPosition, const Vector3& lDirection, f32 lfSpeed) {
        ++miPlaceRequests; mPlacePosition = lPosition; mPlaceDirection = lDirection; mfPlaceSpeed = lfSpeed;
    }
    void SetBaseDeformation(f32, BrnPhysics::Deformation::DeformationResetType) {}
};
namespace RaceCarEntityModuleIO {
struct OutputBuffer_PreScene {
    BrnAI::AIModuleIO::RaceCarAIInterface mAI;
    BrnAI::AIModuleIO::RaceCarAIInterface* GetRaceCarAIInterface() { return &mAI; }
};
}

struct RaceCarEntityModule {
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    RaceCar       maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    const BrnResource::VehicleList* mpVehicleList = nullptr;
    const BrnResource::WheelList*   mpWheelList = nullptr;
    bool mbInCarSelectScreen = false, mbCarSelectDontStreamAudio = false, mbIsInOnlineGameMode = false;
    bool mbWaitingForStreaming = false;
    s32  meCarSelectResetType = 0;
    f32  mfPlayerBaseDeformAmountMirror = 0.0f; s32 miPlayerBaseDeformationTypeMirror = 0;
    int  miNextGlobal = 5;
    EActiveRaceCarIndex meAttachRequest = static_cast<EActiveRaceCarIndex>(-7);
    int  miRemoved = 0;

    RaceCarEntityModule() {
        for (int i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i) maActiveRaceCars[i].meIndex = static_cast<EActiveRaceCarIndex>(i);
        for (int i = 0; i < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++i) maRaceCars[i].meGlobal = static_cast<EGlobalRaceCarIndex>(i);
    }
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex le) {
        CGS_ASSERT(le >= 0 && le < E_ACTIVE_RACE_CAR_INDEX_COUNT, "active index");
        return &maActiveRaceCars[le];
    }
    RaceCar* GetGlobalRaceCar(EGlobalRaceCarIndex le) { return &maRaceCars[le]; }
    // Console RemoveRaceCar (0x82304440): detaching the player resets the module's player index.
    void RemoveRaceCar(EGlobalRaceCarIndex le, RaceCarEntityModuleIO::OutputBuffer_PreScene*) {
        ++miRemoved;
        RaceCar& lrCar = maRaceCars[le];
        if (lrCar.mpActive != nullptr) {
            if (lrCar.mpActive->meIndex == mePlayerActiveRaceCarIndex) mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
            lrCar.mpActive->mpRaceCar = nullptr; lrCar.mpActive = nullptr;
        }
    }
    EGlobalRaceCarIndex SpawnRaceCar(BrnAI::AIModuleIO::RaceCarAIInterface*, const Matrix44Affine&, s32, CgsID lModel,
                                     bool, CgsID, s32, s32) {
        RaceCar& lrCar = maRaceCars[miNextGlobal];
        lrCar.muModel = lModel;
        return static_cast<EGlobalRaceCarIndex>(miNextGlobal++);
    }
    // Console AttachActiveRaceCar (0x822F4DB0): an explicit index is used as is; INVALID takes the
    // first free slot.
    EActiveRaceCarIndex AttachActiveRaceCar(RaceCar* lpCar, EActiveRaceCarIndex leSlot) {
        meAttachRequest = leSlot;
        if (leSlot == E_ACTIVE_RACE_CAR_INDEX_INVALID) {
            int i = 0; while (i < E_ACTIVE_RACE_CAR_INDEX_COUNT && maActiveRaceCars[i].mpRaceCar != nullptr) ++i;
            leSlot = static_cast<EActiveRaceCarIndex>(i);
        }
        maActiveRaceCars[leSlot].mpRaceCar = lpCar; lpCar->mpActive = &maActiveRaceCars[leSlot];
        return leSlot;
    }
    void SetActiveRaceCarForPlayerScoringIndex(BrnGameState::GameStateModuleIO::EPlayerScoringIndex, EActiveRaceCarIndex) {}
    void HandleResetPlayerCarAction(const BrnGameState::GameStateModuleIO::ResetPlayerCarAction* lpAction,
                                    RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);
};
#include "fxrcem3_reset_player_car.inc"
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    using Fixture::BrnGameState::GameStateModuleIO::ResetPlayerCarAction;
    Fixture::BrnResource::VehicleList lVehicles; Fixture::BrnResource::WheelList lWheels;

    // ---- G68-D1: a re-spawn re-attaches the new car to the player's OLD slot --------------------
    {
        Fixture::RaceCarEntityModule lModule; Fixture::RaceCarEntityModuleIO::OutputBuffer_PreScene lOut;
        lModule.mpVehicleList = &lVehicles; lModule.mpWheelList = &lWheels;
        // Player seated in slot 3, with slots 0..2 FREE (the case where "first free" differs).
        Fixture::RaceCar& lrOld = lModule.maRaceCars[2];
        lModule.AttachActiveRaceCar(&lrOld, E_ACTIVE_RACE_CAR_INDEX_3);
        lModule.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        ResetPlayerCarAction lAction; std::memset(&lAction, 0, sizeof(lAction));
        lAction.mCarModelId = 0x1234; lAction.mfDeformationAmount = -1.0f; lAction.mbChangeLocation = true;
        lAction.mePlayerScoringIndex = Fixture::BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
        lModule.HandleResetPlayerCarAction(&lAction, &lOut);
        Check(lModule.miRemoved == 1, "G68-D1 the old player car is removed (bl RemoveRaceCar @0x82305310)");
        Check(lModule.meAttachRequest == E_ACTIVE_RACE_CAR_INDEX_3,
              "G68-D1 AttachActiveRaceCar gets the OLD player slot (r27 read @0x823051CC; mr r5,r27 @0x82305364), not INVALID");
        Check(lModule.mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_3,
              "G68-D1 the new player car sits in slot 3 (not the first free slot 0)");

        // First spawn: no player yet -> INVALID (first free slot), as before.
        Fixture::RaceCarEntityModule lFirst; lFirst.mpVehicleList = &lVehicles; lFirst.mpWheelList = &lWheels;
        lFirst.HandleResetPlayerCarAction(&lAction, &lOut);
        Check(lFirst.meAttachRequest == E_ACTIVE_RACE_CAR_INDEX_INVALID && lFirst.miRemoved == 0
                  && lFirst.mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_0,
              "G68-D1 first spawn: slot INVALID -> first free slot, nothing removed");
    }

    // ---- G68-D2: a teleport-only record (car id 0) places the car ------------------------------
    {
        Fixture::RaceCarEntityModule lModule; Fixture::RaceCarEntityModuleIO::OutputBuffer_PreScene lOut;
        lModule.mpVehicleList = &lVehicles; lModule.mpWheelList = &lWheels;
        Fixture::RaceCar& lrCar = lModule.maRaceCars[0];
        lModule.AttachActiveRaceCar(&lrCar, E_ACTIVE_RACE_CAR_INDEX_1);
        lModule.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_1;
        ResetPlayerCarAction lAction; std::memset(&lAction, 0, sizeof(lAction));
        lAction.mbChangeLocation = true;
        lAction.mPosition = { 900.0f, 12.5f, -40.0f, 1.0f };
        lAction.mDirection = { 0.0f, 0.0f, -1.0f, 0.0f };
        lAction.mfDeformationAmount = -1.0f;
        lAction.mePlayerScoringIndex = Fixture::BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
        lModule.HandleResetPlayerCarAction(&lAction, &lOut);
        const Fixture::ActiveRaceCar& lrActive = lModule.maActiveRaceCars[1];
        Check(lrActive.miPlaceRequests == 1, "G68-D2 teleport: exactly one RequestPlaceOnTrack (bl @0x82305544)");
        Check(lrActive.mPlacePosition.x == 900.0f && lrActive.mPlacePosition.y == 12.5f && lrActive.mPlacePosition.z == -40.0f,
              "G68-D2 teleport: v1 = the transform's wAxis (+0x30) == the record's position");
        Check(lrActive.mPlaceDirection.z == -1.0f && lrActive.mPlaceDirection.x == 0.0f,
              "G68-D2 teleport: v2 = the transform's zAxis (+0x20) == the record's direction");
        Check(lrActive.mfPlaceSpeed == 0.0f, "G68-D2 teleport: f1 = flt_82001CC0 == 0.0f");
        Check(lModule.miRemoved == 0, "G68-D2 teleport: no re-spawn");

        // Same-place teleport (no change of location): the car's own transform.
        Fixture::RaceCarEntityModule lSame; lSame.mpVehicleList = &lVehicles; lSame.mpWheelList = &lWheels;
        lSame.AttachActiveRaceCar(&lSame.maRaceCars[0], E_ACTIVE_RACE_CAR_INDEX_0);
        lSame.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        lAction.mbChangeLocation = false;
        lSame.HandleResetPlayerCarAction(&lAction, &lOut);
        Check(lSame.maActiveRaceCars[0].miPlaceRequests == 1 && lSame.maActiveRaceCars[0].mPlacePosition.x == 5.0f
                  && lSame.maActiveRaceCars[0].mPlacePosition.z == 7.0f && lSame.maActiveRaceCars[0].mPlaceDirection.z == 1.0f,
              "G68-D2 teleport in place: GetTransform() rows +0x30 / +0x20 (lvx128 v126/v127 @0x823050D0/D4)");
    }

    Check(guAssertions == 0, "valid fixtures fire no assertions");
    std::printf("FxRcem3ResetPlayerCar: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
