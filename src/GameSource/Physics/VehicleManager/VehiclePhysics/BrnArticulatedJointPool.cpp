#include "types.hpp"

namespace BrnPhysics::Vehicle
{
class ArticulatedJoint
{
public:
    int Construct();

private:
    u8 mPad0[80];
};

class ArticulatedJointPool
{
public:
    int Construct();

private:
    ArticulatedJoint maJoints[10];
    u64              mu64ActiveMask;
    u64              mu64AllocatedMask;
    float            mfMaxAngle;
    float            mfMinAngle;
    float            mfDamping;
    float            mfStiffness;
};

int ArticulatedJointPool::Construct()
{
    mu64ActiveMask = 0;
    mu64AllocatedMask = 0;

    int liResult = 0;
    for (ArticulatedJoint& lJoint : maJoints)
        liResult = lJoint.Construct();

    mfMaxAngle = 30.0f;
    mfMinAngle = 10.0f;
    mfStiffness = 15.0f;
    mfDamping = 10.0f;
    return liResult;
}
}
