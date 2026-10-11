// Complete production UpdateResolver/AddInputCollision/GetRandomSampleID and
// normalization bodies. Real descriptor/frame/history/RNG types; queue, resource
// and renderer endpoints are instrumented. Original-broken synthetic records are
// deliberately disabled, so their indeterminate fields cannot create a false pass.
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

static u32 suAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*,const char*,int) { ++suAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }
struct TestLog : CgsDev::StrStreamBase
{ CgsDev::StrStreamBase& operator<<(const char*) override { return *this; } };
namespace CgsDev { namespace Log { StrStreamBase* gpDebugPrint = nullptr; } }
namespace BrnSound { namespace Debug { struct DebugComponent { static TestLog mLogCombined; };
TestLog DebugComponent::mLogCombined; } }

namespace BrnPhysics { namespace Deformation {
enum { E_GLASS_STATE_SMASHED=2 };
struct GlassSmashOrCrackEvent
{
    EntityId mVehicleEntityId{0};
    s32 meGlassPart=0,meNewState=0;
    bool mbDontPlaySmashEffect=false;
    f32 mfCrackAmount=0;
};
} }
struct Queue
{
    s32 kind, length = 1;
    BrnPhysics::Deformation::GlassSmashOrCrackEvent event;
    s32 GetLength() const { return length; }
    const BrnPhysics::Deformation::GlassSmashOrCrackEvent& GetEvent(s32) const { return event; }
};
namespace BrnPhysics
{
namespace ContactSpy
{
    struct ContactSpyInterface
    {
        bool valid = true;
        mutable Queue race{1}, traffic{2}, discarded{3}, physical{8}, props{9};
        bool IsValid() const { return valid; }
        Queue* GetRaceCarContacts() const { return &race; }
        Queue* GetTrafficContacts() const { return &traffic; }
        Queue* GetDiscardedContacts() const { return &discarded; }
        Queue* GetPhysicalCarPartContacts() const { return &physical; }
        Queue* GetPropContacts() const { return &props; }
    };
}
namespace Deformation
{
    struct DeformationState {};
    struct DeformationOutputInterface
    {
        DeformationState* mpDeformationState = nullptr;
        Queue mGlassSmashOrCrackQueue{4}, mJointedPartStateQueue{5};
        Queue mBrokenJointNotificationQueue{6}, mDetachedPartNotificationQueue{7};
    };
}
}
namespace BrnDirector { namespace Camera { struct Camera { Matrix44Affine transform; Camera() { transform.SetIdentity(); } }; } }
struct VehicleInterface {};
struct GameModeInterface {};
namespace BrnSound { namespace Module {
struct SoundLogicModule
{
    CgsNumeric::Random random;
    SoundLogicModule() { random.Construct(); }
    CgsNumeric::Random& GetRandomGenerator() { return random; }
};
namespace Io
{
    struct RootInputBuffer
    {
        BrnDirector::Camera::Camera camera;
        BrnPhysics::ContactSpy::ContactSpyInterface contacts;
        BrnPhysics::Deformation::DeformationOutputInterface deformation;
        VehicleInterface vehicles;
        GameModeInterface mode;
        const BrnDirector::Camera::Camera* GetDirectorCamera() const { return &camera; }
        const BrnPhysics::ContactSpy::ContactSpyInterface& GetContactSpyQueueInterface() const { return contacts; }
        const BrnPhysics::Deformation::DeformationOutputInterface& GetDeformationInterface() const { return deformation; }
        const VehicleInterface* GetVehicleInterface() const { return &vehicles; }
        const GameModeInterface& GetGameModeInterface() const { return mode; }
    };
}
} }
namespace Attrib { namespace Gen {
struct crashbinlist {};
struct propscrashbinlist {};
struct crashbin
{
    inline static int counts[3] = {2,2,2};
    inline static int samples[3][2] = {{10,11},{20,21},{30,31}};
    crashbin(u64,void*) {}
    bool IsValid() const { return true; }
    const int& mNumCollisionsLarge() const { return counts[0]; }
    const int& mNumCollisionsMedium() const { return counts[1]; }
    const int& mNumCollisionsSmall() const { return counts[2]; }
    const int& mCollisionsLarge(u32 i) const { return samples[0][i]; }
    const int& mCollisionsMedium(u32 i) const { return samples[1][i]; }
    const int& mCollisionsSmall(u32 i) const { return samples[2][i]; }
};
struct propscrashbin : crashbin { propscrashbin(u64 k,void* p) : crashbin(k,p) {} };
} }

namespace BrnSound { namespace Logic { namespace Collision {
bool CollisionAudioDiagEnabled() { return false; }
u32 MapCameraStateToBinFlags(const BrnDirector::Camera::Camera&) { return 1; }
u32 MapGameModesToBinFlags(const GameModeInterface&) { return 1; }
u32 GetEntityOwner(EntityId id) { return id.muValue>>24; }
u32 GetEntityIndex(EntityId id) { return (id.muValue>>10)&0x3FFF; }
u32 DebugFlagIndex(u64 n) { u32 i=static_cast<u32>(-1); while(n) { ++i; n>>=1; } return i; }
struct CameraInfo { Matrix44Affine mTransform; CameraInfo() { mTransform.SetIdentity(); } };
struct RaceCarCache
{
    static constexpr u32 KU_MAX_NUM_RACE_CARS = 8;
    struct RaceCarCacheNode
    {
        CgsSound::Utils::DataPoint<bool> mbActive{false};
        CgsSound::Utils::DataPoint<Matrix44Affine> mTransform;
        Vector3 mMin{},mMax{},mComOffset{};
    } nodes[8];
    void Update(const VehicleInterface&,const BrnPhysics::Deformation::DeformationState&) {}
    const RaceCarCacheNode* GetRaceCar(u32 i) const { return &nodes[i]; }
};
struct Cache
{
    u32 builds=0;
    template<class L,class B> void Build(const L&) { ++builds; }
};
enum { E_COLLISION_SPLICE_BANK_COLLISION=0, E_COLLISION_SPLICE_BANK_MAX=1 };
class CollisionStateManager
{
public:
    static constexpr u32 E_MAX_SCRAPE_HISTORY=16;
    Cache maBinLoopupCache[2];
    Attrib::Gen::crashbinlist mCrashBinList;
    Attrib::Gen::propscrashbinlist mPropsCrashBinList;
    FrameInformation mFrameInformation;
    RaceCarCache mRaceCarCache;
    CameraInfo mCameraInfo;
    ScrapeInfo maScrapeHistory[E_MAX_SCRAPE_HISTORY];
    InputCollision maInputCollision[64];
    u32 mu32InputCollisionCount=0,mu32OutputCollisionCount=0;
    u32 mx32CameraBinFlags=0,mx32GameModeBinFlags=0;
    f32 mfCurrentTime=0;
    std::vector<s32> calls;
    Module::SoundLogicModule* module=nullptr;
    CgsSound::Utils::SelectionHistory<512,u16,u16,65536> maSelectionHistory[1];
    void* GetLogicModule() { return module; }
    template<class T> void ImportContactSpies(const T& q,const LogicInputBuffer&,f32,f32) { calls.push_back(q.kind); }
    void UpdateGlass(const BrnPhysics::Deformation::DeformationOutputInterface&) { calls.push_back(4); }
    void UpdateHingingBodyParts(const Queue*) { calls.push_back(5); }
    void SetCameraInfo(const BrnDirector::Camera::Camera& c) { mCameraInfo.mTransform=c.transform; }
    void UpdateScrapes(const FrameInformation&) { calls.push_back(10); }
    void CullInputCollisions() { calls.push_back(11); }
    void ProcessCollisions() { calls.push_back(12); }
    void AddInputCollision(const InputCollision&);
    void UpdateResolver(const LogicInputBuffer&,const FrameInformation&,f32);
    template<class B> void GetRandomSampleID(OutputCollision&);
};
// Instrumented output endpoint, outside the deterministic behavior under test.
void InputCollision::DebugPrint(const CollisionStateManager&) const {}
} } }

#include "sound_statistics_batch_collision.inc"

static u32 suChecks=0,suFailures=0;
static void Check(bool b,const char* n) { ++suChecks; if(!b) { ++suFailures; std::printf("FAIL: %s\n",n); } }
static bool Near(f32 a,f32 b) { return std::fabs(a-b)<0.00001f; }
int main()
{
    using namespace BrnSound::Logic;
    using namespace BrnSound::Logic::Collision;
    BrnSound::Module::SoundLogicModule module;
    CollisionStateManager mgr;
    mgr.module=&module;
    BrnSound::Module::Io::RootInputBuffer input;
    FrameInformation frame;
    frame.mPlayerTransform.SetIdentity();
    mgr.UpdateResolver(input,frame,1.0f/60);
    Check(mgr.calls==std::vector<s32>({1,2,3,4,5,6,7,8,9,10,11,12}), "ARTIST complete contact-group and resolver order");
    Check(mgr.maBinLoopupCache[0].builds==1 && mgr.maBinLoopupCache[1].builds==1, "filter controls preserve both per-frame cache rebuilds");
    mgr.calls.clear(); KI_DEBUG_FILTER_NO_COLLISION=1;
    mgr.UpdateResolver(input,frame,1.0f/60);
    Check(mgr.calls==std::vector<s32>({4,5,6,7,8,9,10,11,12}), "No Normal removes race, traffic and discarded group together");
    mgr.calls.clear(); KI_DEBUG_FILTER_NO_COLLISION=0; KI_DEBUG_FILTER_NO_GLASS=1;
    mgr.UpdateResolver(input,frame,1.0f/60);
    Check(mgr.calls==std::vector<s32>({1,2,3,5,6,7,8,9,10,11,12}), "No Glass suppresses only glass import");
    mgr.calls.clear(); KI_DEBUG_FILTER_NO_GLASS=0; KI_DEBUG_FILTER_NO_BODY_PARTS=1;
    mgr.UpdateResolver(input,frame,1.0f/60);
    Check(mgr.calls==std::vector<s32>({1,2,3,4,9,10,11,12}), "No bodyparts removes hinge, joint, detach and physical-part group");
    mgr.calls.clear(); KI_DEBUG_FILTER_NO_BODY_PARTS=0; KI_DEBUG_FILTER_NO_PROPS=1;
    mgr.UpdateResolver(input,frame,1.0f/60);
    Check(mgr.calls==std::vector<s32>({1,2,3,4,5,6,7,8,10,11,12}), "No Props suppresses prop import");
    mgr.calls.clear(); KI_DEBUG_FILTER_NO_COLLISION=1; KI_DEBUG_FILTER_NO_GLASS=1; KI_DEBUG_FILTER_NO_BODY_PARTS=1;
    mgr.UpdateResolver(input,frame,1.0f/60);
    Check(mgr.calls==std::vector<s32>({10,11,12}), "all import switches retain scrape, cull and process tail");
    KI_DEBUG_FILTER_NO_COLLISION=KI_DEBUG_FILTER_NO_GLASS=KI_DEBUG_FILTER_NO_BODY_PARTS=KI_DEBUG_FILTER_NO_PROPS=0;
    mgr.calls.clear(); input.contacts.valid=false;
    mgr.UpdateResolver(input,frame,1.0f/60);
    Check(mgr.calls==std::vector<s32>({10,11,12}), "invalid contact input keeps original resolver tail");

    InputCollision hit;
    hit.mPosition={0,0,0,0}; hit.maMaterial[0]=(1ull<<3)|(1ull<<7); hit.maMaterial[1]=1ull<<10;
    hit.maEntityID[0]={0}; hit.maEntityID[1]={0}; hit.mfPriorityAddition=0;
    hit.meAction=AttribSys::Enums::eAction::Collision; hit.meOrientation=AttribSys::Enums::eOrientation::Front;
    hit.mePipeline=InputCollision::E_REGULAR; hit.mbCull=false;
    for(auto& p:hit.maParameter) p=VecFloat{};
    const auto Accept = [&]() { mgr.mu32InputCollisionCount=0; mgr.AddInputCollision(hit); return mgr.mu32InputCollisionCount==1; };
    Check(Accept(), "unfiltered regular contact is retained");
    KI_DEBUG_FILTER_MATERIAL=3;
    Check(Accept(), "regular material filter accepts side A overlap");
    KI_DEBUG_FILTER_MATERIAL=10;
    Check(Accept(), "regular material filter accepts side B overlap");
    KI_DEBUG_FILTER_MATERIAL=11;
    Check(!Accept(), "regular material filter rejects absent bit");
    hit.mePipeline=InputCollision::E_PROP; KI_DEBUG_FILTER_MATERIAL=3;
    Check(!Accept(), "prop material filtering ignores matching side A");
    KI_DEBUG_FILTER_MATERIAL=10;
    Check(Accept(), "prop material filtering accepts side B");
    hit.maMaterial[1]=1ull<<40; KI_DEBUG_FILTER_MATERIAL=40;
    Check(Accept(), "material filtering retains full u64 mask width");
    KI_DEBUG_FILTER_MATERIAL=0; hit.mePipeline=InputCollision::E_REGULAR;
    KI_DEBUG_FILTER_ACTION=1;
    Check(Accept(), "action option one accepts Collision bit one");
    KI_DEBUG_FILTER_ACTION=2;
    Check(!Accept(), "action filter uses exact selected bit");
    hit.meAction=static_cast<AttribSys::Enums::eAction::eAction>(3);
    Check(!Accept(), "action bit overlap alone is rejected");
    KI_DEBUG_FILTER_ACTION=-1;
    Check(Accept(), "nonpositive action option disables filtering");
    KI_DEBUG_FILTER_ACTION=0; hit.mPosition={70,0,0,0};
    Check(Accept(), "early distance boundary seventy is inclusive");
    hit.mPosition={70.01f,0,0,0};
    Check(!Accept(), "early distance beyond seventy is culled");

    OutputCollision out;
    out.mBinKey=1; out.meBankType=0; out.meSize=E_SIZE_LARGE;
    mgr.GetRandomSampleID<Attrib::Gen::crashbin>(out);
    Check(out.miSampleID==10 || out.miSampleID==11, "real history picks an authored large candidate");
    out.meSize=E_SIZE_MEDIUM;
    mgr.GetRandomSampleID<Attrib::Gen::crashbin>(out);
    Check(out.miSampleID==20 || out.miSampleID==21, "real history picks authored medium candidates");
    out.meSize=E_SIZE_SMALL;
    mgr.GetRandomSampleID<Attrib::Gen::propscrashbin>(out);
    Check(out.miSampleID==30 || out.miSampleID==31, "prop path uses authored small candidates");
    KI_DEBUG_SAMPLE_ID_TO_PLAY_LARGE=400; KI_DEBUG_SAMPLE_ID_TO_PLAY_SMALL=7;
    mgr.GetRandomSampleID<Attrib::Gen::crashbin>(out);
    Check(out.miSampleID==407, "live force controls add the two original ID fields");
    KI_DEBUG_SPAWN_LOTS_OF_SAMPLES=1;
    // Public SetSeed primes eight LCG steps. These seeds put the old high word
    // at 202/100 for the next ARTIST modulo-101 integer draw.
    module.random.SetSeed(0xEE2701AC76E1BE28ull);
    mgr.GetRandomSampleID<Attrib::Gen::propscrashbin>(out);
    Check(out.miSampleID==0, "Play Tons overrides forced ID with original modulo101 draw");
    module.random.SetSeed(0x8DB5440676E1BE28ull);
    mgr.GetRandomSampleID<Attrib::Gen::crashbin>(out);
    Check(out.miSampleID==100, "original random range includes endpoint100");
    KI_DEBUG_SPAWN_LOTS_OF_SAMPLES=0; KI_DEBUG_SAMPLE_ID_TO_PLAY_SMALL=-1;
    mgr.GetRandomSampleID<Attrib::Gen::crashbin>(out);
    Check(out.miSampleID==30 || out.miSampleID==31, "disabling force restores real authored selection");

    const Vector3 stress={320000,0,0,0};
    Check(Near(NormalisedImpulse(stress,1),1), "original CRT normalization product");
    KF_FASTEST_COLLISION=400; KF_BIGGEST_THING_MASS=3200;
    Check(Near(NormalisedImpulse(stress,1),1), "menu edits preserve original CRT-only product behavior");
    Check(Near(NormalisedImpulse(stress,0.5f),2), "actual time-step normalization remains live");
    Check(suAsserts==0, "deterministic inputs produce no assertions");
    std::printf("SoundStatisticsBatchCollision: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures ? 1 : 0;
}
