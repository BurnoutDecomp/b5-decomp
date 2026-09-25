// FX-FOLLOWUPS (crash parity 2026-09-25, item 2 step C): the loose octree's VOLUME walk --
//   LooseOctree::VolumeTest @0x828CA910            (slot 8; no PC declaration or body before)
//   LooseOctree::VolumeTestRecursive @0x828BDE28   (no PC body before)
//   LooseOctree::Construct @0x828C99D8's volume-walk state (0x828C9E34..0x828CA09C): the two identity frames,
//     the VolumeVolumeQuery (68862272), the unit BoxVolume and the unit SphereVolume
//   CoarseQueryResultBuffer::GetNumResultsAttempted @0x828ADCD8 (its :348 batch assert; VolumeTest inlines it)
// run_fxfollowups_octree_volume.py pastes the PRODUCTION text (fxfu_octvol.inc): the LooseOctree constructor,
// the file-local AllocFromResourceAllocator and PrimeOctreeVolumeQuery, Construct, AllocRecursive, the
// _miVolumeTestPerfMon definition, VolumeTest and VolumeTestRecursive; CgsSpatialPartition.cpp is compiled
// alongside. The rwcollision pieces are FIXTURES: VolumeVolumeQuery::GetResourceDescriptor / Initialize record
// their arguments, BoxVolume / SphereVolume::Initialize record theirs and stamp a minimal volume in the block,
// and GetPrimitiveIntersections records the query's nine primed fields, answers with an exact sphere-vs-box /
// sphere-vs-sphere overlap of the input volume against the query volume, and then POISONS the nine fields, so
// every later query proves its own priming.
//
// Every expectation is the ARTIST decode (the production banners carry the addresses):
//   * Construct: frames = identity rows with an all-zero w row; descriptor + Initialize(100, 100) into
//     macVolumeVolumeQueryBuffer; BoxVolume {1,1,1} in macBoxVolumeBuffer -> mpNodeVolume; SphereVolume 1.0 in
//     macSphereVolumeBuffer -> mpEntityVolume; in that order;
//   * VolumeTest: the block {volume, flags, buffer, transform}; the monitor around the walk from node 0; the
//     answer is GetNumResultsAttempted() > 0, whose :348 assert fires outside a batch;
//   * VolumeTestRecursive: the node box {HalfSize (mParams0.w), HalfHeight (mParams1.z), HalfSize} at the
//     node's whole mPosition vector; a miss prunes the sub-tree; NO node type-mask gate; the chain walked from
//     the head to the 0xFFFF sentinel whenever muNumElements > 0 (the count gates only the entry); a
//     type-matching link's sphere -> radius w and the whole sphere vector as the translation row; pushed on a
//     hit; then the children whose sub-tree mask meets the flags, in order.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.h"
#ifdef GetFreeSpace
#undef GetFreeSpace
#endif
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsCoarseQueryResultBuffer.h"
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManager.h"
#include "rw/rwcore_structs.h"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>
#include <string>
#include <vector>

namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; } }

// ---- the assert seam: record every message ---------------------------------------------------------------------
static std::vector<std::string> gaAsserts;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
} }
static int CountAsserts(const char* lpcText)
{
    int liCount = 0;
    for (const std::string& lrMessage : gaAsserts)
        if (lrMessage.find(lpcText) != std::string::npos)
            ++liCount;
    return liCount;
}

// ---- one ordered event log for the monitor, the queries and the rwcollision fixtures -----------------------------
enum EEvent { E_START_MONITOR, E_STOP_MONITOR, E_QUERY, E_DESCRIPTOR, E_VVQ_INITIALIZE, E_BOX_INITIALIZE,
              E_SPHERE_INITIALIZE };
struct Event { EEvent meKind; s32 miValue; };
static std::vector<Event> gaEvents;

namespace CgsDev { namespace PerfMonCpu {
void StartMonitor(s32 liMonitorHandle) { gaEvents.push_back(Event{ E_START_MONITOR, liMonitorHandle }); }
void StopMonitor(s32 liMonitorHandle)  { gaEvents.push_back(Event{ E_STOP_MONITOR, liMonitorHandle }); }
} }

// ---- the resource allocator the extracted Construct carves its node arrays from ---------------------------------
namespace EA { namespace Allocator { ICoreAllocator::~ICoreAllocator() {} } }
namespace rw
{
    void* IResourceAllocator::Alloc(size_t, const char*, uint32_t, uint32_t, uint32_t) { return nullptr; }
    void* IResourceAllocator::Alloc(size_t, const char*, uint32_t) { return nullptr; }
    void IResourceAllocator::Free(void*, size_t) {}
    ::rw::Resource IResourceAllocator::DoAllocate(const ::rw::ResourceDescriptor&, const char*) { return ::rw::Resource(); }
    void IResourceAllocator::DoFreeDisposable(::rw::Resource&) {}
}
struct TestAllocator : rw::IResourceAllocator
{
    std::vector<void*> maBlocks;
    ::rw::Resource DoAllocate(const ::rw::ResourceDescriptor& lrDescriptor, const char*) override
    {
        const u32 luSize  = lrDescriptor.m_baseResourceDescriptors[0].m_size;
        const u32 luAlign = lrDescriptor.m_baseResourceDescriptors[0].m_alignment < 16u
                          ? 16u : lrDescriptor.m_baseResourceDescriptors[0].m_alignment;
        void* lpBlock = _aligned_malloc(luSize ? luSize : 16u, luAlign);
        std::memset(lpBlock, 0, luSize ? luSize : 16u);
        maBlocks.push_back(lpBlock);
        ::rw::Resource lResource;
        lResource.m_baseResources[0] = lpBlock;
        return lResource;
    }
};

// ---- the rwcollision fixtures ----------------------------------------------------------------------------------
struct QueryRecord
{
    f32         mfPadding;
    const void* mpInputVols;
    const void* mpInputMats;
    u32         muNumInputs;
    u32         muCurrInput;
    u32         muPairCount;
    const void* mpQueryVol;
    const void* mpQueryMtx;
    const void* mpCullTable;
    const void* mpInputVolume;    // *m_inputVols
    const void* mpInputMatrix;    // *m_inputMats
    u32         muQueryType;      // the query volume's descriptor slot (4 box / 1 sphere)
    f32         mfHx, mfHy, mfHz; // box half extents
    f32         mfRadius;         // sphere radius
    Vector3     mQueryPos;        // the query transform's translation row, all four lanes
    bool        mbAnswer;
};
static std::vector<QueryRecord> gaQueries;

struct DescriptorCall { int miVolumes; int miResults; };
static std::vector<DescriptorCall> gaDescriptorCalls;
struct VvqInitCall { void* mapBuffers[5]; int miVolumes; int miResults; };
static std::vector<VvqInitCall> gaVvqInitCalls;
struct BoxInitCall { void* mpBlock; void* mapRest[4]; rw::collision::Vec4 mDims; };
static std::vector<BoxInitCall> gaBoxInitCalls;
struct SphereInitCall { void* mpBlock; void* mapRest[4]; f32 mfRadius; };
static std::vector<SphereInitCall> gaSphereInitCalls;

namespace rw { namespace collision {

void* VolumeVolumeQuery::GetResourceDescriptor(void* lpOut, int liVolumes, int liResults)
{
    gaEvents.push_back(Event{ E_DESCRIPTOR, 0 });
    gaDescriptorCalls.push_back(DescriptorCall{ liVolumes, liResults });
    u32* lpu = static_cast<u32*>(lpOut);
    for (int i = 0; i < 10; ++i) lpu[i] = 0;
    lpu[0] = KU_VOLUME_VOLUME_QUERY_HOST_SIZE_R100;
    lpu[1] = 16;
    return lpOut;
}

void* VolumeVolumeQuery::Initialize(void** lppBuffer, int liVolumes, int liResults)
{
    gaEvents.push_back(Event{ E_VVQ_INITIALIZE, 0 });
    VvqInitCall lCall;
    for (int i = 0; i < 5; ++i) lCall.mapBuffers[i] = lppBuffer[i];
    lCall.miVolumes = liVolumes;
    lCall.miResults = liResults;
    gaVvqInitCalls.push_back(lCall);
    std::memset(lppBuffer[0], 0, sizeof(VolumeVolumeQuery));
    return lppBuffer[0];
}

BoxVolume* BoxVolume::Initialize(const ::rw::Resource& arResource, const Vec4& arHalfDimensions)
{
    gaEvents.push_back(Event{ E_BOX_INITIALIZE, 0 });
    BoxInitCall lCall;
    lCall.mpBlock = arResource.m_baseResources[0];
    for (int i = 0; i < 4; ++i) lCall.mapRest[i] = arResource.m_baseResources[i + 1];
    lCall.mDims = arHalfDimensions;
    gaBoxInitCalls.push_back(lCall);
    if (lCall.mpBlock == nullptr) return nullptr;
    BoxVolume* lpBox = static_cast<BoxVolume*>(lCall.mpBlock);
    std::memset(lpBox, 0, sizeof(BoxVolume));
    lpBox->muVTableSlot = E_VOLUMETYPE_BBOX;
    lpBox->mBoxData.mfHx = arHalfDimensions.x;
    lpBox->mBoxData.mfHy = arHalfDimensions.y;
    lpBox->mBoxData.mfHz = arHalfDimensions.z;
    return lpBox;
}

SphereVolume* SphereVolume::Initialize(const ::rw::Resource& arResource, f32 afRadius)
{
    gaEvents.push_back(Event{ E_SPHERE_INITIALIZE, 0 });
    SphereInitCall lCall;
    lCall.mpBlock = arResource.m_baseResources[0];
    for (int i = 0; i < 4; ++i) lCall.mapRest[i] = arResource.m_baseResources[i + 1];
    lCall.mfRadius = afRadius;
    gaSphereInitCalls.push_back(lCall);
    if (lCall.mpBlock == nullptr) return nullptr;
    SphereVolume* lpSphere = static_cast<SphereVolume*>(lCall.mpBlock);
    std::memset(lpSphere, 0, sizeof(SphereVolume));
    lpSphere->muVTableSlot = E_VOLUMETYPE_SPHERE;
    lpSphere->mfRadius = afRadius;
    return lpSphere;
}

// Records the primed fields, answers with an exact overlap test, then poisons the fields.
int VolumeVolumeQuery::GetPrimitiveIntersections()
{
    gaEvents.push_back(Event{ E_QUERY, static_cast<s32>(gaQueries.size()) });
    QueryRecord lRecord;
    std::memset(&lRecord, 0, sizeof(lRecord));
    lRecord.mfPadding   = m_padding;
    lRecord.mpInputVols = m_inputVols;
    lRecord.mpInputMats = m_inputMats;
    lRecord.muNumInputs = m_numInputs;
    lRecord.muCurrInput = m_currInput;
    lRecord.muPairCount = m_volRefPairCount;
    lRecord.mpQueryVol  = m_queryVol;
    lRecord.mpQueryMtx  = m_queryMtx;
    lRecord.mpCullTable = m_cullTable;

    bool lbAnswer = false;
    if (m_inputVols != nullptr && m_inputMats != nullptr && m_queryVol != nullptr && m_queryMtx != nullptr)
    {
        const Volume* lpInput = m_inputVols[0];
        const rw::math::vpu::Matrix44Affine* lpInputMatrix = m_inputMats[0];
        lRecord.mpInputVolume = lpInput;
        lRecord.mpInputMatrix = lpInputMatrix;
        lRecord.muQueryType = m_queryVol->muVTableSlot;
        lRecord.mQueryPos   = m_queryMtx->wAxis;
        const f32 lfCx = lpInputMatrix->wAxis.x, lfCy = lpInputMatrix->wAxis.y, lfCz = lpInputMatrix->wAxis.z;
        const f32 lfR  = lpInput->mfRadius;
        const f32 lfQx = m_queryMtx->wAxis.x, lfQy = m_queryMtx->wAxis.y, lfQz = m_queryMtx->wAxis.z;
        if (m_queryVol->muVTableSlot == E_VOLUMETYPE_BBOX)
        {
            lRecord.mfHx = m_queryVol->mBoxData.mfHx;
            lRecord.mfHy = m_queryVol->mBoxData.mfHy;
            lRecord.mfHz = m_queryVol->mBoxData.mfHz;
            auto Excess = [](f32 lfC, f32 lfQ, f32 lfH) {
                const f32 lfD = std::fabs(lfC - lfQ) - lfH;
                return lfD > 0.0f ? lfD : 0.0f;
            };
            const f32 lfDx = Excess(lfCx, lfQx, lRecord.mfHx);
            const f32 lfDy = Excess(lfCy, lfQy, lRecord.mfHy);
            const f32 lfDz = Excess(lfCz, lfQz, lRecord.mfHz);
            lbAnswer = (lfDx * lfDx + lfDy * lfDy + lfDz * lfDz) <= lfR * lfR;
        }
        else
        {
            lRecord.mfRadius = m_queryVol->mfRadius;
            const f32 lfDx = lfCx - lfQx, lfDy = lfCy - lfQy, lfDz = lfCz - lfQz;
            const f32 lfSum = lfR + lRecord.mfRadius;
            lbAnswer = (lfDx * lfDx + lfDy * lfDy + lfDz * lfDz) <= lfSum * lfSum;
        }
    }
    lRecord.mbAnswer = lbAnswer;
    gaQueries.push_back(lRecord);

    // poison: the next query must prime every field again
    m_padding         = 99.0f;
    m_inputVols       = nullptr;
    m_inputMats       = nullptr;
    m_numInputs       = 7;
    m_currInput       = 5;
    m_volRefPairCount = 9;
    m_queryVol        = nullptr;
    m_queryMtx        = nullptr;
    m_cullTable       = reinterpret_cast<const rw::BitTable::Storage*>(static_cast<uintptr_t>(0x10));
    return lbAnswer ? 1 : 0;
}

} }

// ---- the production text ---------------------------------------------------------------------------------------
namespace CgsSceneManager
{
#include "fxfu_octvol.inc"

    // The virtuals this test does not exercise (the vtable needs them).
    void LooseOctree::Destruct() {}
    bool LooseOctree::Prepare() { return true; }
    bool LooseOctree::Release() { return true; }
    bool LooseOctree::SphereTest(u32, Vector3, f32, CoarseQueryResultBuffer<16384>*) { return false; }
    bool LooseOctree::LineTest(u32, Vector3, Vector3, CoarseQueryResultBuffer<16384>*) { return false; }
    bool LooseOctree::FrustumTestVp(u32, const CgsGeometric::Frustum&, const Matrix44&,
                                    CoarseQueryResultBuffer<16384>*) { return false; }
    void LooseOctree::Update() {}
    void LooseOctree::SetEntityPosition(u16, Vector3) {}
    void LooseOctree::SetEntityRadius(u16, f32) {}
    void LooseOctree::FrustumTestEntities(const CgsGeometric::Frustum&, u32, const u16*, s32,
                                          CoarseQueryResultBuffer<16384>*) {}
    void LooseOctree::AddEntityToGraph(u16) {}
    void LooseOctree::RemoveEntityFromGraph(u16) {}
}

using namespace CgsSceneManager;
typedef SpatialPartition::VolumeTestRecursiveFuncParams Params;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPassed, const char* lpcName)
{
    ++giChecks;
    if (!lbPassed) { ++giFailures; std::printf("FAIL  %s\n", lpcName); }
    else           { std::printf("ok    %s\n", lpcName); }
}

static Vector3 V3(f32 x, f32 y, f32 z, f32 w) { Vector3 v; v.x = x; v.y = y; v.z = z; v.w = w; return v; }
static Vector4 V4(f32 x, f32 y, f32 z, f32 w) { Vector4 v; v.x = x; v.y = y; v.z = z; v.w = w; return v; }
static bool Same(const Vector3& a, f32 x, f32 y, f32 z, f32 w) { return a.x == x && a.y == y && a.z == z && a.w == w; }

// ---- the tree under test -----------------------------------------------------------------------------------------
alignas(alignof(LooseOctree)) static unsigned char gaOctreeStorage[sizeof(LooseOctree)];
static TestAllocator gAllocator;

static LooseOctree* BuildTree()
{
    std::memset(gaOctreeStorage, 0, sizeof(gaOctreeStorage));
    LooseOctree* lpTree = ::new (static_cast<void*>(gaOctreeStorage)) LooseOctree();
    // poison the frames: Construct must write every lane
    for (Matrix44Affine* lpFrame : { &lpTree->mNodeTransform, &lpTree->mEntityTransform })
    {
        lpFrame->xAxis = V3(7, 7, 7, 7); lpFrame->yAxis = V3(7, 7, 7, 7);
        lpFrame->zAxis = V3(7, 7, 7, 7); lpFrame->wAxis = V3(7, 7, 7, 7);
    }
    SpatialPartitionConstructParams lParams;
    std::memset(&lParams, 0, sizeof(lParams));
    lParams.muDepth = 2;   // the root plus one static level: nodes 1..4 are the root's children
    lParams.mCentrePos = V3(0, 0, 0, 0);
    lParams.mfBaseSize = 1000.0f;
    lParams.mfLooseness = 0.3f;
    lParams.muAdaptiveNodeSplitThreshold = 32;
    lParams.muAdaptiveMaxDepth = 10;
    lpTree->Construct(&lParams, &gAllocator);
    for (int i = 0; i < SpatialPartition::KI_MAX_NUM_ENTITIES; ++i)
    {
        lpTree->maEntityLinks[i].mx32TypeFlags  = 0;
        lpTree->maEntityLinks[i].mu16NextEntity = 0xFFFF;
        lpTree->maEntityLinks[i].mu16PrevEntity = 0xFFFF;
    }
    return lpTree;
}

// Geometry for node n: centre (x, y, z, w), HalfSize in mParams0.w, HalfHeight in mParams1.z. mHalfDimensions
// gets DIFFERENT numbers, so a body reading it instead is caught.
static void SetNode(LooseOctree* lpTree, int n, Vector3 lPosition, f32 lfHalfSize, f32 lfHalfHeight,
                    u32 luNodeFlags, u32 luSubTreeFlags)
{
    LooseOctreeNode& lrNode = lpTree->mpNodes[n];
    lrNode.mPosition = lPosition;
    lrNode.mHalfDimensions = V3(lfHalfSize * 3.0f, lfHalfHeight * 3.0f, lfHalfSize * 3.0f, 0.0f);
    lrNode.mParams0 = V4(0.0f, lfHalfSize * 5.0f, 0.0f, lfHalfSize);
    lrNode.mParams1 = V4(lPosition.y - lfHalfHeight, lPosition.y + lfHalfHeight, lfHalfHeight, 0.0f);
    lrNode.mxNodeEntityFlags = luNodeFlags;
    lrNode.muNumElements = 0;
    lrNode.muHeadIndex = 0xFFFF;
    lrNode.muTailIndex = 0xFFFF;
    lpTree->mpNodesEntityInfo[n].mxSubTreeEntityFlags = luSubTreeFlags;
}

struct EntitySpec { u16 mu16Index; u32 mx32Type; CgsGeometric::Sphere mSphere; };
static CgsGeometric::Sphere Sphere(f32 x, f32 y, f32 z, f32 r) { CgsGeometric::Sphere s; s.mPositionRadius = V4(x, y, z, r); return s; }
static void FileChain(LooseOctree* lpTree, int n, const std::vector<EntitySpec>& laSpecs, u32 luCount)
{
    LooseOctreeNode& lrNode = lpTree->mpNodes[n];
    lrNode.muNumElements = luCount;
    lrNode.muHeadIndex = laSpecs.empty() ? 0xFFFF : laSpecs.front().mu16Index;
    for (size_t i = 0; i < laSpecs.size(); ++i)
    {
        SpatialPartitionEntityLink& lrLink = lpTree->maEntityLinks[laSpecs[i].mu16Index];
        lrLink.mx32TypeFlags  = laSpecs[i].mx32Type;
        lrLink.mu16NextEntity = (i + 1 < laSpecs.size()) ? laSpecs[i + 1].mu16Index : 0xFFFF;
        lrLink.mu16PrevEntity = (i > 0) ? laSpecs[i - 1].mu16Index : 0xFFFF;
        lpTree->maEntityBoundingSpheres[laSpecs[i].mu16Index] = laSpecs[i].mSphere;
    }
}

static CoarseQueryResultBuffer<16384>* NewBuffer(bool lbBegin = true)
{
    CoarseQueryResultBuffer<16384>* lpBuffer = new CoarseQueryResultBuffer<16384>();
    lpBuffer->Construct();
    if (lbBegin) lpBuffer->BeginResultsBatch();
    return lpBuffer;
}
static std::vector<u16> Pushed(CoarseQueryResultBuffer<16384>* lpBuffer)
{
    std::vector<u16> la;
    for (s32 i = lpBuffer->miCurrentResultsStartPosition; i < lpBuffer->miTotalBufferSize; ++i) la.push_back(lpBuffer->mau16Buffer[i]);
    return la;
}

// The caller's volume: a sphere of radius r (the director's camera probe is a 0.1 m one), placed by the
// caller's transform.
struct Probe
{
    alignas(16) unsigned char maVolume[sizeof(rw::collision::SphereVolume)];
    Matrix44Affine mTransform;
    const VolRef::Volume* Volume() const { return reinterpret_cast<const VolRef::Volume*>(maVolume); }
};
static void MakeProbe(Probe& lrProbe, f32 x, f32 y, f32 z, f32 r)
{
    std::memset(lrProbe.maVolume, 0, sizeof(lrProbe.maVolume));
    rw::collision::SphereVolume* lpSphere = reinterpret_cast<rw::collision::SphereVolume*>(lrProbe.maVolume);
    lpSphere->muVTableSlot = rw::collision::E_VOLUMETYPE_SPHERE;
    lpSphere->mfRadius = r;
    lrProbe.mTransform.SetIdentity();
    lrProbe.mTransform.wAxis = V3(x, y, z, 1.0f);
}

static void ResetLogs() { gaEvents.clear(); gaQueries.clear(); gaAsserts.clear(); }

// Every query primed the nine fields afresh (the fixture poisons them after each call).
static bool PrimedEveryTime(const LooseOctree* lpTree, const Probe& lrProbe, int* lpiBad)
{
    for (size_t i = 0; i < gaQueries.size(); ++i)
    {
        const QueryRecord& q = gaQueries[i];
        const bool lbOk = q.mfPadding == 0.0f && q.muNumInputs == 1 && q.muCurrInput == 0 && q.muPairCount == 0
                       && q.mpCullTable == nullptr && q.mpInputVolume == lrProbe.Volume()
                       && q.mpInputMatrix == &lrProbe.mTransform
                       && (q.mpQueryVol == lpTree->mpNodeVolume || q.mpQueryVol == lpTree->mpEntityVolume)
                       && q.mpQueryMtx == ((q.mpQueryVol == lpTree->mpNodeVolume)
                                           ? static_cast<const void*>(&lpTree->mNodeTransform)
                                           : static_cast<const void*>(&lpTree->mEntityTransform));
        if (!lbOk) { if (lpiBad) *lpiBad = static_cast<int>(i); return false; }
    }
    return true;
}

// A compact trace of the queries: 'N' + node centre x for a box, 'E' + sphere centre x for an entity.
static std::string Trace(const LooseOctree* lpTree)
{
    std::string ls;
    char lac[48];
    for (const QueryRecord& q : gaQueries)
    {
        std::snprintf(lac, sizeof(lac), "%s%g%s ", q.mpQueryVol == lpTree->mpNodeVolume ? "N" : "E",
                      static_cast<double>(q.mQueryPos.x), q.mbAnswer ? "+" : "-");
        ls += lac;
    }
    return ls;
}

int main()
{
    // =============================================================================================================
    // B. Construct builds the walk's state (0x828C9E34..0x828CA09C)
    // =============================================================================================================
    ResetLogs();
    gaDescriptorCalls.clear(); gaVvqInitCalls.clear(); gaBoxInitCalls.clear(); gaSphereInitCalls.clear();
    LooseOctree* lpTree = BuildTree();
    {
        const bool lbVvq = gaDescriptorCalls.size() == 1 && gaDescriptorCalls[0].miVolumes == 100
                        && gaDescriptorCalls[0].miResults == 100 && gaVvqInitCalls.size() == 1
                        && gaVvqInitCalls[0].mapBuffers[0] == lpTree->macVolumeVolumeQueryBuffer
                        && gaVvqInitCalls[0].mapBuffers[1] == nullptr && gaVvqInitCalls[0].mapBuffers[4] == nullptr
                        && gaVvqInitCalls[0].miVolumes == 100 && gaVvqInitCalls[0].miResults == 100
                        && static_cast<void*>(lpTree->mpVolumeVolumeQuery) == lpTree->macVolumeVolumeQueryBuffer;
        Check(lbVvq, "B1 Construct: GetResourceDescriptor(100, 100), Initialize({macVolumeVolumeQueryBuffer, 0,0,0,0}, 100, 100) -> mpVolumeVolumeQuery");

        bool lbFrames = true;
        for (const Matrix44Affine* lpFrame : { &lpTree->mNodeTransform, &lpTree->mEntityTransform })
        {
            lbFrames = lbFrames && Same(lpFrame->xAxis, 1, 0, 0, 0) && Same(lpFrame->yAxis, 0, 1, 0, 0)
                    && Same(lpFrame->zAxis, 0, 0, 1, 0) && Same(lpFrame->wAxis, 0, 0, 0, 0);
        }
        Check(lbFrames, "B2 Construct: mNodeTransform and mEntityTransform = identity rows, all-zero translation row (w too)");

        const bool lbBox = gaBoxInitCalls.size() == 1 && gaBoxInitCalls[0].mpBlock == lpTree->macBoxVolumeBuffer
                        && gaBoxInitCalls[0].mapRest[0] == nullptr && gaBoxInitCalls[0].mapRest[3] == nullptr
                        && gaBoxInitCalls[0].mDims.x == 1.0f && gaBoxInitCalls[0].mDims.y == 1.0f
                        && gaBoxInitCalls[0].mDims.z == 1.0f && gaBoxInitCalls[0].mDims.w == 0.0f
                        && static_cast<void*>(lpTree->mpNodeVolume) == lpTree->macBoxVolumeBuffer;
        Check(lbBox, "B3 Construct: BoxVolume::Initialize({macBoxVolumeBuffer, 0,0,0,0}, {1,1,1,0}) -> mpNodeVolume");

        const bool lbSphere = gaSphereInitCalls.size() == 1
                           && gaSphereInitCalls[0].mpBlock == lpTree->macSphereVolumeBuffer
                           && gaSphereInitCalls[0].mapRest[0] == nullptr && gaSphereInitCalls[0].mfRadius == 1.0f
                           && static_cast<void*>(lpTree->mpEntityVolume) == lpTree->macSphereVolumeBuffer;
        Check(lbSphere, "B4 Construct: SphereVolume::Initialize({macSphereVolumeBuffer, 0,0,0,0}, 1.0) -> mpEntityVolume");

        std::vector<EEvent> laOrder;
        for (const Event& e : gaEvents) laOrder.push_back(e.meKind);
        Check(laOrder == std::vector<EEvent>{ E_DESCRIPTOR, E_VVQ_INITIALIZE, E_BOX_INITIALIZE, E_SPHERE_INITIALIZE }
              && gaAsserts.empty(),
              "B5 Construct: descriptor, query Initialize, box, sphere -- in the console's order, no assert");
    }

    // The scene: root node 0 at (0,0,0) HalfSize 100 / HalfHeight 20; its four children (1..4) at
    // x = -50, 50, -50, 50 / z = -50, -50, 50, 50, HalfSize 50 / HalfHeight 20.
    const u32 KX_CAR = 0x2, KX_PROP = 0x4, KX_QUERY = 0x2 | 0x8;
    SetNode(lpTree, 0, V3(0, 0, 0, 7.5f), 100.0f, 20.0f, KX_CAR | KX_PROP, KX_CAR | KX_PROP);
    SetNode(lpTree, 1, V3(-50, 0, -50, 1.0f), 50.0f, 20.0f, KX_CAR, KX_CAR);
    SetNode(lpTree, 2, V3( 50, 0, -50, 2.0f), 50.0f, 20.0f, KX_CAR, KX_CAR);
    SetNode(lpTree, 3, V3(-50, 0,  50, 3.0f), 50.0f, 20.0f, KX_PROP, KX_PROP);   // mask misses the query
    SetNode(lpTree, 4, V3( 50, 0,  50, 4.0f), 50.0f, 20.0f, 0, KX_CAR);          // NO node mask, sub-tree mask set
    const bool lbTopology = lpTree->mpNodes[0].muFirstChildIndex == 1 && lpTree->mpNodes[1].muFirstChildIndex == KU_INVALID_NODE;

    // =============================================================================================================
    // V/R. VolumeTest + VolumeTestRecursive
    // =============================================================================================================
    // R1/R2/R3: a probe outside the root box -- one query (the root box), nothing else.
    {
        ResetLogs();
        Probe lProbe; MakeProbe(lProbe, 500.0f, 0.0f, 0.0f, 0.1f);
        CoarseQueryResultBuffer<16384>* lpBuffer = NewBuffer();
        const bool lbAnswer = lpTree->VolumeTest(KX_QUERY, lProbe.Volume(), &lProbe.mTransform, lpBuffer);
        const QueryRecord* lpQ = gaQueries.empty() ? nullptr : &gaQueries[0];
        Check(lbTopology && lpQ != nullptr && lpQ->mpQueryVol == lpTree->mpNodeVolume
              && lpQ->mpQueryMtx == &lpTree->mNodeTransform && lpQ->muQueryType == rw::collision::E_VOLUMETYPE_BBOX
              && lpQ->mfHx == 100.0f && lpQ->mfHy == 20.0f && lpQ->mfHz == 100.0f && Same(lpQ->mQueryPos, 0, 0, 0, 7.5f),
              "R1 the node query: mpNodeVolume at &mNodeTransform, half extents {HalfSize, HalfHeight, HalfSize} = {mParams0.w, mParams1.z, mParams0.w}, translation = the node's whole mPosition (w 7.5)");
        int liBad = -1;
        Check(!gaQueries.empty() && PrimedEveryTime(lpTree, lProbe, &liBad),
              "R2 every query primed: padding 0, the caller's volume / transform as the one input, 1 input, input 0, 0 pairs, no cull table");
        Check(gaQueries.size() == 1 && Pushed(lpBuffer).empty() && !lbAnswer,
              "R3 a volume that misses the node box prunes the sub-tree: one query, no push, VolumeTest answers false");
        delete lpBuffer;
    }

    // V1/V2 + R4..R8: a probe at (30, 0, -40) radius 45 overlapping the root and children 1, 2 and 4.
    {
        ResetLogs();
        // root chain: a car hit at x=20 (index 10), a prop (type misses the query) at x=21 (index 11),
        // a car miss at x=90 (index 12), a car hit at x=40 (index 13); muNumElements 1 -- the sentinel ends the
        // chain, not the count.
        FileChain(lpTree, 0, { { 10, KX_CAR,  Sphere(20, 0, -40, 3) }, { 11, KX_PROP, Sphere(21, 0, -40, 3) },
                               { 12, KX_CAR,  Sphere(90, 0, 60, 3) },  { 13, KX_CAR,  Sphere(40, 0, -40, 2) } }, 1);
        // child 1 (box 30 m away): one car at x=-10 (dist 40 <= 45 + 12); child 2 (the probe inside): a car miss
        // at x=95 (dist 65 > 48); child 3: its sub-tree mask misses the query; child 4 (box 40 m away, NO node
        // mask): one car at x=30,z=0 (dist 40 <= 45 + 15).
        FileChain(lpTree, 1, { { 20, KX_CAR, Sphere(-10, 0, -40, 12) } }, 1);
        FileChain(lpTree, 2, { { 30, KX_CAR, Sphere(95, 0, -40, 3) } }, 1);
        FileChain(lpTree, 3, { { 40, KX_PROP, Sphere(30, 0, 40, 3) } }, 1);
        FileChain(lpTree, 4, { { 50, KX_CAR, Sphere(30, 0, 0, 15) } }, 1);

        Probe lProbe; MakeProbe(lProbe, 30.0f, 0.0f, -40.0f, 45.0f);
        CoarseQueryResultBuffer<16384>* lpBuffer = NewBuffer();
        const bool lbAnswer = lpTree->VolumeTest(KX_QUERY, lProbe.Volume(), &lProbe.mTransform, lpBuffer);
        const std::string lsTrace = Trace(lpTree);
        std::printf("      trace: %s\n", lsTrace.c_str());

        const QueryRecord* lpFirst = gaQueries.empty() ? nullptr : &gaQueries[0];
        const bool lbBlock = lpFirst != nullptr
                          && reinterpret_cast<const char*>(lpFirst->mpInputMats) - reinterpret_cast<const char*>(lpFirst->mpInputVols)
                             == static_cast<std::ptrdiff_t>(offsetof(Params, mpTransform))
                          && offsetof(Params, mpVolume) == 0;
        Check(lbBlock, "V1 VolumeTest's block: the input-volume array IS the block (&mpVolume, its first member) and the matrix array is &mpTransform in the same block");

        const bool lbMonitor = gaEvents.size() >= 3 && gaEvents.front().meKind == E_START_MONITOR
                            && gaEvents.front().miValue == -1 && gaEvents.back().meKind == E_STOP_MONITOR
                            && gaEvents.back().miValue == -1 && gaEvents[1].meKind == E_QUERY;
        Check(lbMonitor, "V2 VolumeTest brackets the walk with Start/StopMonitor(_miVolumeTestPerfMon == -1)");

        // expected: N0+ E20+ E90- E40+ (index 11 skipped: type) | N-50+ E-10+ | N50+ E95- | (child 3 not visited:
        // sub-tree mask) | N50+ (child 4, z=50) E30+
        Check(lsTrace == "N0+ E20+ E90- E40+ N-50+ E-10+ N50+ E95- N50+ E30+ ",
              "R4 the query order: node box, its type-matching chain to the 0xFFFF sentinel (count 1, four links), then children 1, 2, 4 in order, each box before its chain");
        const std::vector<u16> laPushed = Pushed(lpBuffer);
        Check(laPushed == std::vector<u16>{ 10, 13, 20, 50 },
              "R5 pushed in walk order, by entity index: 10, 13 (root), 20 (child 1), 50 (child 4)");

        bool lbSpheres = true;
        for (const QueryRecord& q : gaQueries)
        {
            if (q.mpQueryVol != lpTree->mpEntityVolume) continue;
            lbSpheres = lbSpheres && q.muQueryType == rw::collision::E_VOLUMETYPE_SPHERE && q.mpQueryMtx == &lpTree->mEntityTransform
                     && q.mfRadius == q.mQueryPos.w;   // the radius and the translation row's w are both sphere.w
        }
        const QueryRecord* lpE20 = gaQueries.size() > 1 ? &gaQueries[1] : nullptr;
        Check(lbSpheres && lpE20 != nullptr && Same(lpE20->mQueryPos, 20, 0, -40, 3) && lpE20->mfRadius == 3.0f,
              "R6 each entity query: mpEntityVolume at &mEntityTransform, radius = sphere.w, translation row = the WHOLE sphere vector (w = radius)");
        Check(lbAnswer, "R7 VolumeTest answers true when the walk attempted a result");

        int liBad = -1;
        Check(PrimedEveryTime(lpTree, lProbe, &liBad) && gaAsserts.empty(),
              "R8 all ten queries primed afresh; no assert");
        delete lpBuffer;
    }

    // R9: muNumElements 0 with a live head -- the count gates the entry: no entity query.
    {
        ResetLogs();
        FileChain(lpTree, 0, { { 10, KX_CAR, Sphere(20, 0, -40, 3) } }, 0);
        SetNode(lpTree, 1, V3(-50, 0, -50, 1.0f), 50.0f, 20.0f, KX_CAR, 0);
        SetNode(lpTree, 2, V3( 50, 0, -50, 2.0f), 50.0f, 20.0f, KX_CAR, 0);
        SetNode(lpTree, 3, V3(-50, 0,  50, 3.0f), 50.0f, 20.0f, KX_PROP, 0);
        SetNode(lpTree, 4, V3( 50, 0,  50, 4.0f), 50.0f, 20.0f, 0, 0);
        lpTree->mpNodes[0].muHeadIndex = 10;
        Probe lProbe; MakeProbe(lProbe, 20.0f, 0.0f, -40.0f, 5.0f);
        CoarseQueryResultBuffer<16384>* lpBuffer = NewBuffer();
        const bool lbAnswer = lpTree->VolumeTest(KX_QUERY, lProbe.Volume(), &lProbe.mTransform, lpBuffer);
        Check(gaQueries.size() == 1 && Pushed(lpBuffer).empty() && !lbAnswer,
              "R9 muNumElements 0 with a live head: no entity query (the count gates the entry), no children with empty sub-tree masks");
        delete lpBuffer;
    }

    // V3/V4: the attempted-count read asserts outside a batch (:348) -- the accessor, and VolumeTest's inlined one.
    {
        ResetLogs();
        CoarseQueryResultBuffer<16384>* lpBuffer = NewBuffer(false);
        (void)lpBuffer->GetNumResultsAttempted();
        Check(CountAsserts("GetNumResultsAttempted called outside of a BeginResultsBatch/EndResultsBatch pair") == 1,
              "V3 CoarseQueryResultBuffer::GetNumResultsAttempted asserts outside a batch (0x828ADCD8, :348)");
        ResetLogs();
        Probe lProbe; MakeProbe(lProbe, 500.0f, 0.0f, 0.0f, 0.1f);
        (void)lpTree->VolumeTest(KX_QUERY, lProbe.Volume(), &lProbe.mTransform, lpBuffer);
        Check(CountAsserts("GetNumResultsAttempted called outside of a BeginResultsBatch/EndResultsBatch pair") == 1,
              "V4 VolumeTest outside a batch fires that assert once (0x828CA95C..0x828CA994)");
        delete lpBuffer;
    }

    std::printf("FxFollowupsOctreeVolume: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
