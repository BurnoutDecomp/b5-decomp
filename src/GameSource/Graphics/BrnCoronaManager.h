#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"          // Vector2, Vector3, Vector4, Matrix44
#include "GameSource/Effects/Curves.h"  // BrnEffects::Curves::SmoothStep
#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h"
#include "rw/rwcore_structs.h"       // rw::RGBA, rw::Resource, rw::IResourceAllocator

// Pointer/reference-only uses below; forward-declared to avoid pulling in the full renderer /
// resource-allocator header cascades (AGENTS.md forward-declaration exception (b)).
namespace renderengine { class Texture; class TextureState; }



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
// distance-fade bias / falloff curve). mParams[eCoronaTypeCount] is the archetype table.
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
    f32 mfBiasDistance;                          // BrnCoronaManager.h:89
    BrnEffects::Curves::SmoothStep mScaleCurve;   // BrnCoronaManager.h:90

    // BrnCoronaManager.h:80. Attested through sub_823FD428, which indexes the table at a 48-byte
    // console stride and tail-calls AddCorona. HOMED BY GROUP `coronadata` (SEAM S2) in
    // GameSource/Graphics/BrnCoronaTypeParams.cpp, together with the table itself.
    static const BrnCoronaTypeParams& GetCoronaTypeParams(BrnCoronaType leType) { return mParams[leType]; }

private:
    // BrnCoronaManager.h:94 (the DWARF member name is `mParams`; this tree's committed spelling is
    // mParams -- kept, and recorded here so the two are not mistaken for different members).
    // Defined where GetCoronaTypeParams is bodied (group `coronadata`).
    static BrnCoronaTypeParams mParams[eCoronaTypeCount];
};

// BrnCoronaManager.h:98 (DWARF) -- the world corona system: an atlas texture/sampler state, two
// double-buffered corona-vertex buffers, and a per-frame submission interface pair
// (BrnSubmissionInterface[2], swapped by mu8SubmissionSwapIndex). Owned by BrnRendererModule
// (mCoronaManager, X360 renderer+0x3800).
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
        // running write count (used by the AddCorona/AddPropCorona KI_MAX_CORONAS assert and, in
        // BrnCoronaManager::Render, as the batch's corona count: `lwz r11, 8(r31)` @0x82407690
        // reads mBufferIterator.muIndex at interface+0x08).
        struct DerivedCoronaIterator : public renderengine::CoronaBuffer::Iterator
        {
        public:
            uint32_t GetNumCoronasWritten() const { return muIndex; }
        };

        // BrnCoronaManager.h:113. INLINED on the console -- recovered from BrnRendererModule::Update
        // @0x82405EC4-0x82405FA0 (see the .cpp).
        void SetCameraInfo(Matrix44 lViewProj, Vector3 lCameraPosition, Vector4 lViewXyScale);

        // BrnCoronaManager.h:126 / asm @ 0x823FD138. Bodied in the .cpp.
        void AddPropCorona(Vector3 lvPosition, Vector3 lvDirection,
                           const rw::math::fpu::Vector2Template<f32>& lvSize,
                            f32 lfAlpha, int32_t liTextureID);

        // BrnCoronaManager.h:134 / asm @ 0x823FD270. Bodied in the .cpp.
        void AddCorona(Vector3 lvPosition, Vector3 lvDirection, f32 lfScale,
                        f32 lfOpacity, const BrnCoronaTypeParams& lCoronaTypeParams);

        // BrnCoronaManager.h:142 / asm @ sub_823FD428 -- the one-line forwarder that indexes the
        // archetype table by type. Bodied in the .cpp.
        void AddCorona(Vector3 lvPosition, Vector3 lvDirection, f32 lfScale,
                        f32 lfOpacity, const BrnCoronaType& leCoronaType);

        // BrnCoronaManager.h:159. Not X360-attested for this TU -- declared only.
        const BrnCoronaTypeParams& GetTypeParams(BrnCoronaType leType);

    private:
        // BrnCoronaManager.h:167.
        static const int32_t KI_MAX_CORONAS = 512;

        // BrnCoronaManager.h:173..177. Confirmed member order/offsets by the AddCorona/AddPropCorona
        // asm (mBufferIterator at +0x04, mCameraPosition at +0x50) and by Render @0x824075D8
        // (mViewProj at +0x10, mViewXyScale at +0x60, sizeof == 0x70).
        renderengine::CoronaBuffer* mpBuffer;
        DerivedCoronaIterator       mBufferIterator;
        Matrix44                    mViewProj;
        Vector3                     mCameraPosition;
        Vector4                     mViewXyScale;

        // BrnCoronaManager.h:170. Not X360-attested -- declared only.
        void Construct();

        friend class BrnCoronaManager;
    };

    // BrnCoronaManager.h:184 / asm @ 0x823FCD90. Bodied in the .cpp.
    //
    // ⚠ DELIBERATE DWARF DEVIATION, flagged rather than silent. The DWARF spells the parameter
    // `BrnResource::LinearResourceAllocator&`; this takes the BASE `rw::IResourceAllocator&`,
    // because (a) the X360 body uses the argument for exactly one thing -- the virtual at vtable
    // +0x10, `lwz r11,0(r31); lwz r11,0x10(r11); bctrl` @0x823FCE28-44 -- which is
    // rw::IResourceAllocator::DoAllocate, and (b) the sibling SetTextureAtlas below is ALREADY
    // committed with `const rw::IResourceAllocator&` for the same reason, so the base spelling is
    // the tree's own convention here rather than a second one. Retype BOTH together if
    // BrnResource::LinearResourceAllocator is ever reconstructed.
    void Construct(rw::IResourceAllocator& lrAllocator);

    // BrnCoronaManager.h:187. INLINED on the console (no standalone address). Bodied in the .cpp.
    void Destruct();

    // BrnCoronaManager.h:190. The eRendererPrepareCoronas stage of BrnRendererModule::Prepare, which
    // is not reconstructed on PC -- see the .cpp. Declared only.
    bool Prepare();

    // BrnCoronaManager.h:193. Not X360-attested for this TU (attested elsewhere: ledger TU
    // SDKs/EATech/include/cmn/rw/core/resource/resourceallocator.h @ 0x823F7720, not bodied there
    // either) -- declared only here.
    bool Release(const rw::IResourceAllocator& lpAllocator);

    // BrnCoronaManager.h:196. INLINED into BrnRendererModule::StartOfFrame @0x823FC160. Bodied.
    void Clear();

    // BrnCoronaManager.h:199. INLINED into BrnRendererModule::SwapBuffers @0x823FC678. Bodied.
    void Swap();

    // BrnCoronaManager.h:202 / asm @ 0x824075D8. Bodied in the .cpp.
    void Render(f32 lfWhiteLevel);

    // BrnCoronaManager.h:205. INLINED into BrnRendererModule::Update @0x824060F0-108. Bodied.
    BrnSubmissionInterface* GetSubmissionInterface();

    // BrnCoronaManager.h:213 / asm @ 0x823FD000. Bodied in the .cpp.
    void SetTextureAtlas(const rw::IResourceAllocator& lAllocator, renderengine::Texture* lpTextureAtlas);

    bool IsConstructed() const { return m_coronaBuffer0 != 0 && m_coronaBuffer1 != 0; }

protected:
    bool                        mbActive;                       // BrnCoronaManager.h:217
    renderengine::Texture*      m_textureAtlas;                 // BrnCoronaManager.h:219
    renderengine::TextureState* m_textureStateAtlas;             // BrnCoronaManager.h:220
    renderengine::CoronaBuffer* m_coronaBuffer0;                 // BrnCoronaManager.h:222
    renderengine::CoronaBuffer* m_coronaBuffer1;                 // BrnCoronaManager.h:223
    rw::Resource                m_textureStateAtlasResource;     // BrnCoronaManager.h:225
    rw::Resource                m_coronaBuffer0Resource;         // BrnCoronaManager.h:226
    rw::Resource                m_coronaBuffer1Resource;         // BrnCoronaManager.h:227
    // BrnCoronaManager.h:229. NOT const -- the DWARF declares `Vector2[12][4] s_atlasUVs` with no
    // const, and the console agrees: the 768 bytes at unk_82FAFC10 are ZERO in the shipped image and
    // are written at run time by the static initialiser sub_82C4EDD8. (The previous `static const`
    // spelling was an inference from "it never changes after startup", and it is wrong about the
    // storage class -- a const object cannot be the target of that initialiser.)
    static Vector2              s_atlasUVs[12][4];
    BrnSubmissionInterface      mSubmissionInterface[2];         // BrnCoronaManager.h:231
    uint8_t                     mu8SubmissionSwapIndex;          // BrnCoronaManager.h:232
};

CGS_STATIC_ASSERT((sizeof(BrnCoronaTypeParams) == 0x30), "ARTIST corona type stride");
CGS_STATIC_ASSERT((offsetof(BrnCoronaTypeParams, mScaleCurve) == 0x18), "ARTIST packed curve offset");
CGS_STATIC_ASSERT((sizeof(BrnCoronaManager::BrnSubmissionInterface) == 0x70), "ARTIST submission interface");
CGS_STATIC_ASSERT((sizeof(BrnCoronaManager) == 0x140), "ARTIST corona manager");
