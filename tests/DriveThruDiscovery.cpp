#include <cmath>
#include <cstdio>
#include <initializer_list>
using f32 = float;
using s32 = int;
using u8 = unsigned char;
using CgsID = unsigned long long;
struct Vector3 { float x, y, z; };
namespace rw { namespace math { namespace vpu {
Vector3 operator-(Vector3 a, Vector3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
}}}
namespace BrnTrigger {
struct GenericRegion {
    enum Type { E_TYPE_JUNKYARD, E_TYPE_GAS, E_TYPE_BODY, E_TYPE_PAINT, E_TYPE_CAR_PARK };
    CgsID GetId() const { return 1234; }
};
}
struct Profile {
    bool mbKnown = false;
    bool IsDriveThruDiscoverd(CgsID, BrnTrigger::GenericRegion::Type) { return mbKnown; }
    int GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::Type) { return 1; }
};
struct Progression {
    Profile mProfile;
    int miDiscoveries = 0;
    void OnDriveThru(CgsID, BrnTrigger::GenericRegion::Type, void*) {
        ++miDiscoveries;
        mProfile.mbKnown = true;
    }
};
struct Car {
    bool mbActive = true;
    Vector3 mPosition{0,0,0};
    bool IsPlayerCarActive() const { return mbActive; }
    Vector3 GetPlayerPosition() const { return mPosition; }
};
struct Fixture {
    bool lbIsInJunkyard = false, lbIsOnline = false, lbIsFreeburn = true;
    u8 leType = 1;
    BrnTrigger::GenericRegion mRegion;
    struct Entry {
        Vector3 mPosition{0,0,0};
        BrnTrigger::GenericRegion* mpGenericRegion;
    } lrEntry{{0,0,0}, &mRegion};
    Progression mProgression;
    Progression* mpProgressionManager = &mProgression;
    Profile* lpProfile = &mProgression.mProfile;
    Car mCar;
    Car* lpRcOutputInterface = &mCar;
    void* lpActionQueue = nullptr;
    bool mbCurrentDriveThruJustFound = false;
    int miNumDriveThrusOfThisTypeFound = 0, miTotalDriveThrusOfThisType = 0;
    BrnTrigger::GenericRegion::Type meDiscoveredDriveThruType{};
    int GetTotalDriveThrusOfType(BrnTrigger::GenericRegion::Type) { return 12; }
    void Step();
};
#include "drive_thru_discovery.inc"
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool pass, const char* name) {
        ++checks;
        if (!pass) { ++failures; std::printf("FAIL %s\n", name); }
    };
    for (int online = 0; online < 2; ++online)
        for (int freeburn = 0; freeburn < 2; ++freeburn) {
            Fixture f;
            f.lbIsOnline = online != 0; f.lbIsFreeburn = freeburn != 0;
            f.mCar.mPosition = {40,0,0};
            f.Step();
            check(f.mProgression.miDiscoveries == freeburn, "discovery follows freeburn, offline and online");
        }
    for (int type = 0; type < 4; ++type) {
        Fixture f; f.leType = static_cast<u8>(type); f.mCar.mPosition = {60,0,0};
        f.Step();
        check(f.mProgression.miDiscoveries == 1, "all four service types discovered without entering");
        check(f.mbCurrentDriveThruJustFound && f.miNumDriveThrusOfThisTypeFound == 1 &&
              f.miTotalDriveThrusOfThisType == 12 &&
              f.meDiscoveredDriveThruType == type, "discovery arms correct HUD counts and type");
        f.Step();
        check(f.mProgression.miDiscoveries == 1, "already discovered service is not counted again");
    }
    {
        Fixture f; f.leType = 4; f.Step();
        check(f.mProgression.miDiscoveries == 0, "car parks require entry");
    }
    {
        Fixture f; f.lbIsInJunkyard = true; f.Step();
        check(f.mProgression.miDiscoveries == 0, "no discovery during junkyard selection");
    }
    {
        Fixture f; f.mCar.mbActive = false; f.Step();
        check(f.mProgression.miDiscoveries == 0, "inactive car cannot discover");
    }
    for (Vector3 p : {Vector3{75,0,0}, Vector3{0,5,0}, Vector3{74.9f,4.9f,0},
                     Vector3{74,4,0}}) {
        Fixture f; f.mCar.mPosition = p; f.Step();
        const bool expected = p.x*p.x+p.y*p.y+p.z*p.z < 5625 && std::fabs(p.y) < 5;
        check((f.mProgression.miDiscoveries != 0) == expected, "strict 3D radius and vertical boundary");
    }
    std::printf("DriveThruDiscovery: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
