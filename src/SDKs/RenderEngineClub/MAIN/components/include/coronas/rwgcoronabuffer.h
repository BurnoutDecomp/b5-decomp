#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

// [FIXED 2026-08-17, carlights step 1, group `coronas`] This header did not compile AT ALL at
// HEAD: it declared `namespace rw { typedef u32 RGBA; }`, while the include above reaches
// rw/rwcore_structs.h, which defines `struct rw::RGBA` (4 bytes, with the m_rgba word and the
// four-byte ctor RwRGBA.cpp bodies). Measured:
//     cl /c ... -> error C2371: 'rw::RGBA': redefinition; different basic types
//                  rwcore_structs.h(259): see declaration of 'rw::RGBA'
// It went unnoticed because the header has no includers, so nothing ever compiled it. The typedef
// is deleted; the real struct is used, and the one assignment into the corona record now takes
// .m_rgba explicitly.
namespace rw
{
namespace math
{
struct Vector2
{
    float mfX;
    float mfY;
};

struct Vector3
{
    float mfX;
    float mfY;
    float mfZ;
};
}
}

namespace renderengine
{
struct ResourceDescriptor5
{
    struct Entry
    {
        u32 muSize;
        u32 muAlignment;
    };

    Entry maEntries[5];
};

// THE 64-BYTE CORONA RECORD. Layout corrected 2026-08-17 (carlights step 1, group `coronas`)
// against its two ends -- the WRITER, renderengine::CoronaBuffer::Iterator::Write @0x823F3350
// (rwgcoronabufferiterator.cpp: 48 bytes of caller payload at +0x00, `stfs f1,0x30`, the
// byte-swapped colour `stw r9,0x34`, `stw r6,0x38`), and the READER,
// renderengine::CoronaRenderer::Dispatch<shadow::Device> @0x82404F30, which per record does
//     lvx v0,r31        -> +0x00 position (the w lane is overwritten with the +0x30 bias)
//     _R31 + 16         -> +0x10 direction, passed to the vertex writer as a Vector3
//     *(_R31 + 32/36)   -> +0x20/+0x24 size x/y (the quad's half-extents, sign-flipped per corner)
//     *(_R31 + 48)      -> +0x30 the bias distance, splatted into the position's w lane
//     _R31 + 52         -> +0x34 the RGBA8 colour handed to the vertex writer
//     *(_R31 + 56)      -> +0x38 the texture id, << 6 into the atlas UV table
//
// The three previous fields at +0x28/+0x30/+0x34 (`mu64ColourAndDistance`, `mfDistance`,
// `miTextureID`) were off by one slot each: what sat at +0x34 is the COLOUR, and what sat at
// +0x38 is the TEXTURE ID. Nothing consumed them yet, so this is a rename+re-lane, not a
// behaviour change -- but MakeDefaultCorona below was seeding `miTextureID = -1` into what is
// really the colour word (which is right: the console's template colour IS 0xFFFFFFFF) while
// leaving the real texture id at its console value of 0.
struct Corona
{
    float mafPosition[4];    // +0x00  world position (w unused by the writer)
    float mafDirection[4];   // +0x10  facing direction (w unused)
    float mafSize[2];        // +0x20  quad half-extents x/y
    float mafSizePad[2];     // +0x28  written by the writer's third VMX store, unread
    float mfBiasDistance;    // +0x30  depth bias, becomes position.w in the vertex stream
    u32   muColour;          // +0x34  RGBA8 (byte-swapped by Iterator::Write)
    s32   miTextureID;       // +0x38  atlas page index; row = miTextureID * 64 in s_atlasUVs
    u32   muUnused3C;        // +0x3C  never written by Write, never read by Dispatch
};

static_assert(sizeof(Corona) == 64, "Corona must match runtime buffer stride");

class CoronaBuffer
{
public:
    struct Parameters
    {
        void SetNumCoronas(int liNumCoronas);

        int miNumCoronas;
    };

    struct Iterator
    {
        void Write(const Corona& rCorona);
        void Write(
            rw::math::Vector3 lPosition,
            rw::math::Vector3 lDirection,
            rw::math::Vector2 lSize,
            float lfDistance,
            rw::RGBA lColour,
            int liTextureID);
        void SetPosition(u32 luIndex);
        Corona& operator*();
        void operator++();

        Corona* mpData;
        u32     muIndex;
        u32     muNumCoronas;
    };

    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* pDescriptor, Parameters* pParameters);
    static CoronaBuffer* Initialize(CoronaBuffer** ppBuffer, Parameters* pParameters);

    void Release();
    u32 GetNumCoronas() const;
    const Corona* GetCoronas() const;
    void Lock(Iterator& rIterator);
    void Unlock();

    u32     muNumCoronas;
    Corona* mpData;
};

class CoronaRenderer
{
public:
    static void Initialize(void* pResourceAllocator);
};
}

// =================================================================================================
// [FLAG BLOCKED -- DUPLICATE TYPE, NOT HOST-VALID] (updated 2026-08-17, carlights step 1, `coronas`)
//
// This is a SECOND declaration of the global-namespace `BrnCoronaManager`, whose real home is
// GameSource/Graphics/BrnCoronaManager.h (it owns AddCorona / AddPropCorona / SetTextureAtlas and
// the DWARF member set). The two are never `#include`d together today -- this header has NO
// includers anywhere in the tree:
//     $ grep -rn "rwgcoronabuffer.h" b5-decomp/src        # only comment mentions, no #include
// -- so there is no live ODR conflict, but this file cannot be mounted until it is merged away.
//
// THE MERGE IS NOW FULLY SPECIFIED. Every member below is the SAME member as the real class; the
// X360 offsets were measured this wave from three functions and they agree exactly with the real
// header's DWARF order:
//     BrnCoronaManager::Render      @0x824075D8  `lbz r11,0(r3)` (mbActive) / `lwz r11,8(r3)`
//                                   (m_textureStateAtlas) / `lbz r11,0x130(r3)` (swap index) /
//                                   `mulli r11,r11,0x70` + `addi r31,r11,0x50`
//                                   (mSubmissionInterface[2], base +0x50, stride 0x70)
//     BrnRendererModule::Update     @0x82405F60-74 and @0x824060F8-104  (same expression twice)
//     BrnCoronaManager::SetTextureAtlas @0x823FD000 (`a1+20` == m_textureStateAtlasResource)
//   +0x00 mbActive(bool) +0x04 m_textureAtlas +0x08 m_textureStateAtlas +0x0C m_coronaBuffer0
//   +0x10 m_coronaBuffer1 +0x14/+0x28/+0x3C the three rw::Resources (5 guest words each)
//   +0x50 mSubmissionInterface[2] (0x70 each) +0x130 mu8SubmissionSwapIndex; sizeof == 0x134.
//
// *** THE MEMBER OFFSETS BELOW ARE GUEST OFFSETS AND DO NOT HOLD ON THIS x64 HOST *** -- the
// pointers are 4 bytes on the console and 8 here, and rw::Resource is 20 bytes there and 32 here,
// so this declaration reproduces the console ORDER and ROLES only. Nothing may read it by offset.
// The merge is: delete this class, `#include "GameSource/Graphics/BrnCoronaManager.h"`, and define
// BrnCoronaManager::Construct against the real members (which requires growing that header's
// `renderengine::CoronaBuffer` with the Parameters/statics declared above -- a 52-TU includer
// closure that must be gated, see this wave's report).
//
// The member NAMES were corrected in place this wave, because four of them were not just
// differently spelled but wrong about what the field is: `muConstructed` is mbActive,
// `muActiveCoronaCount` is m_textureStateAtlas (Construct clears it, SetTextureAtlas fills it),
// and the two `*BufferMirror` fields are the two BrnSubmissionInterface::mpBuffer slots -- which is
// what makes `*(a1+80)` / `*(a1+192)` mean "publish each buffer to its submission interface"
// rather than "keep a spare copy".
// =================================================================================================
class BrnCoronaManager
{
public:
    renderengine::CoronaBuffer* Construct(void* pResourceAllocator);

private:
    u32                         muConstructed;              // +0x00  real: bool mbActive
    u32                         muTextureAtlas;             // +0x04  real: renderengine::Texture* m_textureAtlas
    u32                         muTextureStateAtlas;        // +0x08  real: renderengine::TextureState* m_textureStateAtlas
    renderengine::CoronaBuffer* mpCoronaBuffer0;            // +0x0C  real: m_coronaBuffer0
    renderengine::CoronaBuffer* mpCoronaBuffer1;            // +0x10  real: m_coronaBuffer1
    u8                          maTextureStateAtlasResource[20];  // +0x14  real: rw::Resource
    renderengine::CoronaBuffer* mapCoronaBuffer0Resource[5];      // +0x28  real: rw::Resource
    renderengine::CoronaBuffer* mapCoronaBuffer1Resource[5];      // +0x3C  real: rw::Resource
    renderengine::CoronaBuffer* mpSubmissionInterface0Buffer;     // +0x50  real: mSubmissionInterface[0].mpBuffer
    u8                          maSubmissionInterface0Rest[108];  // +0x54  rest of interface 0
    renderengine::CoronaBuffer* mpSubmissionInterface1Buffer;     // +0xC0  real: mSubmissionInterface[1].mpBuffer
    u8                          maSubmissionInterface1Rest[108];  // +0xC4  rest of interface 1
    u32                         muSubmissionSwapIndex;            // +0x130 real: u8 mu8SubmissionSwapIndex
};

namespace
{
class ResourceAllocator
{
public:
    // NOT a vtable slot. Declaring this `virtual` put it at slot 0, which on the
    // rw::IResourceAllocator actually behind the reinterpret_cast is the VIRTUAL
    // DESTRUCTOR -- so the call allocated nothing and left the allocator's vptr
    // downgraded to the inert base for the rest of the run. Call the interface by
    // NAME instead; see CgsResourceAllocatorCreate.h.
    void* Create(
        void* pOut,
        ResourceAllocator* /*pAllocator*/,
        renderengine::ResourceDescriptor5* pDescriptor,
        int /*liFlags*/)
    {
        return CgsGraphics::ResourceAllocatorCreate(this, pOut, pDescriptor);
    }
};

// CORRECTED 2026-08-17 (carlights step 1, group `coronas`). These six globals were read as a
// three-deep buffer shift-chain (`gpCoronaBuffer0 = gpCoronaBuffer1; gpCoronaBuffer1 =
// gpCoronaBuffer2; gpCoronaBuffer2 = 0`). THERE IS NO SHIFT CHAIN. The X360 body
// (@0x823FCDC8-0x823FCDF8) copies THREE DIFFERENT device-state-factory globals into three
// corona-owned slots, and both consumers name what they are:
//
//   dword_82FAB6C0 = dword_83010F78 == saBlendStates[2]
//        == E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA
//        (CgsBlendStateFactory.h -- its reader census names this very load,
//         "BrnCoronaManager::Construct -> +0x08 = slot 2 (@0x823FCDD8)").
//        CONSUMER: CoronaRenderer::Begin<shadow::Device> @0x823FF2C0 ->
//        shadow::Device::Xbox2SetStateLowLevelShadowed(dword_82FAB6C0, ...).
//        *** THIS IS THE CONSOLE'S CORONA BLEND STATE: ADDITIVE, NO ALPHA TEST. ***
//   dword_82FAB6C4 = dword_83010914 == saDepthStencilStates[2]
//        == E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF
//   dword_82FAB6C8 = dword_83010910 == saDepthStencilStates[1]
//        == E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF
//        (CgsDepthStencilStateFactory.h's enum + its reader census, which lists
//         "0x823FCD90 BrnCoronaManager::Construct  reader" against dword_83010910.)
//        CONSUMER: CoronaRenderer::Dispatch<shadow::Device> @0x82404F30 picks between them --
//        `if (BatchParameters.muFlags & 1) state = dword_82FAB6C4; else state = dword_82FAB6C8;`
//        and BrnCoronaManager::Render @0x824076AC always passes muFlags = 1, so the live corona
//        pass is DEPTH-TESTED (LEQ) ADDITIVE WITH NO DEPTH WRITE.
//
//   dword_82FAB6BC = 0   -- the atlas TextureState slot, cleared here, filled by SetTextureAtlas.
//   dword_82FAB6A8 = 1   -- the render-state word SetTextureAtlas later sets to 4.
//   dword_82FAB6B8 = &unk_82FAFC10 -- NOT a vtable: the ATLAS UV TABLE pointer
//                                     (&BrnCoronaManager::s_atlasUVs[0]); see BrnCoronaManager.cpp.
//
// [FLAG] The three source globals are the state factories' tables, which this tree now owns by
// name (CgsBlendStateFactory::GetState / CgsDepthStencilStateFactory::GetState). When this file
// is rewritten against the real class, these captures must read THOSE accessors, not a local copy.
void* gpCoronaBlendStateAdditive;          // X360 dword_82FAB6C0 <- saBlendStates[2]
void* gpCoronaDepthStateZTestNoWrite;      // X360 dword_82FAB6C4 <- saDepthStencilStates[2]
void* gpCoronaDepthStateNoZTest;           // X360 dword_82FAB6C8 <- saDepthStencilStates[1]
void* gpCoronaAtlasTextureState;           // X360 dword_82FAB6BC (cleared here)
u32   guCoronaRenderStateWord;             // X360 dword_82FAB6A8 (1 here, 4 in SetTextureAtlas)
// [FLAG BLOCKED] s_atlasUVs' 768 bytes (X360 unk_82FAFC10) are not in any export we hold -- see
// the BLOCKED DATA block at the top of GameSource/Graphics/BrnCoronaManager.cpp. The previous
// spelling was `extern u8 gCoronaVTable_82FAFC10;` inside this anonymous namespace with NO
// definition anywhere -- a second reason this header could not compile (C7631: a variable with
// internal linkage was declared but not defined). Held as a null pointer until the table lands.
void* gpCoronaAtlasUVTable;                // X360 dword_82FAB6B8 <- &s_atlasUVs[0]

// The 64-byte template Construct stamps into all 2 x 512 records (X360 @0x823FCE4C-0x823FCEE8:
// v38 = {0,0,0,0}, v39 = {0,0,1,0}, v40 = {1,1,0,0}, v41 = 1.0f @+0x30, v42 = -1 @+0x34,
// v43 = 0 @+0x38). Re-laned 2026-08-17 with the corrected Corona record: the -1 is the COLOUR
// (0xFFFFFFFF, opaque white), and the texture id is 0, not -1.
// (+0x3C is never assigned by the console either -- it copies whatever the stack held. Zeroed
// here: Dispatch never reads it, so a defined zero is the safe reproduction of an indeterminate
// byte, and it is called out rather than passed off as attested.)
renderengine::Corona MakeDefaultCorona()
{
    renderengine::Corona lCorona = {};

    lCorona.mafDirection[2] = 1.0f;
    lCorona.mafSize[0] = 1.0f;
    lCorona.mafSize[1] = 1.0f;
    lCorona.mfBiasDistance = 1.0f;
    lCorona.muColour = 0xFFFFFFFFu;
    lCorona.miTextureID = 0;
    return lCorona;
}

void FillDefaultCoronas(renderengine::CoronaBuffer* pBuffer, const renderengine::Corona& lCorona)
{
    for (u32 luIndex = 0; luIndex < pBuffer->muNumCoronas; ++luIndex)
        pBuffer->mpData[luIndex] = lCorona;
}

void AllocateCoronaBufferHandles(
    ResourceAllocator* pAllocator,
    renderengine::CoronaBuffer** papHandles,
    renderengine::ResourceDescriptor5* pDescriptor)
{
    renderengine::CoronaBuffer* lapAllocatedHandles[5] = {};
    pAllocator->Create(lapAllocatedHandles, pAllocator, pDescriptor, 0);

    for (int liHandle = 0; liHandle < 5; ++liHandle)
        papHandles[liHandle] = lapAllocatedHandles[liHandle];
}
}

inline void renderengine::CoronaBuffer::Parameters::SetNumCoronas(int liNumCoronas)
{
    miNumCoronas = liNumCoronas;
}

inline void renderengine::CoronaBuffer::Iterator::Write(const Corona& rCorona)
{
    mpData[muIndex] = rCorona;
    ++muIndex;
}

inline void renderengine::CoronaBuffer::Iterator::Write(
    rw::math::Vector3 lPosition,
    rw::math::Vector3 lDirection,
    rw::math::Vector2 lSize,
    float lfDistance,
    rw::RGBA lColour,
    int liTextureID)
{
    Corona& rCorona = mpData[muIndex];
    rCorona.mafPosition[0] = lPosition.mfX;
    rCorona.mafPosition[1] = lPosition.mfY;
    rCorona.mafPosition[2] = lPosition.mfZ;
    rCorona.mafDirection[0] = lDirection.mfX;
    rCorona.mafDirection[1] = lDirection.mfY;
    rCorona.mafDirection[2] = lDirection.mfZ;
    rCorona.mafSize[0] = lSize.mfX;
    rCorona.mafSize[1] = lSize.mfY;
    rCorona.mfBiasDistance = lfDistance;
    rCorona.muColour = lColour.m_rgba;
    rCorona.miTextureID = liTextureID;
    ++muIndex;
}
// [FLAG UNATTESTED] Neither of the two Write overloads above has an X360 address. The ONLY
// attested writer is renderengine::CoronaBuffer::Iterator::Write(const void*, f64, u32, int)
// @0x823F3350 -- bodied in ../src/coronas/rwgcoronabufferiterator.cpp and declared at the shape
// its caller uses in GameSource/Graphics/BrnCoronaManager.h:63. These two are a convenience
// surface this header invented; their FIELD LANES were corrected with the Corona record above,
// but they should be deleted, not grown, when this header is merged into the real one.

inline void renderengine::CoronaBuffer::Iterator::SetPosition(u32 luIndex)
{
    muIndex = luIndex;
}

inline renderengine::Corona& renderengine::CoronaBuffer::Iterator::operator*()
{
    return mpData[muIndex];
}

inline void renderengine::CoronaBuffer::Iterator::operator++()
{
    ++muIndex;
}

inline void renderengine::CoronaBuffer::Release()
{
}

inline u32 renderengine::CoronaBuffer::GetNumCoronas() const
{
    return muNumCoronas;
}

inline const renderengine::Corona* renderengine::CoronaBuffer::GetCoronas() const
{
    return mpData;
}

inline void renderengine::CoronaBuffer::Lock(Iterator& rIterator)
{
    rIterator.mpData = mpData;
    rIterator.muIndex = 0;
    rIterator.muNumCoronas = muNumCoronas;
}

inline void renderengine::CoronaBuffer::Unlock()
{
}

inline renderengine::CoronaBuffer* BrnCoronaManager::Construct(void* pResourceAllocator)
{
    ResourceAllocator* lpAllocator = static_cast<ResourceAllocator*>(pResourceAllocator);
    renderengine::ResourceDescriptor5 lDescriptor;
    renderengine::CoronaBuffer::Parameters lParameters;

    muConstructed = 1;                       // X360 `*a1 = 1` -> the real member is mbActive
    renderengine::CoronaRenderer::Initialize(pResourceAllocator);

    // @0x823FCDC8-0x823FCDF8, in the asm's order. See the banner above the globals for the
    // identification of all six stores.
    // [FLAG BLOCKED] The three state captures have no source in scope from this header:
    //   gpCoronaBlendStateAdditive     must become CgsBlendStateFactory::GetState(
    //         E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA)      -- but that
    //         accessor is declared NON-static (CgsBlendStateFactory.h: "STATICNESS UNDETERMINED"),
    //         so it needs the renderer's factory instance, which this header cannot reach;
    //   gpCoronaDepthStateZTestNoWrite must become CgsDepthStencilStateFactory::GetState(
    //         E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF)                        -- static,
    //   gpCoronaDepthStateNoZTest      must become CgsDepthStencilStateFactory::GetState(
    //         E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF)                       -- static.
    // Left null rather than pointed at a fabricated state object: nothing consumes them yet
    // (CoronaRenderer::Begin/Dispatch are `todo`), and a wrong blend state is exactly the class of
    // silent visual defect this campaign keeps paying for. The previous body assigned a
    // three-deep buffer shift-chain that is not in the binary at all.
    gpCoronaBlendStateAdditive     = nullptr;   // <- saBlendStates[2]
    gpCoronaDepthStateZTestNoWrite = nullptr;   // <- saDepthStencilStates[2]
    gpCoronaDepthStateNoZTest      = nullptr;   // <- saDepthStencilStates[1]
    gpCoronaAtlasTextureState      = nullptr;   // X360 dword_82FAB6BC = 0
    guCoronaRenderStateWord        = 1;         // X360 dword_82FAB6A8 = 1
    gpCoronaAtlasUVTable           = nullptr;   // X360 dword_82FAB6B8 = &s_atlasUVs[0], BLOCKED

    lParameters.SetNumCoronas(512);
    renderengine::CoronaBuffer::GetResourceDescriptor(&lDescriptor, &lParameters);

    // Two allocations from the SAME descriptor -- one resource per corona buffer (X360 calls the
    // allocator twice at @0x823FCE10 and @0x823FCE24, into `a1+40` and `a1+60`).
    AllocateCoronaBufferHandles(lpAllocator, mapCoronaBuffer0Resource, &lDescriptor);
    AllocateCoronaBufferHandles(lpAllocator, mapCoronaBuffer1Resource, &lDescriptor);

    mpCoronaBuffer0 = renderengine::CoronaBuffer::Initialize(mapCoronaBuffer0Resource, &lParameters);
    mpCoronaBuffer1 = renderengine::CoronaBuffer::Initialize(mapCoronaBuffer1Resource, &lParameters);

    const renderengine::Corona lDefaultCorona = MakeDefaultCorona();
    FillDefaultCoronas(mpCoronaBuffer0, lDefaultCorona);
    FillDefaultCoronas(mpCoronaBuffer1, lDefaultCorona);

    // @0x823FCEF4-0x823FCF08. `*(a1+304) = 0` is mu8SubmissionSwapIndex; `*(a1+8) = 0` is
    // m_textureStateAtlas (there is no atlas until PrepareAgain -> SetTextureAtlas runs); and
    // `*(a1+80)` / `*(a1+192)` are mSubmissionInterface[0].mpBuffer / [1].mpBuffer -- i.e. each
    // buffer is PUBLISHED to its submission interface, which is how AddCorona/AddPropCorona reach
    // it later. (The previous names read these as spare "mirror" copies.)
    muSubmissionSwapIndex       = 0;
    muTextureStateAtlas         = 0;
    mpSubmissionInterface0Buffer = mpCoronaBuffer0;
    mpSubmissionInterface1Buffer = mpCoronaBuffer1;

    return mpCoronaBuffer1;
}
