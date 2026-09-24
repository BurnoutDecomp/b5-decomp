// FX-GEOMETRIC (crash parity 2026-09-24): BrnWorld::PlaceOnTrackManager::PostSceneUpdate @0x822D3168 -- the
// PRODUCER of the place-on-track fine line test (DWARF BrnPlaceOnTrackManager.h:49, PS3 0x14A31C). Absent before:
// the PC answered requests with ApplyPendingRequestsWithoutSceneQueryBringUp instead. run_fxgeometric_place_on_track.py
// extracts the production body + the constants it names; the IO buffer / module / car here are small fakes that
// record what the body does. Expectations, from the ARTIST asm:
//   for EActiveRaceCarIndex 0..7 (the :39 post-increment tripwire): skip unless IsAttached() && mbToBePlacedOnTrack
//   lEvent.mLineStart = mPlaceOnTrackPosition + (0, 50, 0, 0)     vaddfp128 v127 @0x822D3290 (flt_820138DC = 50.0)
//   lEvent.mLineEnd   = mPlaceOnTrackPosition - (0, 50, 0, 0)     vsubfp128 v126 @0x822D32A8 (w lane: pos.w -/+ 0)
//   mQueryId = Set(5, (u16)slot)                                  clrlwi 16 ; oris 5          @0x822D3254/64
//   mx32EntityTypeFlags = 2 (K_ENTITY_TYPE_FLAG_WORLD)            li r17, 2 @0x822D31C8
//   mExcludeEntityId = invalid (-1)                               li r26, -1 @0x822D31CC
//   meExclusionMode = 0 (E_EXCLUDE_ENTITY_ONLY)                   stw r24(0) var_D4
//   mxVolumeTypeFlags = 0xFF                                      li r18, 0xFF @0x822D31D8
//   prints (gxMessageFilterFlags & 1), then GetSceneFineLineTestQueue()->AddEvent, then "Line test requested"
//   the producer never clears mbToBePlacedOnTrack (PrePhysicsUpdate's PlaceCarOnTrack does).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"   // the REAL InEventLineTestFine
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
// The log sink: every piece of text lands in gText.
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

namespace BrnWorld {
class ActiveRaceCar
{
public:
    bool mbAttached = false;
    bool mbToBePlacedOnTrack = false;
    Vector3 mPlaceOnTrackPosition = {};
    bool IsAttached() const { return mbAttached; }
    bool ToBePlacedOnTrack() const { return mbToBePlacedOnTrack; }
    const Vector3& GetPlaceOnTrackPosition() const { return mPlaceOnTrackPosition; }
};
class RaceCarEntityModule
{
public:
    ActiveRaceCar maCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    std::vector<int> maAsked;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) { maAsked.push_back(static_cast<int>(leIndex)); return &maCars[leIndex]; }
};
namespace RaceCarEntityModuleIO {
struct SceneFineLineTestQueue
{
    std::vector<CgsSceneManager::SceneManagerIO::InEventLineTestFine> maEvents;
    bool AddEvent(const CgsSceneManager::SceneManagerIO::InEventLineTestFine& lrEvent)
    {
        CgsDev::gText += "<AddEvent>";
        maEvents.push_back(lrEvent);
        return true;
    }
};
struct OutputBuffer_PostScene
{
    SceneFineLineTestQueue mQueue;
    int miGets = 0;
    SceneFineLineTestQueue* GetSceneFineLineTestQueue() { ++miGets; return &mQueue; }
};
}

#include "fxg_pot_consts.inc"   // the constants the body names, from the sources

class PlaceOnTrackManager
{
public:
    void PostSceneUpdate(RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput);
    RaceCarEntityModule* mpRaceCarEntityModule = nullptr;
};
#include "fxg_pot_body.inc"     // PlaceOnTrackManager::PostSceneUpdate, verbatim
}   // namespace BrnWorld

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static u32 Word(const CgsSceneManager::SceneQueryId& lrId) { u32 lu; std::memcpy(&lu, &lrId, 4); return lu; }
static u32 Word(const CgsSceneManager::EntityId& lrId) { u32 lu; std::memcpy(&lu, &lrId, 4); return lu; }

int main()
{
    using namespace BrnWorld;
    RaceCarEntityModule lModule;
    for (int i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i)
        lModule.maCars[i].mPlaceOnTrackPosition = Vector3{ 100.0f + i, 2.0f * i, -50.0f - i, 0.5f * i };
    lModule.maCars[1].mbAttached = true;  lModule.maCars[1].mbToBePlacedOnTrack = true;
    lModule.maCars[2].mbAttached = true;  lModule.maCars[2].mbToBePlacedOnTrack = false;   // nothing pending
    lModule.maCars[4].mbAttached = true;  lModule.maCars[4].mbToBePlacedOnTrack = true;
    lModule.maCars[5].mbAttached = false; lModule.maCars[5].mbToBePlacedOnTrack = true;    // detached
    PlaceOnTrackManager lManager;
    lManager.mpRaceCarEntityModule = &lModule;
    RaceCarEntityModuleIO::OutputBuffer_PostScene lOutput;

    CgsDev::gText.clear();
    lManager.PostSceneUpdate(&lOutput);
    const std::vector<CgsSceneManager::SceneManagerIO::InEventLineTestFine>& e = lOutput.mQueue.maEvents;

    Check(lModule.maAsked.size() == 8 && lModule.maAsked[0] == 0 && lModule.maAsked[7] == 7,
          "P1 every active slot 0..7 is visited (GetActiveRaceCar per slot)");
    Check(e.size() == 2, "P1 exactly the attached cars with a pending request post a line test (slots 1 and 4)");
    Check(e.size() == 2 && Word(e[0].mQueryId) == 0x00050001u && Word(e[1].mQueryId) == 0x00050004u,
          "P2 mQueryId = Set(5, slot) (`clrlwi 16 ; oris 5`), slot order");
    Check(e.size() == 2 && e[0].mLineStart.x == 101.0f && e[0].mLineStart.y == 52.0f && e[0].mLineStart.z == -51.0f
          && e[0].mLineStart.w == 0.5f,
          "P2 mLineStart = position + (0, 50, 0, 0), w lane carried (vaddfp128)");
    Check(e.size() == 2 && e[1].mLineEnd.x == 104.0f && e[1].mLineEnd.y == 8.0f - 50.0f && e[1].mLineEnd.z == -54.0f
          && e[1].mLineEnd.w == 2.0f,
          "P2 mLineEnd = position - (0, 50, 0, 0) (vsubfp128): a 100 m vertical line");
    Check(e.size() == 2 && e[0].mx32EntityTypeFlags == 2u && Word(e[0].mExcludeEntityId) == 0xFFFFFFFFu
          && e[0].meExclusionMode == CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY && e[0].mxVolumeTypeFlags == 0xFF,
          "P2 world-only (2), no exclude entity (-1), E_EXCLUDE_ENTITY_ONLY, every volume type (0xFF)");
    Check(lModule.maCars[1].mbToBePlacedOnTrack && lModule.maCars[4].mbToBePlacedOnTrack,
          "P3 the producer only asks -- mbToBePlacedOnTrack is cleared by PrePhysicsUpdate's placement, not here");
    const std::string lExpect =
        "[PLACEONTRACK] Generating line test to place race car 1 on track\n"
        "    lEvent.mLineStart=(101.000000, 52.000000, -51.000000), lEvent.mLineEnd=(101.000000, -48.000000, -51.000000)\n"
        "<AddEvent>[PLACEONTRACK] Line test requested\n";
    Check(CgsDev::gText.compare(0, lExpect.size(), lExpect) == 0,
          "P4 the console's three prints, in order around the AddEvent (gxMessageFilterFlags & 1)");

    CgsDev::gText.clear();
    CgsDev::Message::gxMessageFilterFlags = 0;
    RaceCarEntityModuleIO::OutputBuffer_PostScene lQuiet;
    lManager.PostSceneUpdate(&lQuiet);
    Check(lQuiet.mQueue.maEvents.size() == 2 && CgsDev::gText == "<AddEvent><AddEvent>",
          "P5 with the message filter off the same two events post and nothing prints");
    Check(gaAsserts.empty(), "P6 no tripwire (the slot walk stays within E_ACTIVE_RACE_CAR_INDEX_COUNT)");

    std::printf("FxGeometricPlaceOnTrack: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
