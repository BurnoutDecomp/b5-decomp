// Observe the production world/backdrop packet producers and category policies.
#define main ExistingWorldTests
#include "PCWorldReflectionDistance.cpp"
#undef main
#include <limits>
#include <thread>
using Renderable = CgsGraphics::Renderable;

namespace CgsGraphics {
class Camera { public: Vector3 mPosition; const Vector3& GetPosition() const { return mPosition; } };
}
namespace CgsPC::Reflections {
struct WorldCapture {
    static void SubmitBackdrops(BrnWorld::WorldEntityModule&, CgsGraphics::DispatchFrame*,
        const CgsGraphics::Camera&, const BrnWorld::ShaderLodInfo&, s32);
};
}
#include "pc_reflection_backdrops.inc"

int main()
{
    ExistingWorldTests();
    using namespace CgsPC::Reflections;
    using namespace CgsGraphics;
    using namespace BrnWorld;
    Backdrops() = {}; Traffic() = {}; Rivals() = {}; Lights() = {}; Particles() = {};
    renderengine::EnvironmentMapDrawDistancePC() = 0.0f;
    Check(CaptureDistance(75) == 75, "disabled categories preserve original capture distance");
    ObjectSettings lSettings;
    Check(!lSettings.IsVisible(1, 250), "new categories are opt-in");
    lSettings.mbEnabled = true;
    lSettings.mfDrawDistance = 100;
    Check(lSettings.IsVisible(10000, 250) && !lSettings.IsVisible(10001, 250), "fixed distance includes its boundary");
    lSettings.miDistanceMode = E_DISTANCE_RELATIVE;
    lSettings.mfDrawDistanceScale = 0.25f;
    Check(lSettings.GetDrawDistance(400) == 100 && lSettings.GetDrawDistance(800) == 200,
        "relative draw distance follows live normal distance");
    Check(!lSettings.IsVisible(std::numeric_limits<float>::quiet_NaN(), 250), "non-finite object distances are rejected");
    lSettings.mfDrawDistanceScale = std::numeric_limits<float>::infinity();
    Check(lSettings.GetDrawDistance(200) == 100, "invalid scale falls back to a finite policy");
    lSettings.mfDrawDistanceScale = 10;
    Check(lSettings.GetDrawDistance(2000) == 10000, "draw distance stays inside capture limit");
    {
        VehicleScope lOuter(E_CAPTURE_TRAFFIC);
        Check(IsVehicleCapture() && &VehicleSettings() == &Traffic(), "traffic policy applies only within its capture");
        { VehicleScope lInner(E_CAPTURE_RIVALS); Check(&VehicleSettings() == &Rivals(), "nested capture selects rival policy"); }
        Check(&VehicleSettings() == &Traffic(), "nested capture restores traffic policy");
        bool lbOtherThread = true;
        std::thread lThread([&] { lbOtherThread = IsVehicleCapture(); }); lThread.join();
        Check(!lbOtherThread, "capture scope does not leak into another thread");
    }
    Check(!IsVehicleCapture(), "main view policy resumes after capture");
    {
        VehicleScope lScope(E_CAPTURE_PLAYER_WHEELS);
        Check(IsPlayerWheelCapture() && &WheelSettings()==&PlayerWheels(),
            "player wheels use an independent policy while the shell is excluded");
        PlayerWheels().mbEnabled=true;PlayerWheels().miDistanceMode=E_DISTANCE_RELATIVE;
        PlayerWheels().mfDrawDistanceScale=0.5f;
        Check(PlayerWheels().IsVisible(20*20,40) && !Rivals().mbEnabled,
            "player wheels can be captured with rival capture disabled");
        PlayerWheels()={};
    }
    Check(!IsPlayerWheelCapture() && &WheelSettings()==&Wheels(),
        "player-wheel policy does not leak into other passes");
    Decals().mbEnabled=true;Decals().miDistanceMode=E_DISTANCE_RELATIVE;Decals().mfDrawDistanceScale=0.25f;
    sfParticleNormalDistance=800;
    Check(!Particles().mbEnabled && Decals().GetDrawDistance(sfParticleNormalDistance)==200
        && CaptureDistance(75)==200,"decals have an independent relative capture radius");
    Decals()={};sfParticleNormalDistance=10000;

    Model lModel;
    ObjectSettings lRelative;
    lRelative.mLod.miMode = renderengine::E_ENVIRONMENT_MAP_LOD_RELATIVE;
    const f32 lafNormal[5] = {50,110,175,250,300};
    SetVehicleLodDistances(lafNormal);
    Check(SelectVehicleLod(lRelative, &lModel, 25 * 25) == 0
        && SelectVehicleLod(lRelative, &lModel, 26 * 26) == 1
        && SelectVehicleLod(lRelative, &lModel, 56 * 56) == 2,
        "relative vehicle LOD follows live quality thresholds without box proxies");
    {
        VehicleScope lScope(E_CAPTURE_TRAFFIC, 8, 600);
        Check(GlassMeshList() == 28 && sfVehicleDrawDistance == 600,
            "glass is routed to its own face list and uses the parent vehicle distance");
    }
    Check(sfVehicleDrawDistance == NormalVehicleDistance() || sfVehicleDrawDistance == 50,
        "vehicle scope restores its caller distance");
    Instance laInstances[2] = {{&lModel,{}}, {&lModel,{}}};
    InstanceList lList = {laInstances, 1, 2};
    WorldEntityModule lWorld;
    lWorld.miCurrentBackdropZoneId = 7;
    lWorld.mWorldGraphicsStreamer.mpList = &lList;
    DispatchFrame lFrame;
    Camera lCamera;
    ShaderLodInfo lShaderLod;
    auto submit = [&](f32 lfDistance) {
        laInstances[1].mTransform.mPosition.x = lfDistance;
        const u32 luBefore = lFrame.maLists[5].muCount;
        WorldCapture::SubmitBackdrops(lWorld, &lFrame, lCamera, lShaderLod, 5);
        return lFrame.maLists[5].muCount - luBefore;
    };
    Check(submit(100) == 0, "disabled backdrop feed emits no packet");
    Backdrops().mbEnabled = true;
    Backdrops().mfDrawDistance = 150;
    Check(submit(75) == 0 && submit(76) == 1, "backdrops start after the detailed world radius");
    Check(siOpaque == 5 && siTransparent == 5 && su8Technique == 7, "backdrops use this face's mesh routes and environment technique");
    Check(submit(150) == 1 && submit(151) == 0, "backdrop cutoff is independent from world distance");
    renderengine::EnvironmentMapDrawDistancePC() = 125;
    Check(submit(125) == 0 && submit(126) == 1, "backdrop shell follows an extended world radius");
    renderengine::EnvironmentMapDrawDistancePC() = 0;
    Backdrops().miDistanceMode = E_DISTANCE_RELATIVE;
    Backdrops().mfDrawDistanceScale = 0.25f;
    laInstances[1].mfMaxDrawDistanceSq = 400 * 400;
    Check(submit(100) == 1 && submit(101) == 0, "relative backdrop cutoff uses authored instance distance");
    lModel.mabExists[1] = lModel.mabExists[2] = false;
    Check(submit(100) == 1 && siDrawState == 0, "single-mesh backdrops remain visible without LOD2");
    Backdrops().mLod.miMode = renderengine::E_ENVIRONMENT_MAP_LOD_CUSTOM;
    Backdrops().mLod.mafTransitionDistances[0] = 90;
    Backdrops().mLod.mafTransitionDistances[1] = 95;
    Check(submit(100) == 1, "distance LOD modes preserve sparse backdrop meshes");
    Check(lList.muNumInstances == 1 && lWorld.miCurrentBackdropZoneId == 7,
        "reflection feed preserves resident stream state");
    std::printf("PCReflectionScene: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
