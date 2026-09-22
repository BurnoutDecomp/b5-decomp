// Execute the production shaping block, including both state branches.
// Solver and deformation-chain integration are outside this numerical fixture.
#include "BrnCommonTypes.h"
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#undef private
#include "rw/physics/rigidbody.h"
#include <initializer_list>
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnImpulsePasser.h"
#include <cmath>
#include <cstdio>

namespace vpu = rw::math::vpu;


// The small receiver supplies/captures the block's external calls. Types of
// vectors, masses, entity IDs and RNG are the production types.
struct ShapeVehicle
{
    bool showtime = false, boosting = false;
    VecFloat mass{1000,1000,1000,1000};
    Vector3 velocity{3,-5,7,9};
    unsigned reports = 0;
    Vector3 reportedPoint{};
    EntityId reportedOther{};
    bool reportedCar = false, reportedGood = false;
    bool IsPlayerVehicleInShowtime() const { return showtime; }
    bool IsBounceBoosting() const { return boosting; }
    VecFloat GetMass() const { return mass; }
    Vector3 GetLinearVelocity() const { return velocity; }
    void SetLinearVelocity(Vector3 value) { velocity = value; }
    Vector3 GetLocalVelocity(Vector3, rw::physics::InputSpace) const { return velocity; }
    void SetJustBounced(Vector3 point, bool car, bool good, EntityId other)
    { ++reports; reportedPoint=point; reportedCar=car; reportedGood=good; reportedOther=other; }
};
namespace Vehicle { using RaceCarPhysics = ShapeVehicle; }
struct ShapeObject;
struct StoredImpulseContact
{
    Vector3 mPointOnA{13,17,19,23}, mPointOnB{29,31,37,41}, mNormal{1,0,0,0};
    ShapeObject* mpOtherVehicle = nullptr;
    int* mpOtherSensor = nullptr;
    float mfImpactTimeInFrame = .25f;
    unsigned mContactId = 73;
    void GetInverse(StoredImpulseContact&) const;
};
using Contact = StoredImpulseContact;
#include "car_car_inverse.inc"
using BrnPhysics::Deformation::ImpulseParams;
using BrnPhysics::Deformation::ImpulsePasser;

#include "car_car_constants.inc"
struct ShapeObject
{
    ShapeVehicle vehicle;
    EntityId id{};
    bool bounced = false, parity = false;
    ImpulsePasser mImpulsePasser{};
    ImpulseParams receivedParams{};
    Contact receivedContact;
    Vector3 receivedDirection{}, receivedRelative{};
    bool receivedSpy = true, receivedFriction = false;
    void ApplySensorImpulse(VecFloat, const Contact& contact, const ImpulseParams& params,
        Vector3 relative, Vector3 direction, VecFloat, int*, bool spy, bool friction)
    {
        receivedParams=params; receivedContact=contact;
        receivedDirection=direction; receivedRelative=relative;
        receivedSpy=spy; receivedFriction=friction;
    }
    void Reverse(ShapeObject& lOtherCar, const Contact& lContact, int* lpSensor)
    {
        ImpulseParams lParams{};
        lParams.mImpulsePosition=lContact.mPointOnA; // state left by the first apply
        VecFloat lvfInvInertiaB{2,2,2,2}, lvfTimeStep{.02f,.02f,.02f,.02f}, lvfShapedMagnitude{100,100,100,100};
        Vector3 lRelativeMotion{3,4,5,0}, lImpulseUnit{1,0,0,0};
#include "car_car_reverse.inc"
    }
    ShapeVehicle* AsRaceCarPhysics() { return &vehicle; }
    EntityId GetGlobalEntityId() const { return id; }
    u8 GetGameModeByte() const { return static_cast<u8>(id.muValue >> 24); }
    bool HasBouncedThisFrame() const { return bounced; }
    void SetHasBouncedThisFrame(bool value) { bounced = value; }
    void SetBounceRandomParity(bool value) { parity = value; }
    Vector3 Shape(ShapeObject& lOtherCar, Vector3 lImpulse, VecFloat lvfIteration,
                  const Contact& lContact, const CgsNumeric::Random& lRandom)
    {
        ShapeVehicle& lThisBody = vehicle;
        ShapeVehicle& lOtherBody = lOtherCar.vehicle;
#include "car_car_shaping.inc"
        return lImpulse;
    }
};

int main()
{
    unsigned checks=0, failures=0;
    auto Check = [&](bool pass, const char* name) {
        ++checks;
        if (!pass) { ++failures; if (failures < 18) std::fprintf(stderr,"FAIL: %s\n",name); }
    };
    auto Vector = [&](Vector3 actual, Vector3 expected, const char* name) {
        const auto Close=[](float a,float b){return std::fabs(a-b)<=.00001f*(1+std::fabs(b));};
        Check(Close(actual.x,expected.x)&&Close(actual.y,expected.y)&&Close(actual.z,expected.z)&&Close(actual.w,expected.w),name);
    };
    Contact contact;
    constexpr u64 seed = 0x1234567800000001ull;
    for (float playerMass : {800.f,1500.f,2800.f})
    for (float trafficMass : {500.f,1125.f,1500.f,2250.f,3500.f})
    for (bool boosting : {false,true})
    for (Vector3 impulse : {Vector3{100,-200,300,40},Vector3{-400,9000,-600,70}})
    {
        ShapeObject player, traffic;
        player.id.muValue=0x01000400; traffic.id.muValue=0x02000800;
        player.vehicle.showtime=true; player.vehicle.boosting=boosting;
        player.vehicle.mass={playerMass,playerMass,playerMass,playerMass};
        traffic.vehicle.mass={trafficMass,trafficMass,trafficMass,trafficMass};
        CgsNumeric::Random random{}; random.muSeed=seed;
        const Vector3 output=player.Shape(traffic,impulse,{0,0,0,0},contact,random);
        const float floor=playerMass*(boosting?2.79999995f:1.5f);
        const float massScale=trafficMass<1125? .75f : trafficMass>2250? 1.5f : trafficMass*(1.f/1500.f);
        Vector(output,{impulse.x*.1f,(impulse.y>floor?impulse.y:floor)*massScale,impulse.z*.1f,impulse.w*.1f},"mass-shaped impulse");
        Vector(player.vehicle.velocity,{3,0,7,9},"downward velocity removed, other lanes preserved");
        Vector(player.vehicle.reportedPoint,contact.mPointOnA,"reported contact point");
        Check(player.vehicle.reportedOther.muValue==traffic.id.muValue,"reported traffic identity");
        Check(player.vehicle.reports==1 && player.vehicle.reportedCar &&
            player.vehicle.reportedGood==(impulse.x*impulse.x+impulse.y*impulse.y+impulse.z*impulse.z>=2000000),"bounce report and original stress");
        Check(random.muSeed==seed*CgsNumeric::KU_RANDOM_MULTIPLIER+1 &&
            traffic.parity==((u32(seed>>32)%3)==0),"one original RNG draw and traffic parity");
    }
    // Normal race-car hits retain iteration scaling; traffic's secondary bounce gets x30.
    for (bool showtime : {false,true}) for (unsigned owner : {1u,2u})
    for (bool otherShowtime : {false,true}) for (bool otherBounced : {false,true})
    for (float iteration : {0.f,1.f,2.f})
    {
        ShapeObject a,b;
        a.vehicle.showtime=showtime; b.vehicle.showtime=otherShowtime; b.bounced=otherBounced;
        a.id.muValue=owner<<24; b.id.muValue=1u<<24; // no traffic target for primary bounce
        CgsNumeric::Random random{}; random.muSeed=seed;
        Vector3 impulse{100,-200,300,40};
        const bool secondary=!showtime && owner==2 && (otherShowtime||otherBounced);
        const float factor=showtime?1.f:(iteration+1)*.5f*(secondary?30.f:1.f);
        Vector(a.Shape(b,impulse,{iteration,iteration,iteration,iteration},contact,random),
            {100*factor,-200*factor,300*factor,40*factor},"ordinary and secondary bounce gates");
        Check(a.bounced==secondary && a.vehicle.reports==0 && random.muSeed==seed,"gated side effects");
        Vector(a.vehicle.velocity,{3,-5,7,9},"gated velocity unchanged");
    }
    ShapeObject attacker, victim;
    int sensorA=1, sensorB=2;
    contact.mpOtherVehicle=&victim; contact.mpOtherSensor=&sensorB;
    attacker.Reverse(victim,contact,&sensorA);
    Vector(victim.receivedParams.mImpulsePosition,contact.mPointOnB,"victim uses its own contact point");
    Vector(victim.receivedContact.mPointOnA,contact.mPointOnB,"reverse contact point A");
    Vector(victim.receivedContact.mPointOnB,contact.mPointOnA,"reverse contact point B");
    Check(victim.receivedContact.mpOtherVehicle==&attacker && victim.receivedContact.mpOtherSensor==&sensorA,"reverse contact ownership");
    Check(victim.receivedParams.mpImpulsePasser==&victim.mImpulsePasser && victim.receivedParams.mvfInverseInertia.x==2,"victim inertia and chain");
    Vector(victim.receivedDirection,{-1,0,0,0},"reversed impulse direction");
    Vector(victim.receivedRelative,{-3,-4,-5,0},"reversed relative motion");
    Check(!victim.receivedSpy && victim.receivedFriction,"reversed apply flags");
    std::printf("CarCarBounce: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
