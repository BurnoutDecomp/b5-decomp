#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"          // Vector2, Vector3, Vector4, Matrix44
#include "GameSource/Effects/Curves.h"  // BrnEffects::Curves::SmoothStep
#include "rw/rwcore_structs.h"       // rw::RGBA, rw::Resource, rw::IResourceAllocator

// Pointer/reference-only uses below; forward-declared to avoid pulling in the full renderer /
// resource-allocator header cascades (AGENTS.md forward-declaration exception (b)).
namespace renderengine { class Texture; class TextureState; }
// (`namespace BrnResource { class LinearResourceAllocator; }` was here for Construct's parameter.
//  Construct now takes the BASE interface -- see the DWARF-deviation note on its declaration below --
//  so the forward declaration has no user and is removed rather than left dangling.)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnCoronaManager::Construct                                                          @ 0x823FCD90
//   BrnCoronaManager::Render                                                             @ 0x824075D8
//   BrnCoronaManager::SetTextureAtlas                                                    @ 0x823FD000
//   BrnCoronaManager::BrnSubmissionInterface::AddCorona(..., const BrnCoronaTypeParams&)  @ 0x823FD270
//   BrnCoronaManager::BrnSubmissionInterface::AddCorona(..., const BrnCoronaType&)        @ sub_823FD428
//   BrnCoronaManager::BrnSubmissionInterface::AddPropCorona                               @ 0x823FD138
//   BrnCoronaManager::{Clear,Swap,Destruct,GetSubmissionInterface,
//                      BrnSubmissionInterface::SetCameraInfo}   INLINED -- recovered from their
//                      inliners (BrnRendererModule::StartOfFrame @0x823FC160, ::SwapBuffers
//                      @0x823FC678, ::Update @0x82405E28); see the .cpp for the per-function cites.
//
// Home path: GameSource/Graphics/BrnCoronaManager.cpp (ledger TU id).
//
// ---- THE DUPLICATE-TYPE MERGE (coronas step 1, 2026-08-17) -------------------------------------
// `renderengine::Corona` and the full `renderengine::CoronaBuffer` used to live in a SECOND header,
// SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h, alongside a THIRD,
// guest-offset declaration of `class BrnCoronaManager`. That header is now a shim that includes this
// one, and the guest-offset BrnCoronaManager duplicate is deleted -- its one body (Construct
// @0x823FCD90) is bodied here against the real members. The types moved DOWN into this header rather
// than this header being made to include that one, because this header has a 53-TU includer closure
// (measured; the list is in the wave's work/ dir) and adding an include to it would have pushed a
// new cascade through every one of them. Nothing new is #included here; the two types only need
// Vector2/Vector3 and rw::RGBA, which this header already had.
//
// Layout is the DecFIGS DWARF struct outline (references/DecFIGS/dwarfdump/GameSource/Graphics/
// BrnCoronaManager.h + .../coronas/rwgcorona.h + rwgcoronabuffer.h), gated on the X360 asm.

namespace renderengine
{
// ---- renderengine::Corona -- THE 64-BYTE CORONA RECORD (DWARF rwgcorona.h:12-18) ---------------
// Pinned at BOTH ends of the console pipeline and confirmed a third time by the DWARF:
//   WRITER  CoronaBuffer::Iterator::Write @0x823F3350 -- three 128-bit stores at +0x00/+0x10/+0x20,
//           `stfs f1,0x30`, `stw r9,0x34` (the colour), `stw r6,0x38` (the texture id), stride 64.
//   READER  CoronaRenderer::Dispatch @0x82404F30 -- position at +0x00 (w overwritten with +0x30),
//           direction at +0x10, size x/y at +0x20/+0x24, the bias at +0x30, the RGBA8 at +0x34, and
//           `*(rec+0x38) << 6` as the atlas UV row index.
//
// The host layout REPRODUCES THE CONSOLE'S EXACTLY, because rw::math::vpu::Vector2/Vector3 are
// 16-byte alignas(16) registers here just as they are VMX registers there:
//   +0x00 mvPosition (16) +0x10 mvDirection (16) +0x20 mvSize (16) +0x30 mfDistance +0x34 muColour
//   +0x38 miTextureID +0x3C (padding; never written by Write, never read by Dispatch)
// -- so the static_assert below is a real check, not a coincidence.
//
// (The previous float-array spelling `mafPosition[4] / mafDirection[4] / mafSize[2] / mafSizePad[2]`
// produced the same bytes but invented two members: the 1a verifier's F1 records that DWARF's
// `size` is ONE 16-byte Vector2 covering +0x20..+0x2F, and that there is no member at +0x3C.)
struct Corona
{
    Vector3 mvPosition;    // +0x00  rwgcorona.h:13  world position (w unused by the writer)
    Vector3 mvDirection;   // +0x10  rwgcorona.h:14  facing direction (w unused)
    Vector2 mvSize;        // +0x20  rwgcorona.h:15  quad half-extents (x, y)
    f32     mfDistance;    // +0x30  rwgcorona.h:16  the depth bias, spliced into position.w
    u32     muColour;      // +0x34  rwgcorona.h:17  DWARF types it RGBA8; the attested writer lays
                           //                        down a 4-byte word, so the record carries the word
    s32     miTextureID;   // +0x38  rwgcorona.h:18  atlas page; row = miTextureID * 4 in s_atlasUVs
};

static_assert(sizeof(Corona) == 64, "renderengine::Corona must match the 64-byte runtime buffer stride");

// ---- renderengine::CoronaBuffer (DWARF rwgcoronabuffer.h:10) -----------------------------------
// A header plus `m_numCoronas` 64-byte records; GetResourceDescriptor sizes the pair as one block.
// Bodies: SDKs/RenderEngineClub/MAIN/components/src/coronas/rwgcoronabuffer.cpp (the two statics)
// and .../rwgcoronabufferiterator.cpp (the attested Iterator::Write @0x823F3350).
class CoronaBuffer
{
public:
    // The five {size, alignment} entries the rw resource allocator consumes
    // (rw::BaseResourceDescriptors<5>, the X360 five-entry form).
    struct ResourceDescriptor5
    {
        struct Entry
        {
            u32 muSize;
            u32 muAlignment;
        };

        Entry maEntries[5];
    };

    // DWARF rwgcoronabuffer.h:13
    struct Parameters
    {
        void SetNumCoronas(int liNumCoronas) { miNumCoronas = liNumCoronas; }   // :14

        int miNumCoronas;   // :16
    };

    // DWARF rwgcoronabuffer.h:20. THREE members (:37/:38/:39) -- the third is load-bearing and was
    // missing from the previous declaration: BrnCoronaManager::Clear (recovered from
    // BrnRendererModule::StartOfFrame @0x823FC160) writes all three,
    //     v11[2] = 0; v11[1] = mpBuffer->m_data; v11[3] = mpBuffer->m_numCoronas;
    // i.e. iterator +0x04 data / +0x08 index / +0x0C count. Without muNumCoronas the class was one
    // word short of what the console's own rewind writes.
    struct Iterator
    {
        // 0x823F3350 -- write one 64-byte corona record and advance the cursor. The DWARF
        // signature (rwgcoronabuffer.h:28) is the PPC ABI's own shape: three vector registers
        // (v1/v2/v3), one FPR (f1) and two GPRs, which is exactly what the asm uses -- Hex-Rays
        // renders it as (const void* 48-byte payload, double, u32, int) because it cannot see the
        // by-value vector parameters.
        void Write(Vector3 lvPosition, Vector3 lvDirection, Vector2 lvSize,
                   f32 lfDistance, rw::RGBA lColour, int liTextureID);

        void SetPosition(u32 luIndex) { muIndex = luIndex; }          // :30
        Corona& operator*()           { return mpData[muIndex]; }     // :32
        void operator++()             { ++muIndex; }                  // :33

        Corona* mpData;        // +0x00 :37
        u32     muIndex;       // +0x04 :38
        u32     muNumCoronas;  // +0x08 :39
    };

    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* pDescriptor,
                                                      Parameters* pParameters);   // :42
    static CoronaBuffer* Initialize(CoronaBuffer** ppBuffer, Parameters* pParameters);   // :43

    void Release() {}                                                             // :44
    u32 GetNumCoronas() const { return muNumCoronas; }                            // :46
    const Corona* GetCoronas() const { return mpData; }                           // :47
    void Lock(Iterator& rIterator)                                                // :49
    {
        rIterator.mpData       = mpData;
        rIterator.muIndex      = 0;
        rIterator.muNumCoronas = muNumCoronas;
    }
    void Unlock() {}                                                              // :50

    u32     muNumCoronas;   // :53
    Corona* mpData;         // :54
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
    static const BrnCoronaTypeParams& GetCoronaTypeParams(BrnCoronaType leType);

private:
    // BrnCoronaManager.h:94 (the DWARF member name is `mParams`; this tree's committed spelling is
    // smParams -- kept, and recorded here so the two are not mistaken for different members).
    // Defined where GetCoronaTypeParams is bodied (group `coronadata`).
    static BrnCoronaTypeParams smParams[eCoronaTypeCount];
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
        void SetCameraInfo(const Matrix44& lViewProj, const Vector3& lCameraPosition, const Vector4& lViewXyScale);

        // BrnCoronaManager.h:126 / asm @ 0x823FD138. Bodied in the .cpp.
        void AddPropCorona(const Vector3& lvPosition, const Vector3& lvDirection, const Vector2& lvSize,
                            f32 lfAlpha, int32_t liTextureID);

        // BrnCoronaManager.h:134 / asm @ 0x823FD270. Bodied in the .cpp.
        void AddCorona(const Vector3& lvPosition, const Vector3& lvDirection, f32 lfScale,
                        f32 lfOpacity, const BrnCoronaTypeParams& lCoronaTypeParams);

        // BrnCoronaManager.h:142 / asm @ sub_823FD428 -- the one-line forwarder that indexes the
        // archetype table by type. Bodied in the .cpp.
        void AddCorona(const Vector3& lvPosition, const Vector3& lvDirection, f32 lfScale,
                        f32 lfOpacity, const BrnCoronaType& leCoronaType);

        // BrnCoronaManager.h:159. Not X360-attested for this TU -- declared only.
        const BrnCoronaTypeParams& GetTypeParams(BrnCoronaType leType);

        // [FLAG PC bring-up] SEAM S3 (group `coronaproducer`). NOT an X360 function: on the console
        // the manager is always Constructed before any producer runs, so a producer may write
        // unconditionally. On PC the manager comes up lazily (it needs a live D3D9 device -- see
        // BrnRendererModule's EnsureCoronaManagerBringUp), so a producer that ran a frame early
        // would write through a null mpData. This is the ONE question a producer has to be able to
        // ask. DELETE-WHEN the manager is Constructed at the console's own point in Prepare.
        bool IsReady() const { return mpBuffer != 0 && mBufferIterator.mpData != 0; }

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

    // [FLAG PC bring-up] Has Construct run? The console never needs to ask (Prepare runs it before
    // anything reads); on PC the manager is built lazily, so Render and the producers do.
    bool IsConstructed() const { return m_coronaBuffer0 != 0 && m_coronaBuffer1 != 0; }

    // =============================================================================================
    // [FLAG PC bring-up] Publish the render camera into the interface Render is ABOUT TO READ.
    //
    // NOT an X360 function. The console publishes through BrnSubmissionInterface::SetCameraInfo from
    // BrnRendererModule::Update @0x82405EC4-0x82405FA0, into the interface at
    // mu8SubmissionSwapIndex -- i.e. the WRITE side, one Swap before Render reads it. Two facts make
    // that unreachable on this build, and both are recorded rather than worked around silently:
    //   1. THE CADENCE. BrnRendererModule::Update has exactly ONE call site on this build --
    //      BrnGameModule::GamePrepare's not-done tail (BrnGameModule.cpp:2647) -- so it runs while
    //      the game is LOADING and never once per rendered frame. A camera published there would be
    //      frozen at whatever the last GamePrepare pass saw.
    //   2. THE SOURCE. Update's version reads its camera out of the RendererIO INPUT buffer
    //      (`lpInput->GetBrnCamera()`), and that buffer is created empty in that same call site --
    //      nothing on this build fills it, because the dispatch IO buffer set is not real (the same
    //      deferral BrnGameModule.cpp:2622 and BrnRendererModule::Update's own [FLAG] record).
    //      (⚠ NOT a link blocker, and an earlier draft of this banner wrongly said it was:
    //       GameSource/Director/Camera/Camera.cpp IS on tools/build/build_game_exe.bat, line 394,
    //       and it bodies GetFOV :179 / CopyToCgsCamera :508 / GetPosition :562. The console's three
    //       loads are LINKABLE today; they simply have no camera to read.)
    // So the renderer publishes the live dispatch camera straight into the READ side, immediately
    // before Render. That is one Swap later than the console -- which makes the camera one frame
    // FRESHER than the console's, never staler -- and it is the same object, written through the
    // same SetCameraInfo. The offset knowledge (which of the two interfaces Render reads) stays
    // inside this class rather than leaking to the caller.
    //
    // DELETE-WHEN Camera.cpp is mounted and Update runs per frame: then the publish moves back into
    // Update, into GetSubmissionInterface()'s slot, and this method goes.
    // =============================================================================================
    void PCBringUpSetRenderCamera(const Matrix44& lViewProj, const Vector3& lCameraPosition,
                                   const Vector4& lViewXyScale);

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
