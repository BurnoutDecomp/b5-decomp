// Actual collision records; lightweight fixtures replace the sound-state pool and
// manager construction. The runner inserts production method bodies at the marker.
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstdlib>
using namespace BrnSound::Logic;
using namespace BrnSound::Logic::Collision;

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* text, const char*, int) { std::fprintf(stderr, "%s\n", text); std::abort(); }
void* EndAssert() { return nullptr; }
} }

struct FixtureState
{
    FixtureState* next = nullptr;
    bool attached = true;
    FixtureState* GetNextState() const { return next; }
    bool IsAttached() const { return attached; }
};
struct FixtureCollisionState : FixtureState
{
    OutputCollision output;
    float time = 0;
    const OutputCollision& GetOutputCollision() const { return output; }
    float GetTimeWeAttached() const { return time; }
};
struct FixtureManager
{
    struct { Matrix44Affine mTransform = {}; } mCameraInfo;
    InputCollision maInputCollision[64];
    u32 mu32InputCollisionCount = 0;
    FrameInformation mFrameInformation;
    float mfCurrentTime = 0;
    FixtureState* head = nullptr;
    FixtureState* GetHeadState() const { return head; }
    void AddInputCollision(const InputCollision&);
    void CullInputCollisions();
    void CullInputCollisions_RemoveDuplicates();
    void CullAgainstPlaying();
};

// INSERT_PRODUCTION_METHODS

static void Check(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
static InputCollision Contact(u32 a, u32 b, float strength = 1)
{
    InputCollision c;
    c.maEntityID[0].muValue = a;
    c.maEntityID[1].muValue = b;
    c.maMaterial[0] = 11;
    c.maMaterial[1] = 22;
    c.maParameter[0] = VecFloat{strength, strength, strength, strength};
    return c;
}
int main()
{
    FixtureManager m;
    auto c = Contact(1, 2);
    c.mPosition.x = 70;
    m.AddInputCollision(c);
    c.mPosition.x = 70.01f;
    m.AddInputCollision(c);
    Check(m.mu32InputCollisionCount == 1, "70-metre distance boundary");

    m = FixtureManager();
    for (int i = 1; i <= 65; ++i) m.AddInputCollision(Contact(1, 2, static_cast<float>(i)));
    Check(m.mu32InputCollisionCount == 2, "full buffer culls duplicates and admits next collision");
    m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 1 && m.maInputCollision[0].maParameter[0].x == 65,
          "strongest repeated collision survives");

    m = FixtureManager();
    for (u32 i = 0; i < 65; ++i) m.AddInputCollision(Contact(i + 1, 100));
    Check(m.mu32InputCollisionCount == 64 && m.maInputCollision[63].maEntityID[0].muValue == 64,
          "full unique buffer drops incoming contact without overflow/assert");

    m = FixtureManager();
    m.AddInputCollision(Contact(1, 2, 3));
    m.AddInputCollision(Contact(2, 1, 5));
    m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 1 && m.maInputCollision[0].maEntityID[0].muValue == 2,
          "reversed regular pair keeps stronger contact");
    m = FixtureManager();
    c = Contact(1, 2); c.mePipeline = InputCollision::E_PROP; m.AddInputCollision(c);
    c = Contact(2, 1); c.mePipeline = InputCollision::E_PROP; m.AddInputCollision(c);
    m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 2, "prop pair order remains significant");

    m = FixtureManager();
    m.AddInputCollision(Contact(1, 2));
    c = Contact(1, 2); c.meOrientation = static_cast<AttribSys::Enums::eOrientation::eOrientation>(2);
    m.AddInputCollision(c); m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 2, "different orientations remain distinct");
    m = FixtureManager();
    m.AddInputCollision(Contact(1, 2, 2));
    c = Contact(1, 2, 1); c.maParameter[0].w = 3; m.AddInputCollision(c);
    m.CullInputCollisions();
    Check(m.maInputCollision[0].maParameter[0].w == 3, "duplicate comparison includes all four lanes");

    m = FixtureManager();
    for (u32 i = 1; i <= 4; ++i) m.AddInputCollision(Contact(i, 100));
    m.maInputCollision[0].mbCull = true; m.maInputCollision[2].mbCull = true;
    m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 2 && m.maInputCollision[0].maEntityID[0].muValue == 4
          && m.maInputCollision[1].maEntityID[0].muValue == 2, "reverse tail compaction order");

    FixtureCollisionState state;
    state.output.maEntityID[0].muValue = 1; state.output.maEntityID[1].muValue = 2;
    state.output.maMaterial[0] = 11; state.output.maMaterial[1] = 22;
    state.output.maParameter[0] = VecFloat{1,1,1,1};
    m = FixtureManager(); m.head = &state; m.mfCurrentTime = .1f;
    m.AddInputCollision(Contact(1, 2, 10)); m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 0, "playing contact suppresses up to tenfold strength");
    m.AddInputCollision(Contact(1, 2, 11)); m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 1, "larger new impact interrupts repeated sound suppression");
    m.mu32InputCollisionCount = 0; m.mfCurrentTime = .3f;
    m.AddInputCollision(Contact(1, 2)); m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 1, "sound exclusivity expires at 0.3 seconds");
    state.output.meFatality = E_FATAL_START;
    m.mu32InputCollisionCount = 0; m.mfCurrentTime = .1f;
    m.AddInputCollision(Contact(8, 9)); m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 0, "fatal-start sound excludes unrelated collisions");
    state.attached = false;
    m.AddInputCollision(Contact(8, 9)); m.CullInputCollisions();
    Check(m.mu32InputCollisionCount == 1, "detached sounds do not cull inputs");
    std::puts("PASS: 14 collision-audio culling checks");
}
