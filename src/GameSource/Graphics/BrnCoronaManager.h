#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"          // Vector2, Vector3, Vector4, Matrix44
#include "GameSource/Effects/Curves.h"  // BrnEffects::Curves::SmoothStep
#include "rw/rwcore_structs.h"       // rw::RGBA, rw::Resource, rw::IResourceAllocator

// Pointer/reference-only uses below; forward-declared to avoid pulling in the full renderer /
// resource-allocator header cascades (AGENTS.md forward-declaration exception (b)).
namespace renderengine { class Texture; class TextureState; }
namespace BrnResource { class LinearResourceAllocator; }

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnCoronaManager::BrnSubmissionInterface::AddCorona(..., const BrnCoronaTypeParams&) @ 0x823FD270
//   BrnCoronaManager::BrnSubmissionInterface::AddPropCorona                              @ 0x823FD138
//   BrnCoronaManager::SetTextureAtlas                                                    @ 0x823FD000
//
// Home path: GameSource/Unity/../Graphics/BrnCoronaManager.cpp (ledger TU id).
//
// CROSS-TREE RECONCILIATION (this header is the single real home for ::BrnCoronaManager):
// prior to this pass the type existed as three inconsistent declarations --
//   * BrnRendererModuleIO.h / BrnRaceCarEntityModuleIO.h forward-declared it as a NAMESPACE
//     (`namespace BrnCoronaManager { struct BrnSubmissionInterface; }`) -- wrong: the DWARF
//     shape (below) is a class/struct, not a namespace.
//   * BrnRendererModule.h carried an empty `struct BrnCoronaManager {};` stub for the
//     mCoronaManager member.
//   * SDKs/RenderEngineClub/.../coronas/rwgcoronabuffer.h declared an unrelated
//     `class BrnCoronaManager` with a *different* (older/renderer-club-mocked) layout; that
//     declaration is where the X360-attested BrnCoronaManager::Construct/Release bodies
//     actually live per the ledger's own address->TU mapping (0x823FCD90 / 0x823F7720), so it
//     is left as-is (it is a distinct ledger TU, not this one) -- only the duplicate class
//     *name* collided, not the functions this TU owns.
// All three now `#include` this header instead of re-declaring the type. See the .cpp for the
// three ledger-tracked bodies; every other member function below is declared only (not
// X360-attested for this TU's DWARF address range -- see AGENTS.md's DWARF/ledger gating rule).
//
// Layout is the DecFIGS DWARF struct outline (references/DecFIGS/dwarfdump/GameSource/Graphics/
// BrnCoronaManager.h), gated on the X360 asm for the three bodied functions (BrnSubmissionInterface
// member offsets +0x00 mpBuffer, +0x04 mBufferIterator, +0x50 mCameraPosition -- confirmed by the
// AddCorona/AddPropCorona back-face-cull + assert store offsets).

namespace renderengine
{
// The corona render buffer's write cursor. Bodied (Write) in the corona vendor tree
// (SDKs/RenderEngineClub/MAIN/components/src/coronas/rwgcoronabufferiterator.cpp @ 0x823F3350);
// redeclared here at matching layout/signature per the established per-TU vendor-slice
// convention (renderengine::CoronaBuffer has no single shared header in this codebase -- each
// consumer declares the minimal slice it calls).
class CoronaBuffer
{
public:
    struct Iterator
    {
        u8* mpData;   // +0x00 record data area (this is result[0] in the X360 asm)
        u32 muIndex;  // +0x04 next record index (result[1])

        // 0x823F3350 -- write one 64-byte corona record and advance the cursor.
        //   lpVectorPayload : the 48 bytes staged in v1/v2/v3 (position/direction/size-ish record
        //                     data, record +0x00..+0x2F)
        //   lfFloat         : the f32 written at +0x30 (a per-corona bias distance)
        //   luColour        : the colour word written byte-swapped at +0x34
        //   liValue         : the value written at +0x38 (the texture id)
        Iterator* Write(const void* lpVectorPayload, f64 lfFloat, u32 luColour, int liValue);
    };
};
}

// BrnCoronaManager.h:39 (DWARF) -- the corona archetype table index. Traffic / race-car / player-car
// lighting + PS3-only Blu-Ray accent lights.
enum BrnCoronaType
{
    eCoronaTypeTrafficHeadLight          = 0,
    eCoronaTypeTrafficRearLight          = 1,
    eCoronaTypeTrafficBrakeLight         = 2,
    eCoronaTypeTrafficIndicator          = 3,
    eCoronaTypeTrafficSpotlights         = 4,
    eCoronaTypeTrafficLightGreen         = 5,
    eCoronaTypeTrafficLightAmber         = 6,
    eCoronaTypeTrafficLightRed           = 7,
    eCoronaTypeRaceCarHeadLight          = 8,
    eCoronaTypeRaceCarRearLight          = 9,
    eCoronaTypeRaceCarBrakeLight         = 10,
    eCoronaTypeRaceCarIndicator          = 11,
    eCoronaTypeRaceCarReversingLight     = 12,
    eCoronaTypeRaceCarBluesTwosRed       = 13,
    eCoronaTypeRaceCarBluesTwosBlue      = 14,
    eCoronaTypeRaceCarSpotlights         = 15,
    eCoronaTypePlayerCarHeadLight        = 16,
    eCoronaTypePlayerCarRearLight        = 17,
    eCoronaTypePlayerCarBrakeLight       = 18,
    eCoronaTypePlayerCarIndicator        = 19,
    eCoronaTypePlayerCarReversingLight   = 20,
    eCoronaTypePlayerCarBluesTwosRed     = 21,
    eCoronaTypePlayerCarBluesTwosBlue    = 22,
    eCoronaTypePlayerCarSpotlights       = 23,
    eCoronaTypePlayerCarPS3BluRayLights  = 24,
    eCoronaTypeCount                     = 25,
};

// BrnCoronaManager.h:75 (DWARF) -- one archetype's tunable render params (size / texture page /
// distance-fade bias / falloff curve). mParams[eCoronaTypeCount] is the archetype table;
// GetCoronaTypeParams is not X360-attested for this TU (no ledger address) so it is declared only.
//
// Offsets confirmed by the AddCorona (0x823FD270) asm reading lCoronaTypeParams by pointer:
//   +0x00 mvSize (loaded as a full 128-bit VMX register -> VMX-aligned Vector2, matches rw::math::
//         vpu::Vector2's 16-byte layout)
//   +0x10 miTextureID
//   +0x14 mfBiasDistance
//   +0x18 mScaleCurve.mCurveParams  (SmoothStep's Vector3, passed as Evaluate's first arg)
//   +0x24 mScaleCurve.mScaleFactors (SmoothStep's Vector2, passed as Evaluate's second arg)
struct BrnCoronaTypeParams
{
    Vector2 mvSize;                              // BrnCoronaManager.h:87
    int32_t miTextureID;                         // BrnCoronaManager.h:88
    f32 mfBiasDistance;                    // BrnCoronaManager.h:89
    BrnEffects::Curves::SmoothStep mScaleCurve;   // BrnCoronaManager.h:90

    // BrnCoronaManager.h:80. Not X360-attested for this TU -- declared only.
    static const BrnCoronaTypeParams& GetCoronaTypeParams(BrnCoronaType leType);

private:
    // BrnCoronaManager.h:94. The archetype table; defined (declaration-only data) where
    // GetCoronaTypeParams is bodied.
    static BrnCoronaTypeParams smParams[eCoronaTypeCount];
};

// BrnCoronaManager.h:98 (DWARF) -- the world corona system: an atlas texture/sampler state, two
// double-buffered corona-vertex buffers, and a per-frame submission interface pair
// (BrnSubmissionInterface[2], swapped by mu8SubmissionSwapIndex). Owned by BrnRendererModule
// (mCoronaManager). Only the three ledger-attested member functions below have real bodies in the
// .cpp; the rest are declared to keep the class shape coherent (per AGENTS.md's DWARF/X360-ledger
// gating rule -- an un-attested method stays declaration-only, never a guessed body).
class BrnCoronaManager
{
public:
    // BrnCoronaManager.h:102 (DWARF) -- the per-frame corona submission front-end handed out to
    // world/race-car/prop renderers (BrnRendererModule::GetCoronaSubmissionInterface,
    // RendererIO::GetCoronaSubmissionInterface). Writes land in mpBuffer via mBufferIterator.
    class BrnSubmissionInterface
    {
    public:
        // BrnCoronaManager.h:147 -- a CoronaBuffer::Iterator with a convenience accessor for the
        // running write count (used by the AddCorona/AddPropCorona KI_MAX_CORONAS assert). Its
        // storage IS the base Iterator's {mpData, muIndex} -- GetNumCoronasWritten reads muIndex,
        // matching the X360 assert's `mBufferIterator.GetNumCoronasWritten() < KI_MAX_CORONAS`
        // reading *(this+8) == mBufferIterator(+4).muIndex(+4).
        struct DerivedCoronaIterator : public renderengine::CoronaBuffer::Iterator
        {
        public:
            uint32_t GetNumCoronasWritten() const { return muIndex; }
        };

        // BrnCoronaManager.h:113. Not X360-attested for this TU -- declared only.
        void SetCameraInfo(const Matrix44& lViewProj, const Vector3& lCameraPosition, const Vector4& lViewXyScale);

        // BrnCoronaManager.h:126 / asm @ 0x823FD138. Bodied in the .cpp.
        void AddPropCorona(const Vector3& lvPosition, const Vector3& lvDirection, const Vector2& lvSize,
                            f32 lfAlpha, int32_t liTextureID);

        // BrnCoronaManager.h:134 / asm @ 0x823FD270. Bodied in the .cpp.
        void AddCorona(const Vector3& lvPosition, const Vector3& lvDirection, f32 lfScale,
                        f32 lfOpacity, const BrnCoronaTypeParams& lCoronaTypeParams);

        // BrnCoronaManager.h:142. No X360 address in the ledger for this overload (only the
        // BrnCoronaTypeParams overload above is attested) -- declared only.
        void AddCorona(const Vector3& lvPosition, const Vector3& lvDirection, f32 lfScale,
                        f32 lfOpacity, const BrnCoronaType& leCoronaType);

        // BrnCoronaManager.h:159. Not X360-attested for this TU -- declared only.
        const BrnCoronaTypeParams& GetTypeParams(BrnCoronaType leType);

    private:
        // BrnCoronaManager.h:167.
        static const int32_t KI_MAX_CORONAS = 512;

        // BrnCoronaManager.h:173..177. Confirmed member order/offsets by the AddCorona/AddPropCorona
        // asm (mBufferIterator at +0x04, mCameraPosition at +0x50).
        renderengine::CoronaBuffer* mpBuffer;
        DerivedCoronaIterator       mBufferIterator;
        Matrix44                    mViewProj;
        Vector3                     mCameraPosition;
        Vector4                     mViewXyScale;

        // BrnCoronaManager.h:170. Not X360-attested for this TU -- declared only.
        void Construct();

        friend class BrnCoronaManager;
    };

    // BrnCoronaManager.h:184. Not X360-attested for this TU (attested elsewhere: ledger TU
    // SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h @ 0x823FCD90,
    // a distinct declaration -- see header comment above) -- declared only here.
    void Construct(const BrnResource::LinearResourceAllocator& lpAllocator);

    // BrnCoronaManager.h:187. Not X360-attested for this TU -- declared only.
    void Destruct();

    // BrnCoronaManager.h:190. Not X360-attested for this TU -- declared only.
    bool Prepare();

    // BrnCoronaManager.h:193. Not X360-attested for this TU (attested elsewhere: ledger TU
    // SDKs/EATech/include/cmn/rw/core/resource/resourceallocator.h @ 0x823F7720, comment-only
    // reference, not yet bodied there either) -- declared only here.
    bool Release(const rw::IResourceAllocator& lpAllocator);

    // BrnCoronaManager.h:196. Not X360-attested for this TU -- declared only.
    void Clear();

    // BrnCoronaManager.h:199. Not X360-attested for this TU -- declared only.
    void Swap();

    // BrnCoronaManager.h:202. Not X360-attested for this TU -- declared only.
    void Render(f32 lfWhiteLevel);

    // BrnCoronaManager.h:205. Not X360-attested for this TU -- declared only.
    BrnSubmissionInterface* GetSubmissionInterface();

    // BrnCoronaManager.h:213 / asm @ 0x823FD000. Bodied in the .cpp.
    void SetTextureAtlas(const rw::IResourceAllocator& lAllocator, renderengine::Texture* lpTextureAtlas);

protected:
    bool                        mbActive;                       // BrnCoronaManager.h:217
    renderengine::Texture*      m_textureAtlas;                 // BrnCoronaManager.h:219
    renderengine::TextureState* m_textureStateAtlas;             // BrnCoronaManager.h:220
    renderengine::CoronaBuffer* m_coronaBuffer0;                 // BrnCoronaManager.h:222
    renderengine::CoronaBuffer* m_coronaBuffer1;                 // BrnCoronaManager.h:223
    rw::Resource                m_textureStateAtlasResource;     // BrnCoronaManager.h:225
    rw::Resource                m_coronaBuffer0Resource;         // BrnCoronaManager.h:226
    rw::Resource                m_coronaBuffer1Resource;         // BrnCoronaManager.h:227
    static const Vector2        s_atlasUVs[12][4];               // BrnCoronaManager.h:229
    BrnSubmissionInterface      mSubmissionInterface[2];         // BrnCoronaManager.h:231
    uint8_t                     mu8SubmissionSwapIndex;          // BrnCoronaManager.h:232
};
