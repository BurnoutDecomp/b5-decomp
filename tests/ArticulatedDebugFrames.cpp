// Grouped production-body regression for matrix/joint frames, control overlays,
// physical-traffic debug geometry, and the articulated-event drain. The runner
// extracts the bodies unchanged. Renderer/assert sinks and the explicitly named
// engine collaborators below are doubles; native rendering remains separate.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerDebugComponent.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerIO.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/vpu/matrix44_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include "SDKs/XboxMath/XMVectorCos.h"
#include "SDKs/XboxMath/XMVectorTan.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(float lfA, double ldB, double ldTolerance = 2.0e-6)
{
    return std::isfinite(lfA) && std::fabs(static_cast<double>(lfA) - ldB) <= ldTolerance;
}
static bool Near3(Vector3 lvA, double ldX, double ldY, double ldZ, double ldTolerance = 2.0e-6)
{
    return Near(lvA.x, ldX, ldTolerance) && Near(lvA.y, ldY, ldTolerance) && Near(lvA.z, ldZ, ldTolerance);
}
static std::array<float, 16> Elements(const Matrix44& lrM)
{
    return {lrM.xAxis.x, lrM.xAxis.y, lrM.xAxis.z, lrM.xAxis.w,
            lrM.yAxis.x, lrM.yAxis.y, lrM.yAxis.z, lrM.yAxis.w,
            lrM.zAxis.x, lrM.zAxis.y, lrM.zAxis.z, lrM.zAxis.w,
            lrM.wAxis.x, lrM.wAxis.y, lrM.wAxis.z, lrM.wAxis.w};
}

using OracleRegister = std::array<unsigned int,4>;
static float OracleFloat(unsigned int bits) { float f;std::memcpy(&f,&bits,4);return f; }
static unsigned int OracleBits(float f) { unsigned int bits;std::memcpy(&bits,&f,4);return bits; }
static float OracleFlush(float f) { return std::fpclassify(f)==FP_SUBNORMAL ? std::copysign(0.0f,f) : f; }
static float OracleVrefp(float f) { return OracleFlush(static_cast<float>(1.0/static_cast<double>(OracleFlush(f)))); }
static float OracleVrsqrtefp(float f) {return OracleFlush(static_cast<float>(1.0/std::sqrt(static_cast<double>(OracleFlush(f)))));}
static float OracleFma(float a,float b,float c) { return OracleFlush(std::fmaf(OracleFlush(a),OracleFlush(b),OracleFlush(c))); }
static float OracleNmsub(float a,float b,float c)
{
    const float f=OracleFlush(std::fmaf(OracleFlush(a),OracleFlush(b),-OracleFlush(c)));
    return std::isnan(f)?f:-f;
}
static float OracleDot4(const OracleRegister& a,const OracleRegister& b)
{
    double sum=static_cast<double>(OracleFlush(OracleFloat(a[0])))*OracleFlush(OracleFloat(b[0]));
    for(unsigned i=1;i<4;++i)sum+=static_cast<double>(OracleFlush(OracleFloat(a[i])))*OracleFlush(OracleFloat(b[i]));
    return std::isfinite(sum) && std::fabs(sum)>(std::numeric_limits<float>::max)()
        ? std::numeric_limits<float>::quiet_NaN() : OracleFlush(static_cast<float>(sum));
}
static OracleRegister OraclePermute(const OracleRegister& a,const OracleRegister& b,const OracleRegister& control)
{
    unsigned char bytes[32];
    for(unsigned i=0;i<8;++i)for(unsigned k=0;k<4;++k)
        bytes[4*i+k]=static_cast<unsigned char>((i<4?a[i]:b[i-4])>>(24-8*k));
    OracleRegister result{};
    for(unsigned i=0;i<4;++i)for(unsigned k=0;k<4;++k)
        result[i]=(result[i]<<8)|bytes[(control[i]>>(24-8*k))&31];
    return result;
}
static std::array<float,16> InterpretArtistInverse(const Matrix44& matrix,OracleRegister& det)
{
    const auto input=Elements(matrix);
    std::array<float,16> output{};
    std::array<OracleRegister,32> v{};
#include "artist_inverse_program.inc"
    return output;
}
static OracleRegister InterpretArtistQuaternion(const Matrix33& matrix)
{
    const std::array<float,12> input{matrix.xAxis.x,matrix.xAxis.y,matrix.xAxis.z,matrix.xAxis.w,
                                    matrix.yAxis.x,matrix.yAxis.y,matrix.yAxis.z,matrix.yAxis.w,
                                    matrix.zAxis.x,matrix.zAxis.y,matrix.zAxis.z,matrix.zAxis.w};
    OracleRegister output{};
    std::array<OracleRegister,32> v{};
#include "artist_quaternion_program.inc"
    return output;
}

struct DrawCommand
{
    unsigned kind, colour;
    Vector3 a, b, c, d;
};
static std::array<DrawCommand, 32> gDraws;
static unsigned gDrawCount = 0;

namespace CgsDev
{
namespace Assert
{
    int BeginAssert() { return 0; }
    int FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
    // Unrelated menu/vtable collaborators are not exercised by this fixture.
    DebugComponent::DebugComponent() : mbActive(false), mpDebugLinkedListNext(nullptr) {}
    void DebugComponent::Update() {}
    void DebugComponent::RenderWorld(Debug3DImmediateRender*) {}
    void DebugComponent::RenderHUD(Debug2DImmediateRender*) {}
    const char* DebugComponent::GetName() const { return "fixture"; }
    const char* DebugComponent::GetPath() const { return "fixture"; }
    bool DebugComponent::IsSimple() const { return false; }
    void DebugComponent::OnActivate() {}
    void DebugComponent::OnRegister() {}

    void Debug3DImmediateRender::DrawLine(Vector3 lvA, Vector3 lvB, rw::RGBA lColour)
    {
        gDraws[gDrawCount++] = DrawCommand{0, lColour.m_rgba, lvA, lvB, {}, {}};
    }
    void Debug3DImmediateRender::DrawSolidQuad(Vector3 lvA, Vector3 lvB, Vector3 lvC,
                                              Vector3 lvD, rw::RGBA lColour)
    {
        gDraws[gDrawCount++] = DrawCommand{1, lColour.m_rgba, lvA, lvB, lvC, lvD};
    }
    void Debug3DImmediateRender::DrawTriangle(Vector3 a,Vector3 b,Vector3 c,rw::RGBA colour)
    {gDraws[gDrawCount++]={2,colour.m_rgba,a,b,c,{}};}
    void Debug3DImmediateRender::DrawQuad(Vector3 a,Vector3 b,Vector3 c,Vector3 d,rw::RGBA colour)
    {gDraws[gDrawCount++]={3,colour.m_rgba,a,b,c,d};}
    void Debug3DImmediateRender::DrawBox(Vector3 min,Vector3 max,Matrix44Affine transform,rw::RGBA colour)
    {gDraws[gDrawCount++]={4,colour.m_rgba,min,max,transform.wAxis,transform.zAxis};}
    void Debug3DImmediateRender::DrawArrow(Vector3 a,Vector3 b,rw::RGBA colour)
    {gDraws[gDrawCount++]={5,colour.m_rgba,a,b,{},{}};}
    void Debug3DImmediateRender::DrawAxis(Matrix44Affine transform)
    {gDraws[gDrawCount++]={6,0,transform.xAxis,transform.yAxis,transform.zAxis,transform.wAxis};}
}
namespace BrnPhysics { namespace Vehicle {
    const char* PhysicalTrafficManagerDebugComponent::GetName() const { return "fixture"; }
    void PhysicalTrafficManagerDebugComponent::OnActivate() {}
    void PhysicalTrafficManagerDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender*) {}
} }

#include "production_bodies.inc"

using BrnPhysics::Vehicle::ArticulatedJointPool;
using BrnPhysics::Vehicle::ArticulatedJointId;
using BrnPhysics::Vehicle::PhysicalTrafficManagerDebugComponent;
using BrnPhysics::Vehicle::BrnPlayerDriverControls;

using namespace BrnPhysics::Vehicle;
// As in the existing FX traffic fixtures, reserve storage with the REAL type's
// size/alignment and initialize only the named records exercised here. No
// virtual simulation method or engine constructor is invoked by these tests.
alignas(VehiclePhysics) static unsigned char gBodyStorage[20][sizeof(VehiclePhysics)];
alignas(PhysicalTrafficManager) static unsigned char gManagerStorage[sizeof(PhysicalTrafficManager)];
static PhysicalTrafficVehicle gTraffic[20];
static VehicleDriver gDrivers[20];
static VehiclePhysics& Body(unsigned index) {return *reinterpret_cast<VehiclePhysics*>(&gBodyStorage[index]);}
static PhysicalTrafficManager& Manager() {return *reinterpret_cast<PhysicalTrafficManager*>(&gManagerStorage);}
static std::array<unsigned,4> gRecycleMasks;
static unsigned gCreates=0,gJointRequests=0;

namespace BrnPhysics { namespace Vehicle {
    // External collaborators are sinks; the debug readers and event drain
    // themselves are extracted unchanged from production.
    Matrix44Affine SimpleVehiclePhysics::GetGraphicsVehicleTransform() const {return mTransform;}
    void SimpleVehiclePhysics::SetGraphicsVehicleTransform(Matrix44Affine transform) {mTransform=transform;}
    void SimpleVehiclePhysics::GetSimpleVehicleBox(CgsGeometric::Box& box) const
    {box.mTransform=mTransform;box.mDimensionsAndFatness={mHalfExtent.x,mHalfExtent.y,mHalfExtent.z,0};}
    Matrix44Affine SimpleVehiclePhysics::GetWheelsWorldTransfrom(EVehicleDrivenWheel,bool) const {return mTransform;}
    void PhysicalTrafficVehicle::SetArticulated(const CreatePhysicalTrafficEvent&,EArticulatedVehicleType type)
    {meArticulatedVehicleType=type;}
    Vector3 PhysicalTrafficVehicle::GetArticulationPointWorldSpace() const {return mpVehicleBody->GetTransform().wAxis;}
    EntityId PhysicalTrafficManager::GetPhysicsEntityId(s32 index) const {return EntityId{0x02000000u|(static_cast<u32>(index)<<10)};}
    s32 PhysicalTrafficManager::CreateTrafficVehicle(const CreatePhysicalTrafficEvent& event,
        VehicleManagerOutputInterface*,VehicleOutputRequestInterface*,BrnPhysics::Deformation::DeformationInputInterface*,
        RaceCarPhysics*,VehicleDriver*,CgsContainers::BitArray<8u>*,const TotalPhysicalTrafficBitArray* mask)
    {
        unsigned bits=0;for(unsigned i=0;i<20;++i)if(mask->IsBitSet(i))bits|=1u<<i;
        gRecycleMasks[gCreates]=bits;
        const s32 slot=2+static_cast<s32>(gCreates++);
        mUsedTrafficVehicles.SetBit(slot);mpaTrafficVehicles[slot].mpVehicleBody=&Body(slot);
        Body(slot).mTransform=event.mInitialTransform;
        return slot;
    }
    s32 ArticulatedJointPool::CreateJoint(ArticulatedJointCreateBuffer*,CgsPhysics::RigidBodyId,CgsPhysics::RigidBodyId,
        const Matrix44Affine&,const Matrix44Affine&,Vector3)
    {return static_cast<s32>(gJointRequests++);}
} }

static void FreshManager()
{
    std::memset(&gManagerStorage,0,sizeof(gManagerStorage));
    std::memset(gBodyStorage,0,sizeof(gBodyStorage));
    std::memset(gTraffic,0,sizeof(gTraffic));std::memset(gDrivers,0,sizeof(gDrivers));
    auto& manager=Manager();manager.mpaTrafficVehicles=gTraffic;manager.mpaTrafficDrivers=gDrivers;
    manager.mUsedTrafficVehicles.UnSetAll();manager.mArticulatedJointPool.Construct();
    for(unsigned i=0;i<20;++i)
    {
        Body(i).mTransform.SetIdentity();Body(i).mHalfExtent={1,1,4,0};
        gTraffic[i].mpVehicleBody=&Body(i);gTraffic[i].meArticulatedVehicleType=PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_NONE;
        gDrivers[i].mCatchupTargetTransform.SetIdentity();
        for(auto& wheel:Body(i).maWheels)wheel.mSlipVariables.w=.5f;
    }
    gAsserts=0;gDrawCount=0;gCreates=0;gJointRequests=0;gRecycleMasks.fill(0xFFFFFFFFu);
}

// Independent double-precision Gauss-Jordan oracle. It shares neither the
// production cofactors nor the VMX instruction pipeline.
static bool ReferenceInverse(const Matrix44& lrMatrix, double (&lInverse)[4][4], double& ldDet)
{
    const auto lElements = Elements(lrMatrix);
    double lRows[4][8]{};
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c)
        { lRows[r][c] = lElements[4*r+c]; lRows[r][4+c] = r == c ? 1.0 : 0.0; }
    ldDet = 1.0;
    for (unsigned c = 0; c < 4; ++c)
    {
        unsigned pivot = c;
        for (unsigned r = c+1; r < 4; ++r)
            if (std::fabs(lRows[r][c]) > std::fabs(lRows[pivot][c])) pivot = r;
        if (lRows[pivot][c] == 0) return false;
        if (pivot != c)
        {
            for (unsigned k = 0; k < 8; ++k) { const double t=lRows[c][k];lRows[c][k]=lRows[pivot][k];lRows[pivot][k]=t; }
            ldDet = -ldDet;
        }
        const double div = lRows[c][c]; ldDet *= div;
        for (unsigned k = 0; k < 8; ++k) lRows[c][k] /= div;
        for (unsigned r = 0; r < 4; ++r) if (r != c)
        {
            const double scale = lRows[r][c];
            for (unsigned k = 0; k < 8; ++k) lRows[r][k] -= scale*lRows[c][k];
        }
    }
    for (unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) lInverse[r][c]=lRows[r][4+c];
    return true;
}

static void TestInverse()
{
    Matrix44 lIdentity; lIdentity.SetIdentity();
    VecFloat lDet;
    const auto lIdentityInverse = Elements(rw::math::vpu::Inverse(lIdentity, lDet));
    bool lbIdentity = true;
    for(unsigned k=0;k<16;++k) lbIdentity &= lIdentityInverse[k] == ((k/4==k%4)?1.0f:0.0f);
    Check(lbIdentity, "inverse identity all16 elements");
    Check(lDet.x==1 && lDet.y==1 && lDet.z==1 && lDet.w==1, "determinant broadcast identity");

    const Matrix44 lDense{{1,2,3,4},{0,1,4,2},{5,6,0,1},{2,7,1,3}};
    double laExpected[4][4], ldExpectedDet;
    Check(ReferenceInverse(lDense,laExpected,ldExpectedDet), "independent general4x4 reference nonsingular");
    const auto laInverse=Elements(rw::math::vpu::Inverse(lDense,lDet));
    bool lbDense=true;
    for(unsigned k=0;k<16;++k) lbDense &= Near(laInverse[k],laExpected[k/4][k%4],1.0e-5);
    Check(lbDense,"full nonaffine inverse all16 elements");
    Check(Near(lDet.x,ldExpectedDet,1.0e-5) && lDet.x==lDet.y && lDet.x==lDet.z && lDet.x==lDet.w,
          "general determinant value and broadcast");

    Matrix44 lZero; lZero.SetZero();
    const auto laSingular=Elements(rw::math::vpu::Inverse(lZero,lDet));
    bool lbNaNs=true;for(float f:laSingular)lbNaNs &= std::isnan(f);
    Check(lbNaNs,"singular inverse retains unguarded Newton NaNs in all16 lanes");
    Check(lDet.x==0 && lDet.y==0 && lDet.z==0 && lDet.w==0,"singular determinant remains zero");

    Matrix44 lSubnormal=lIdentity;lSubnormal.xAxis.x=(std::numeric_limits<float>::min)()*0.5f;
    const auto laSubnormal=Elements(rw::math::vpu::Inverse(lSubnormal,lDet));
    lbNaNs=true;for(float f:laSubnormal)lbNaNs &= std::isnan(f);
    Check(lbNaNs && lDet.x==0,"VMX subnormal diagonal flushes to singular zero");
    Matrix44 lNaN=lIdentity;lNaN.yAxis.z=std::numeric_limits<float>::quiet_NaN();
    const auto laNaN=Elements(rw::math::vpu::Inverse(lNaN,lDet));
    lbNaNs=true;for(float f:laNaN)lbNaNs &= std::isnan(f);
    Check(lbNaNs && std::isnan(lDet.x),"NaN source propagates without identity fallback");

    // A direct register interpreter generated from the SAVED ARTIST stream is
    // also an exact-word oracle under the declared arithmetic/estimate model.
    // These nonquantized inputs distinguish fused minors and determinant order.
    unsigned int seed=0x210825B2u;
    for(unsigned test=0;test<12;++test)
    {
        std::array<float,16> values;
        for(unsigned i=0;i<16;++i)
        {
            seed=1664525u*seed+1013904223u;
            values[i]=OracleFloat((seed&0x807FFFFFu)|0x3F800000u);
            if(i/4==i%4)values[i]+=6.0f;
        }
        const Matrix44 matrix{{values[0],values[1],values[2],values[3]},
                              {values[4],values[5],values[6],values[7]},
                              {values[8],values[9],values[10],values[11]},
                              {values[12],values[13],values[14],values[15]}};
        OracleRegister expectedDet;
        const auto expected=InterpretArtistInverse(matrix,expectedDet);
        const auto actual=Elements(rw::math::vpu::Inverse(matrix,lDet));
        bool same=true;for(unsigned i=0;i<16;++i)same &= OracleBits(actual[i])==OracleBits(expected[i]);
        Check(same,"inverse words equal independent ARTIST register interpreter");
        Check(OracleBits(lDet.x)==expectedDet[0] && OracleBits(lDet.y)==expectedDet[1]
              && OracleBits(lDet.z)==expectedDet[2] && OracleBits(lDet.w)==expectedDet[3],"determinant words equal ARTIST vmsum4 interpreter");
    }
}

static bool QuaternionMatches(rw::math::vpu::Quaternion actual,const OracleRegister& expected)
{
    const OracleRegister words{OracleBits(actual.x),OracleBits(actual.y),OracleBits(actual.z),OracleBits(actual.w)};
    bool same=true;
    // Finite/Inf/zero words are exact. NaN payloads are outside the estimate
    // model; compare NaN classification without silently accepting finite data.
    for(unsigned i=0;i<4;++i)
        same &= words[i]==expected[i] || (std::isnan(OracleFloat(words[i])) && std::isnan(OracleFloat(expected[i])));
    return same;
}
static void CheckQuaternionOracle(const Matrix33& matrix,const char* label)
{
    const auto expected=InterpretArtistQuaternion(matrix);
    rw::physics::JointFrames frames;
    frames.SetChildAngularFrame(matrix);frames.SetParentAngularFrame(matrix);frames.SetParentLinearFrame(matrix);
    Check(QuaternionMatches(rw::math::vpu::QuaternionFromMatrix33(matrix),expected)
          && QuaternionMatches(frames.GetChildAngularFrame(),expected)
          && QuaternionMatches(frames.GetParentAngularFrame(),expected)
          && QuaternionMatches(frames.GetParentLinearFrame(),expected),label);
}
static Matrix33 RotationFromQuaternion(double x,double y,double z,double w)
{
    return Matrix33{{static_cast<float>(1-2*y*y-2*z*z),static_cast<float>(2*(x*y+z*w)),static_cast<float>(2*(x*z-y*w)),0},
                    {static_cast<float>(2*(x*y-z*w)),static_cast<float>(1-2*x*x-2*z*z),static_cast<float>(2*(y*z+x*w)),0},
                    {static_cast<float>(2*(x*z+y*w)),static_cast<float>(2*(y*z-x*w)),static_cast<float>(1-2*x*x-2*y*y),0}};
}
static void TestQuaternionConversion()
{
    Matrix33 identity;identity.SetIdentity();
    CheckQuaternionOracle(identity,"quaternion identity and all three frame setters match original instructions");

    const double rotations[][4]={{1,2,3,4},{-.5,2,-3,.01},{3,-2,.5,-.02},{1,1,0,0},
        {1,0,1,0},{0,1,1,0},{1,1,1,0},{1,1,1,1},{1,0,0,0},{0,1,0,0},{0,0,1,0},
        {.0001,1,-.0002,.0003},{4,-1,2,-7},{-9,-7,3,.4},{.25,.75,-.5,1.5},{-2,5,7,-3}};
    for(unsigned i=0;i<sizeof(rotations)/sizeof(rotations[0]);++i)
    {
        double x=rotations[i][0],y=rotations[i][1],z=rotations[i][2],w=rotations[i][3];
        const double norm=std::sqrt(x*x+y*y+z*z+w*w);x/=norm;y/=norm;z/=norm;w/=norm;
        const Matrix33 matrix=RotationFromQuaternion(x,y,z,w);
        char label[100];std::snprintf(label,sizeof(label),"general rotation%u exact words equal ARTIST quaternion register oracle",i);
        CheckQuaternionOracle(matrix,label);
        const auto q=rw::math::vpu::QuaternionFromMatrix33(matrix);
        const double dot=q.x*x+q.y*y+q.z*z+q.w*w;
        const double sign=dot<0?-1:1;
        std::snprintf(label,sizeof(label),"general rotation%u independently recovers authored quaternion up to sign",i);
        Check(Near(q.x,sign*x,3e-6)&&Near(q.y,sign*y,3e-6)&&Near(q.z,sign*z,3e-6)&&Near(q.w,sign*w,3e-6),label);
    }

    // Nonrotation matrices make the strict tie choices observable without
    // relying only on equal mathematical quaternions from proper rotations.
    Matrix33 tied{{0,.3f,.2f,0},{.7f,0,-.6f,0},{-.4f,.8f,-1,0}};
    CheckQuaternionOracle(tied,"X equals Y tie selects Y candidate with original off-diagonal gather");
    auto q=rw::math::vpu::QuaternionFromMatrix33(tied);
    Check(Near(q.y,std::sqrt(.5)) && q.w<0,"strict X=Y tie keeps Y-largest positive and signed W");
    tied.yAxis.y=-1;tied.zAxis.z=0;
    CheckQuaternionOracle(tied,"X equals Z tie selects Z candidate");
    q=rw::math::vpu::QuaternionFromMatrix33(tied);
    Check(Near(q.z,std::sqrt(.5)) && q.w<0,"strict X=Z tie keeps Z-largest positive and signed W");
    tied.xAxis.x=-1;tied.yAxis.y=0;
    CheckQuaternionOracle(tied,"Y equals Z tie selects Z candidate");
    tied.xAxis.x=tied.yAxis.y=tied.zAxis.z=0;
    CheckQuaternionOracle(tied,"all diagonal ties and zero trace preserve final Z selection");
    Matrix33 roundOrder{{1,.25f,-.5f,0},{.75f,1,.125f,0},{-.25f,.5f,16777216.0f,0}};
    CheckQuaternionOracle(roundOrder,"radicand retains signedXY plus signedZOne addition order at large finite scale");

    Matrix33 zero;zero.SetZero();
    CheckQuaternionOracle(zero,"rank-zero matrix retains original finite Z candidate");
    q=rw::math::vpu::QuaternionFromMatrix33(zero);
    Check(q.x==0&&q.y==0&&q.z==.5f&&q.w==0,"rank-zero matrix is not normalized or replaced by identity");
    Matrix33 reflection=identity;reflection.xAxis.x=-1;
    CheckQuaternionOracle(reflection,"reflection retains original nonunit quaternion arithmetic");
    const float inf=std::numeric_limits<float>::infinity(),nan=std::numeric_limits<float>::quiet_NaN();
    Matrix33 exceptional=identity;exceptional.xAxis.x=inf;
    CheckQuaternionOracle(exceptional,"positive infinite diagonal retains unconditional Inf times zero Newton NaNs");
    q=rw::math::vpu::QuaternionFromMatrix33(exceptional);
    Check(std::isnan(q.x)&&std::isnan(q.y)&&std::isnan(q.z)&&std::isnan(q.w)
          && !rw::math::vpu::IsValid(q),"reviewer Inf11 counterexample invalidates all quaternion lanes and existing validity predicate");
    exceptional.xAxis.x=-inf;
    CheckQuaternionOracle(exceptional,"negative infinite diagonal retains unordered candidate behavior");
    exceptional.xAxis.x=inf;exceptional.yAxis.y=-inf;
    CheckQuaternionOracle(exceptional,"mixed infinite diagonal retains NaN radicands and original selection");
    exceptional=identity;exceptional.yAxis.y=nan;
    CheckQuaternionOracle(exceptional,"NaN diagonal does not acquire a quaternion fallback");
    exceptional=identity;exceptional.yAxis.z=nan;
    CheckQuaternionOracle(exceptional,"NaN off-diagonal preserves per-lane propagation through selected candidate");
    exceptional=identity;exceptional.zAxis.x=inf;
    CheckQuaternionOracle(exceptional,"infinite off-diagonal preserves selected infinite quaternion lane");
    exceptional=identity;exceptional.xAxis.w=nan;exceptional.yAxis.w=inf;exceptional.zAxis.w=-inf;
    CheckQuaternionOracle(exceptional,"unused fourth matrix lanes do not enter original gathers");
    q=rw::math::vpu::QuaternionFromMatrix33(exceptional);
    Check(q.x==0&&q.y==0&&q.z==0&&q.w==1,"unused exceptional W lanes leave identity quaternion exact");
    exceptional=zero;exceptional.xAxis.y=-0.0f;exceptional.zAxis.x=-0.0f;
    CheckQuaternionOracle(exceptional,"signed zero off-diagonals retain original subtraction and multiplication signs");
    exceptional=zero;const float sub=(std::numeric_limits<float>::min)()*.5f;
    exceptional.xAxis.x=sub;exceptional.yAxis.y=-sub;exceptional.zAxis.y=sub;exceptional.xAxis.z=-sub;
    CheckQuaternionOracle(exceptional,"VMX denormal operands flush before radicand comparisons and off-diagonal arithmetic");

    unsigned seed=0x8259B580u;
    for(unsigned test=0;test<12;++test)
    {
        std::array<float,9> a;
        for(auto& value:a){seed=1664525u*seed+1013904223u;value=OracleFloat((seed&0x807FFFFFu)|0x3F000000u);}
        const Matrix33 matrix{{a[0],a[1],a[2],0},{a[3],a[4],a[5],0},{a[6],a[7],a[8],0}};
        CheckQuaternionOracle(matrix,"dense finite nonrotation retains exact original candidate/FMA/select words");
    }
}

static void TestJoint()
{
    ArticulatedJointPool lPool;lPool.Construct();
    Check(lPool.mfSwingAngleDegrees==30 && lPool.mfMaxSwingVelocityDegrees==10
          && lPool.mfTwistAngleDegrees==15 && lPool.mfMaxTwistVelocityDegrees==10,"original pool defaults");
    Matrix44Affine lCab;lCab.SetIdentity();lCab.wAxis={10,20,30,0};
    Matrix44Affine lTrailer;lTrailer.SetIdentity();
    lTrailer.xAxis={0,0,-1,0};lTrailer.zAxis={1,0,0,0};lTrailer.wAxis={-4,5,9,0};
    ArticulatedJointId lId;lId.mu64RawId=0x0200140000090003ull;
    const CgsPhysics::RigidBodyId lCabId(0x0200140012345678ull), lTrailerId(0x020024009ABCDEF0ull);
    CgsPhysics::PhysicsSimulationIO::InAddJoint lEvent;
    lPool.ConstructArticulatedJoint(&lEvent,lId,lCabId,lTrailerId,lCab,lTrailer,{12,23,35,0});
    Check(lEvent.mu64Id==lId.mu64RawId && lEvent.mu64ParentBodyId==static_cast<u64>(lCabId)
          && lEvent.mu64ChildBodyId==static_cast<u64>(lTrailerId),"all three64bit joint ids retain order and width");
    Check(lEvent.mbSpy,"original spy flag enabled");
    const auto& lim=lEvent.mJointLimits;
    Check(lim.GetSwingType()==rw::physics::SWING_CONE && lim.GetTwistType()==rw::physics::TWIST_ARC,"swing cone and twist arc");
    Check(Near(lim.GetSwingAngle(),0.5235987901687622) && Near(lim.GetTwistAngle(),0.2617993950843811),"degrees become original radians");
    Check(Near(lim.GetSwingLimit(),0.8660254037844386) && Near(lim.GetTwistLimit(),0.9659258262890683),"original cosine limit stores");
    const Vector3 angular=lim.GetAngularVelocityLimit();
    Check(Near3(angular,0.1745329201221466,0.1745329201221466,0.1745329201221466),"twist and swing velocity radians");
    Check(Near3(lim.GetPositionLimit(),.1,.1,.1) && Near3(lim.GetLinearVelocityLimit(),.1,.1,.1),"original linear position and velocity limits");
    Check(Near3(lEvent.mJointFrames.GetParentPosition(),2,3,5),"parent hinge transformed into cab space");
    Check(Near3(lEvent.mJointFrames.GetChildPosition(),-26,18,16),"child hinge transformed into rotated trailer space");
    const auto child=lEvent.mJointFrames.GetChildAngularFrame();
    Check(Near(child.x,0) && Near(child.y,0) && Near(child.z,0) && Near(child.w,1),"child angular identity for aligned trailer");
    const auto parent=lEvent.mJointFrames.GetParentAngularFrame();
    Check(Near(parent.x,0) && Near(parent.y,std::sqrt(.5)) && Near(parent.z,0) && Near(parent.w,std::sqrt(.5)),"parent angular frame from negative cab forward basis");
    const auto linear=lEvent.mJointFrames.GetParentLinearFrame();
    Check(Near(linear.x,0) && Near(linear.y,0) && Near(linear.z,0) && Near(linear.w,1),"parent linear frame is cab basis");
    const auto stored=lPool.maJoints[3].GetParentToJointTransform();
    Check(Near3(stored.xAxis,0,0,-1) && Near3(stored.yAxis,0,1,0) && Near3(stored.zAxis,1,0,0)
          && Near3(stored.wAxis,2,3,5),"selected pool slot retains full parentToJoint matrix");
    Check(lPool.maJoints[3].mJointId.mu64RawId==lId.mu64RawId,"selected pool slot retains packed joint id");

    lPool.mfTwistAngleDegrees=0;lPool.mfSwingAngleDegrees=65;lPool.mfMaxSwingVelocityDegrees=17;lPool.mfMaxTwistVelocityDegrees=29;
    lPool.ConstructArticulatedJoint(&lEvent,lId,lCabId,lTrailerId,lCab,lTrailer,{12,23,35,0});
    Check(lEvent.mJointLimits.GetTwistType()==rw::physics::TWIST_LOCKED
          && lEvent.mJointLimits.GetTwistAngle()==0 && lEvent.mJointLimits.GetTwistLimit()==1,"zero twist chooses locked with cosine1");
    const Vector3 tuned=lEvent.mJointLimits.GetAngularVelocityLimit();
    Check(Near(tuned.x,29*0.01745329238474369) && Near(tuned.y,17*0.01745329238474369)
          && Near(tuned.z,17*0.01745329238474369),"independent debug swing and twist velocity cells reach limits");
    Check(Near(lEvent.mJointLimits.GetSwingAngle(),65*0.01745329238474369)
          && Near(lEvent.mJointLimits.GetSwingLimit(),std::cos(65*0.01745329238474369)),"debug swing angle reaches radians and cosine");
    Check(gAsserts==0,"valid joint construction raises no assertion");
}

static void TestControls()
{
    PhysicalTrafficManagerDebugComponent lComponent;
    CgsDev::Debug3DImmediateRender lDisplay{};
    lDisplay.mfVirtualScreenWidth=1280;lDisplay.mfVirtualScreenHeight=720;
    lDisplay.mViewProjectionMatrix=Matrix44{{2,0,0,0},{0,4,0,0},{0,0,1,0},{0,0,0,1}};
    BrnPlayerDriverControls lControls{};lControls.mfSteering=.25f;lControls.mfBrake=.5f;lControls.mfGas=1.25f;
    gDrawCount=0;lComponent.RenderControls({1,-.5f,2,0},&lControls,&lDisplay);
    Check(gDrawCount==6,"driver overlay emits exactly four lines and two quads");
    Check(gDraws[0].kind==0 && gDraws[1].kind==0 && gDraws[2].kind==0 && gDraws[3].kind==1
          && gDraws[4].kind==0 && gDraws[5].kind==1,"original draw call ordering");
    Check(gDraws[0].colour==0xFF008CB4 && gDraws[1].colour==0xFF008CFF && gDraws[2].colour==0xFF008CB4
          && gDraws[3].colour==0xE63C3CB4 && gDraws[4].colour==0xFF008CB4 && gDraws[5].colour==0xE6B43C3C,"original packed draw colours");
    Check(Near3(gDraws[0].a,.4375,2,2) && Near3(gDraws[0].b,1.5625,2,2),"steering baseline uses full inverse scale and heightWidth ratio");
    Check(Near3(gDraws[1].a,1.140625,2.05,2) && Near3(gDraws[1].b,1.140625,1.95,2),"steering value positions original perpendicular marker");
    Check(Near3(gDraws[2].a,.775,2.1,2) && Near3(gDraws[2].b,.775,2.475,2),"brake baseline offset and height");
    Check(Near3(gDraws[3].a,.690625,2.1,2) && Near3(gDraws[3].b,.859375,2.1,2)
          && Near3(gDraws[3].c,.859375,2.2875,2) && Near3(gDraws[3].d,.690625,2.2875,2),"brake quad winding and proportional fill");
    Check(Near3(gDraws[4].a,1.225,2.1,2) && Near3(gDraws[4].b,1.225,2.475,2),"gas baseline offset and height");
    Check(Near3(gDraws[5].a,1.140625,2.1,2) && Near3(gDraws[5].b,1.309375,2.1,2)
          && Near3(gDraws[5].c,1.309375,2.56875,2) && Near3(gDraws[5].d,1.140625,2.56875,2),"gas fill uses raw control without added clamp");

    gDrawCount=0;lComponent.RenderControls({1,0,.000999f,0},&lControls,&lDisplay);
    Check(gDrawCount==0,"projected Z below0.001 clips all geometry");
    gDrawCount=0;lComponent.RenderControls({1,0,.001f,0},&lControls,&lDisplay);
    Check(gDrawCount==6,"projected Z exactly0.001 remains visible");
    gDrawCount=0;lComponent.RenderControls({1,0,std::numeric_limits<float>::quiet_NaN(),0},&lControls,&lDisplay);
    Check(gDrawCount==6,"unordered clipZ preserves original compare branch");
    gDrawCount=0;const unsigned asserts=gAsserts;
    lComponent.RenderControls({1,0,-2,0},nullptr,&lDisplay);
    Check(gDrawCount==0 && gAsserts==asserts+1,"null controls assertion remains nongating before hidden-point return");

    lDisplay.mViewProjectionMatrix=Matrix44{{1,2,3,4},{0,1,4,2},{5,6,0,1},{2,7,1,3}};
    double ref[4][4],det;ReferenceInverse(lDisplay.mViewProjectionMatrix,ref,det);
    gDrawCount=0;lComponent.RenderControls({1,-.5f,2,0},&lControls,&lDisplay);
    const double ratio=720.0/1280.0;
    Check(gDrawCount==6 && Near3(gDraws[0].a,1-2*ref[0][0]*ratio,2-2*ref[0][1]*ratio,2-2*ref[0][2]*ratio,2.0e-5)
          && Near3(gDraws[0].b,1+2*ref[0][0]*ratio,2+2*ref[0][1]*ratio,2+2*ref[0][2]*ratio,2.0e-5),"general projective billboard uses true4x4 inverse rows");
}

static void TestAirRamAndCone()
{
    FreshManager();
    PhysicalTrafficManagerDebugComponent component;
    CgsDev::Debug3DImmediateRender display{};
    component.mfAirRamLengthScale=5;
    Body(0).mTransform.wAxis={10,20,30,0};
    VehiclePhysics::AirRamEffect ram{};ram.mImpulse={0,0,16,0};ram.mPosition={1,2,3,0};
    ram.meImpulseSpace=rw::physics::WORLD_SPACE;
    component.RenderAirRam(&ram,&Body(0),&display);
    Check(gDrawCount==5,"AirRam reaches four production cone triangles and one quad");
    const double radius=20.0*std::tan(8.0*0.01745329238474369);
    Check(gDraws[4].kind==3 && Near3(gDraws[4].a,11,22+radius,13,2.0e-5)
          && Near3(gDraws[4].b,11+radius,22,13,2.0e-5)
          && Near3(gDraws[4].c,11,22-radius,13,2.0e-5)
          && Near3(gDraws[4].d,11-radius,22,13,2.0e-5),
          "impulse16 scale5 gives length20 negativeZ and eight degree aperture");
    Check(Near3(gDraws[0].a,11,22,33) && Near3(gDraws[0].b,11+radius,22,13,2.0e-5)
          && Near3(gDraws[0].c,11,22+radius,13,2.0e-5),"original cone triangle winding and application point");
    Check(gDraws[0].colour==0x96FFB4B4 && gDraws[4].colour==0x96FFB4B4,"original AirRam packed colour");

    gDrawCount=0;ram.mImpulse={0,0,.0019f,0};component.RenderAirRam(&ram,&Body(0),&display);
    Check(gDrawCount==0,"AirRam below0.01OverScale magnitude threshold is suppressed");
    gDrawCount=0;ram.mImpulse={0,0,.0021f,0};component.RenderAirRam(&ram,&Body(0),&display);
    Check(gDrawCount==5,"AirRam above original magnitude threshold remains visible");
    gDrawCount=0;ram.mImpulse={0,0,0,0};component.RenderAirRam(&ram,&Body(0),&display);
    Check(gDrawCount==0,"positive scale suppresses zero impulse at original gate");
    gDrawCount=0;ram.mImpulse={0,0,16,0};component.mfAirRamLengthScale=0;
    component.RenderAirRam(&ram,&Body(0),&display);
    Check(gDrawCount==0,"zero scale retains original infinite threshold");

    component.mfAirRamLengthScale=5;ram.meImpulseSpace=rw::physics::BODY_SPACE;
    Body(0).mTransform.xAxis={0,0,-1,0};Body(0).mTransform.zAxis={1,0,0,0};
    gDrawCount=0;component.RenderAirRam(&ram,&Body(0),&display);
    Check(gDrawCount==5 && Near3(gDraws[0].a,13,22,29)
          && Near(gDraws[4].a.x,-7),"body-space impulse and application point use original transformed frame");

    gDrawCount=0;
    component.RenderCone({0,0,0,0},{0,2,0,0},{2,2,2,2},0xFFFFFFFFu,
                         {.13962633907794952f,.13962633907794952f,.13962633907794952f,.13962633907794952f},&display);
    const double r=2.0*std::tan(8.0*0.01745329238474369);
    Check(gDrawCount==5 && Near3(gDraws[4].a,0,4,r,2.0e-6)
          && Near3(gDraws[4].b,r,4,0,2.0e-6),"nearY nonunit cone preserves X-arm v126/v125 corner roles");
    gDrawCount=0;
    component.RenderCone({0,0,0,0},{0,0,0,0},{2,2,2,2},0xFFFFFFFFu,{.1f,.1f,.1f,.1f},&display);
    Check(gDrawCount==5 && std::isnan(gDraws[4].a.x) && std::isnan(gDraws[4].b.y),
          "zero cone basis retains unguarded normalization NaNs");
    ram.meImpulseSpace=rw::physics::WORLD_SPACE;ram.mImpulse={0,0,0,0};component.mfAirRamLengthScale=-1;
    gDrawCount=0;const unsigned before=gAsserts;
    component.RenderAirRam(&ram,&Body(0),&display);
    Check(gDrawCount==5 && std::isnan(gDraws[4].a.x) && gAsserts==before+1,
          "zero impulse negative scale preserves invalid-direction assert and NaN geometry");
}

static void TestCatchupResetAndFrontArrows()
{
    FreshManager();
    PhysicalTrafficManagerDebugComponent component;component.mpTrafficManager=&Manager();component.mbDrawJoints=false;
    CgsDev::Debug3DImmediateRender display{};
    Manager().mUsedTrafficVehicles.SetBit(0);gDrivers[0].mi8NumOfInterpSteps=1;
    auto& target=gDrivers[0].mCatchupTargetTransform;
    target.xAxis={0,0,-1,0};target.yAxis={0,1,0,0};target.zAxis={1,0,0,0};target.wAxis={100,200,300,0};
    Body(0).mSimpleAttribs.mCOMOffset={1,2,3,0};
    component.DrawCatchupTargets(&display);
    Check(gDrawCount==2 && gDraws[0].kind==4 && gDraws[1].kind==5,"catchup submits a target box and arrow");
    Check(Near3(gDraws[0].c,97,198,301) && Near3(gDraws[1].b,97,198,301),
          "nonzero rotated COM offset corrects both catchup box and arrow endpoint");
    gDrawCount=0;gDrivers[0].mi8NumOfInterpSteps=0;component.DrawCatchupTargets(&display);
    Check(gDrawCount==0,"zero interpolation steps preserve catchup early skip");

    Body(0).mTransform.SetIdentity();Body(0).mTransform.wAxis={10,20,30,0};
    Body(0).mSimpleAttribs.mCOMOffset={0,0,0,0};gDrawCount=0;
    component.DrawTrafficBox(&gTraffic[0],&display,false);
    Check(gDrawCount==2 && gDraws[1].kind==5 && Near3(gDraws[1].a,10,20,34)
          && Near3(gDraws[1].b,10,20,35.5) && gDraws[1].a.w==0 && gDraws[1].b.w==0,
          "simple traffic arrow begins at front half extent with original fourth lane");
    gDrawCount=0;component.DrawPhysicalTraffic(&display);
    const DrawCommand* arrow=nullptr;for(unsigned i=0;i<gDrawCount;++i)if(gDraws[i].kind==5)arrow=&gDraws[i];
    Check(arrow && Near3(arrow->a,10,20,34) && Near3(arrow->b,10,20,35.5),
          "physical traffic arrow begins at front half extent");

    FreshManager();component.mpTrafficManager=&Manager();
    Manager().mUsedTrafficVehicles.SetBit(3);Manager().mUsedTrafficVehicles.SetBit(7);
    Body(3).mTransform.xAxis={0,0,-1,0};Body(3).mTransform.zAxis={1,0,0,0};Body(3).mTransform.wAxis={9,8,7,1};
    Body(7).mTransform.wAxis={123,456,789,0};
    PhysicalTrafficManagerDebugComponent::ResetTrafficAboveWater(&component);
    Check(Near3(Body(3).mTransform.wAxis,3355,6,-1378.800048828125) && Body(3).mTransform.wAxis.w==0,
          "water reset preserves exact XYZ and explicitly zeroes fourth lane");
    Check(Near3(Body(3).mTransform.xAxis,0,0,-1) && Near3(Body(3).mTransform.zAxis,1,0,0)
          && Near3(Body(7).mTransform.wAxis,123,456,789),"water reset preserves orientation and affects only first used vehicle");
    Check(gAsserts==0,"valid debug geometry and reset raise no assertions");
}

static void TestTwoArticulatedRequests()
{
    FreshManager();
    CgsModule::EventQueue<CreateArticulatedTrafficEvent,10> queue{};queue.Construct();
    for(unsigned i=0;i<2;++i)
    {
        auto& event=queue.AddEvent();event.mInitialTransform_Cab.SetIdentity();event.mInitialTransform_Trailer.SetIdentity();
        event.mInitialTransform_Cab.wAxis={10.0f+100.0f*i,0,0,0};
        event.mInitialTransform_Trailer.wAxis={14.0f+100.0f*i,0,0,0};
    }
    ArticulatedJointCreateBuffer working;
    Manager().mpArticulatedJointCreateBuffer=&working;
    CgsContainers::BitArray<8u> used;used.UnSetAll();
    Manager().ProcessCreateArticulatedTrafficEvents(&queue,
        reinterpret_cast<VehicleManagerOutputInterface*>(1),reinterpret_cast<VehicleOutputRequestInterface*>(2),
        reinterpret_cast<BrnPhysics::Deformation::DeformationInputInterface*>(3),reinterpret_cast<RaceCarPhysics*>(4),gDrivers,&used);
    Check(gCreates==4 && gJointRequests==2,"two articulated requests create two cab/trailer pairs and two joints");
    Check(gRecycleMasks[0]==0 && gRecycleMasks[1]==(1u<<2)
          && gRecycleMasks[2]==0 && gRecycleMasks[3]==(1u<<4),
          "recycle mask resets per request and protects only current cab while creating its trailer");
    Check(gTraffic[2].miJointIndex==0 && gTraffic[3].miJointIndex==0
          && gTraffic[4].miJointIndex==1 && gTraffic[5].miJointIndex==1,"each pair receives its own joint index");
    Check(Near3(Body(2).mTransform.wAxis,12,0,0) && Near3(Body(3).mTransform.wAxis,12,0,0)
          && Near3(Body(4).mTransform.wAxis,112,0,0) && Near3(Body(5).mTransform.wAxis,112,0,0),
          "both requests preserve half-separation correction before joint creation");
    Check(gAsserts==0,"valid two-request drain raises no assertions");
}

int main()
{
    TestInverse();TestQuaternionConversion();TestJoint();TestControls();TestAirRamAndCone();TestCatchupResetAndFrontArrows();TestTwoArticulatedRequests();
    std::printf("ArticulatedDebugFrames: %u checks, %u failures\n",gChecks,gFailures);
    return gFailures ? 1 : 0;
}
