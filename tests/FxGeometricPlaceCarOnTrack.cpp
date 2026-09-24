// FX-GEOMETRIC (crash parity 2026-09-24): BrnWorld::PlaceOnTrackManager::PlaceCarOnTrack -- the per-answer tail of
// PrePhysicsUpdate @0x822F6DF8 (0x822F711C..0x822F7898), extracted VERBATIM by run_fxgeometric_place_car_on_track.py
// with the file's own AreVectorsSimilar / KF_PLACE_ON_TRACK_SIMILAR_EPSILON. The car, module, output buffer and the
// BrnMath helpers are small recording fakes. From the ARTIST asm:
//   best == 0 (0x822F712C `beq` -> 0x822F7384): print "    Failed to find valid place on track location - reverting
//     to ring buffer\n" (gxMessageFilterFlags & 1, 0x8201EA18), then ActiveRaceCar::GetResetCoords (0x822F73B0) for
//     position AND direction, normal = unk_82181510 (0, 1, 0, 0) -- the PC parked the car on the REQUESTED pose
//     until 2026-09-24 instead;
//   both arms join at 0x822F7250 and print "    Selected reset data: lResetPosition=" (%f, %f, %f)
//     ", lResetNormal=" (...) ", lResetDirection=" (...) "\n" (0x8201EA68 / 0x8201EA94 / 0x8201EAA4 / 0x82001CC4);
//   BuildTransform(pos, direction, normal), ClearPlaceOnTrack, ResetActiveRaceCar(slot, transform, dir * speed).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::vector<std::string> gaAsserts;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
}
static std::string gText;
namespace Log {
struct DebugPrint
{
    DebugPrint& operator<<(const char* lpc) { gText += lpc; return *this; }
    DebugPrint& operator<<(s32 li) { gText += std::to_string(li); return *this; }
    DebugPrint& operator<<(f32 lf) { char b[64]; std::snprintf(b, sizeof(b), "%f", lf); gText += b; return *this; }
    void AppendFormat(const char* lpcFormat, ...)
    {
        char b[256]; va_list args; va_start(args, lpcFormat); std::vsnprintf(b, sizeof(b), lpcFormat, args); va_end(args);
        gText += b;
    }
};
static DebugPrint gPrint;
DebugPrint* gpDebugPrint = &gPrint;
}
namespace Message { u64 gxMessageFilterFlags = 1; }
}
namespace CgsModule { struct Event {}; }

// BrnMath / rw::math fakes: BuildTransform records its three inputs as w / z / y axes.
namespace BrnMath {
inline bool IsNormal(const Vector3& v) { return std::fabs(v.x * v.x + v.y * v.y + v.z * v.z - 1.0f) < 1e-3f; }
inline void BuildTransform(Matrix44Affine& lrOut, const Vector3& lrPosition, const Vector3& lrAt, const Vector3& lrUp)
{
    std::memset(&lrOut, 0, sizeof(lrOut));
    lrOut.wAxis.x = lrPosition.x; lrOut.wAxis.y = lrPosition.y; lrOut.wAxis.z = lrPosition.z; lrOut.wAxis.w = 1.0f;
    lrOut.zAxis.x = lrAt.x; lrOut.zAxis.y = lrAt.y; lrOut.zAxis.z = lrAt.z;
    lrOut.yAxis.x = lrUp.x; lrOut.yAxis.y = lrUp.y; lrOut.yAxis.z = lrUp.z;
}
}
namespace rw { namespace math { namespace vpu { inline bool IsValid(const Matrix44Affine&) { return true; } } } }

namespace BrnWorld {
struct PlaceOnTrackCandidate { Vector4 mPosition; Vector4 mNormal; u8 maReserved20[16]; u16 muFlags; u8 maReserved32[14]; };

class ActiveRaceCar
{
public:
    Vector3 mRequestPosition = {}, mRequestDirection = {};
    Vector3 mRingPosition = {}, mRingDirection = {};
    f32 mfSpeed = 0.0f;
    bool mbToBePlacedOnTrack = true;
    EActiveRaceCarIndex meIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    mutable int miResetCoordsCalls = 0;
    const Vector3& GetPlaceOnTrackPosition() const { return mRequestPosition; }
    const Vector3& GetPlaceOnTrackDirection() const { return mRequestDirection; }
    f32 GetPlaceOnTrackSpeed() const { return mfSpeed; }
    void ClearPlaceOnTrack() { mbToBePlacedOnTrack = false; }
    EActiveRaceCarIndex GetActiveRaceCarIndex() const { return meIndex; }
    void GetResetCoords(Vector3* lpOutPosition, Vector3* lpOutDirection) const
    {
        ++miResetCoordsCalls; *lpOutPosition = mRingPosition; *lpOutDirection = mRingDirection;
    }
};

namespace RaceCarEntityModuleIO {
struct GameEventQueue
{
    int miEvents = 0; s32 miLastId = -1;
    bool AddEvent(const CgsModule::Event*, s32 liId, s32) { ++miEvents; miLastId = liId; return true; }
};
struct OutputBuffer_PrePhysics
{
    GameEventQueue mQueue;
    int miVehicleInterface = 0;
    GameEventQueue* GetGameEventQueue() { return &mQueue; }
    void* GetVehicleInputInterface() { return &miVehicleInterface; }
};
}

class RaceCarEntityModule
{
public:
    ActiveRaceCar maCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    bool mbInCarSelectScreen = false;
    s32 miResetType = 0;
    int miResets = 0; EActiveRaceCarIndex meResetIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    Matrix44Affine mResetTransform = {}; Vector3 mResetVelocity = {};
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex le) { return &maCars[le]; }
    bool IsInCarSelectScreen() const { return mbInCarSelectScreen; }
    s32 GetCarSelectResetType() const { return miResetType; }
    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const { return E_ACTIVE_RACE_CAR_INDEX_0; }
    void ResetActiveRaceCar(EActiveRaceCarIndex le, const Matrix44Affine& lrTransform, const Vector3& lrVelocity, void*)
    {
        ++miResets; meResetIndex = le; mResetTransform = lrTransform; mResetVelocity = lrVelocity;
    }
};

#include "fxg_pcot_helpers.inc"   // KF_PLACE_ON_TRACK_SIMILAR_EPSILON + AreVectorsSimilar, from the source

class PlaceOnTrackManager
{
public:
    RaceCarEntityModule* mpRaceCarEntityModule = nullptr;
    int miCarSelectCalls = 0;
    void GetValuesForCarSelect(const PlaceOnTrackCandidate*, const ActiveRaceCar*, Vector3* const, Vector3* const,
                               Vector3* const) { ++miCarSelectCalls; }
    void PlaceCarOnTrack(EActiveRaceCarIndex leActiveRaceCarIndex, const PlaceOnTrackCandidate* lpBestIntersection,
                         RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);
};
#include "fxg_pcot_body.inc"     // PlaceOnTrackManager::PlaceCarOnTrack, verbatim
}   // namespace BrnWorld

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main()
{
    using namespace BrnWorld;
    // F1: no usable intersection for slot 3 -- the ring holds an older road pose.
    {
        RaceCarEntityModule lModule;
        ActiveRaceCar& lrCar = lModule.maCars[3];
        lrCar.meIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        lrCar.mRequestPosition = Vector3{ 500.0f, 20.0f, -300.0f, 0.0f };   // the request (e.g. over nothing usable)
        lrCar.mRequestDirection = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        lrCar.mRingPosition = Vector3{ 480.0f, -4.0f, -310.0f, 0.0f };      // the oldest recorded road pose
        lrCar.mRingDirection = Vector3{ 0.0f, 0.0f, -1.0f, 0.0f };
        lrCar.mfSpeed = 10.0f;
        PlaceOnTrackManager lManager; lManager.mpRaceCarEntityModule = &lModule;
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics lOut;
        CgsDev::gText.clear(); gaAsserts.clear();
        lManager.PlaceCarOnTrack(E_ACTIVE_RACE_CAR_INDEX_3, nullptr, &lOut);

        Check(lrCar.miResetCoordsCalls == 1, "F1 the no-intersection arm calls ActiveRaceCar::GetResetCoords (0x822F73B0)");
        Check(lModule.miResets == 1 && lModule.meResetIndex == E_ACTIVE_RACE_CAR_INDEX_3
              && lModule.mResetTransform.wAxis.x == 480.0f && lModule.mResetTransform.wAxis.y == -4.0f
              && lModule.mResetTransform.wAxis.z == -310.0f,
              "F1 the car is reset onto the RING position, not the request (the retired PC park)");
        Check(lModule.mResetTransform.zAxis.z == -1.0f && lModule.mResetTransform.zAxis.x == 0.0f
              && lModule.mResetVelocity.z == -10.0f && lModule.mResetVelocity.x == 0.0f,
              "F1 ... facing the ring DIRECTION, and the velocity is that direction * speed");
        Check(lModule.mResetTransform.yAxis.x == 0.0f && lModule.mResetTransform.yAxis.y == 1.0f
              && lModule.mResetTransform.yAxis.z == 0.0f,
              "F1 the normal is the world Y axis (unk_82181510 = (0, 1, 0, 0))");
        const std::string lExpect =
            "    Failed to find valid place on track location - reverting to ring buffer\n"
            "    Selected reset data: lResetPosition=(480.000000, -4.000000, -310.000000), lResetNormal=(0.000000, "
            "1.000000, 0.000000), lResetDirection=(0.000000, 0.000000, -1.000000)\n";
        Check(CgsDev::gText.compare(0, lExpect.size(), lExpect) == 0,
              "F1 the console's two prints: the failure line, then \"Selected reset data\" with position, normal AND direction");
        Check(!lrCar.mbToBePlacedOnTrack && gaAsserts.empty() && lOut.mQueue.miEvents == 0,
              "F1 the request is cleared, no tripwire, no game event 13 (slot 3 is not the player)");
    }
    // F2: an intersection for the player (slot 0) -- unchanged arm: position/normal from the record, the request's
    // direction, the "Selected reset data" line, game event 13.
    {
        RaceCarEntityModule lModule;
        ActiveRaceCar& lrCar = lModule.maCars[0];
        lrCar.mRequestPosition = Vector3{ 10.0f, 5.0f, 20.0f, 0.0f };
        lrCar.mRequestDirection = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        lrCar.mRingPosition = Vector3{ -1.0f, -1.0f, -1.0f, 0.0f };
        lrCar.mRingDirection = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        lrCar.mfSpeed = 2.0f;
        PlaceOnTrackCandidate lBest;
        std::memset(&lBest, 0, sizeof(lBest));
        lBest.mPosition = Vector4{ 10.0f, -3.5f, 20.0f, 1.0f };
        lBest.mNormal = Vector4{ 0.0f, 1.0f, 0.0f, 0.0f };
        PlaceOnTrackManager lManager; lManager.mpRaceCarEntityModule = &lModule;
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics lOut;
        CgsDev::gText.clear(); gaAsserts.clear();
        lManager.PlaceCarOnTrack(E_ACTIVE_RACE_CAR_INDEX_0, &lBest, &lOut);
        Check(lrCar.miResetCoordsCalls == 0 && lModule.mResetTransform.wAxis.y == -3.5f
              && lModule.mResetTransform.zAxis.z == 1.0f && lModule.mResetVelocity.z == 2.0f,
              "F2 with an intersection: its position, the request's direction, no GetResetCoords");
        Check(CgsDev::gText.find("    Selected reset data: lResetPosition=(10.000000, -3.500000, 20.000000), lResetNormal="
                                 "(0.000000, 1.000000, 0.000000), lResetDirection=(0.000000, 0.000000, 1.000000)\n")
                  == 0,
              "F2 \"Selected reset data\" in the console's format (0x8201EA68 / 0x8201EA94 / 0x8201EAA4)");
        Check(lOut.mQueue.miEvents == 1 && lOut.mQueue.miLastId == 13 && gaAsserts.empty(),
              "F2 the player's placement posts game event 13; no tripwire");
    }

    std::printf("FxGeometricPlaceCarOnTrack: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
