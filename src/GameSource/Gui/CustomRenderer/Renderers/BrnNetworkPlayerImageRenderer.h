#ifndef BRN_NETWORK_PLAYER_IMAGE_RENDERER_H
#define BRN_NETWORK_PLAYER_IMAGE_RENDERER_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                                   // CgsID == u64 (GetID return)
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"    // CgsGui::CustomRenderComponentInterface base
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                              // CgsGui::GuiEventQueueSmall (a queue typedef)
#include "pc/gcm/renderengine/texture.h"                                         // renderengine::Texture / Texture2D / Texture::Locked
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                            // BrnGui::GuiEventNetworkPlayerImage (id 258) -- the canonical home

// ============================================================================
// GameSource/Gui/CustomRenderer/Renderers/BrnNetworkPlayerImageRenderer.h
//
// BrnGui::NetworkPlayerImageRenderer - the GUI custom-render component that
// receives transmitted player "mugshot" images (NetworkTextures) over the
// network, unpacks them into a small triple-buffered set of GPU textures, and
// exposes the current frame's texture to the Flapt (Flash) GUI as a special
// component texture. Reconstructed from BURNOUT_X360_ARTIST.XEX with the DecFIGS
// DWARF (BrnNetworkPlayerImageRenderer.h/.cpp) as the structural authority.
//
// X360 member layout (this == r3), recovered from the function bodies
// (Construct @0x82445A50, Prepare @0x82451560, CopyTexture @0x82445F30,
// GetRenderOutput @0x82445CC0, ClearTextures @0x824460E0, Release @0x82445B28):
//   +0x000  vptr                       (base CgsGui::CustomRenderComponentInterface)
//   +0x004  mbRenderEnabled            (base member)
//   +0x008  mpDefaultTexture
//   +0x00C  maapTextureBuffer[3][3]     (Texture2D*)        -> ends +0x030
//   +0x030  maaLockedTextures[3][3]     (Texture::Locked,28)-> ends +0x12C
//   +0x12C  mapCompressedTextureBuffer[3]                   -> ends +0x138
//   +0x138  maLockedCompressedTextures[3]                   -> ends +0x18C
//   +0x18C  maapYUY2TextureBuffer[3][3]                     -> ends +0x1B0
//   +0x1B0  maaLockedYUY2Textures[3][3]                     -> ends +0x2AC
//   +0x2AC  maabRenderTexture[3][3]     (bool)              -> ends +0x2B5
//   +0x2B5  maabRenderCompressedTexture[3][3]               -> ends +0x2BE
//   +0x2BE  maabRenderYUY2Texture[3][3]                     -> ends +0x2C7
//   +0x2C8  mePrepareStage
//   +0x2CC  meReleaseStage
//   +0x2D0  mpHeapAllocator
//   +0x2D4  mpTextureAllocator
//   +0x2D8  mpGuiCache
//   +0x2DC  mpFlaptRenderer
//   +0x2E0  miCurrentRenderTexture
//   +0x2E4  miCurrentCopyToTexture
//   +0x2E8  miClearTexturesFrameCount
//   +0x2EC  mbRenderTexture
//   +0x2ED  mbUseDefaultTexture
//
// Member access is BY NAME on both the X360 32-bit guest and the PC x64 host
// (pointers widen so the byte offsets shift, but every store/load in the .cpp
// is a named member, never a raw offset). The DWARF declared only the normal +
// compressed buffers; the asm + the Prepare/CopyTexture assert strings prove the
// additional YUY2 buffer/locked arrays interleaved before the bool flag arrays,
// so they are modelled here (FLAG: maapYUY2TextureBuffer / maaLockedYUY2Textures
// are asm-attested, not DWARF-listed).
// ============================================================================

// Forward declarations for the polymorphic-interface parameter types. These are
// passed/returned by pointer only; their full layout is not needed by this TU.
// (GuiEventQueueSmall is a queue typedef pulled in via CgsGuiEvent.h above.)
namespace CgsGui { struct ImRendererSet; }
namespace CgsModule { struct Event; }
namespace rw { struct IResourceAllocator; }
namespace BrnFlapt { struct FlaptRenderer; }
namespace CgsNetwork { class NetworkTexture; }

namespace BrnGui
{
    class GuiCache;

    // ⭐ FORK RETIRED 2026-08-29 (map-world wave). `BrnGui::GuiEventNetworkPlayerImage` used
    // to be re-defined HERE, at namespace scope, while its canonical home already existed at
    // GameSource/Gui/BrnGuiDemangledEventTypes.h:172. Two namespace-scope definitions of one
    // class are an ODR fork that only a co-including TU finds -- and it made this header a
    // hard C2011 against every TU that pulls the demangled home, which is why
    // BrnCrashNavIconRenderer.h (and therefore the CrashNavIcon component) could not be
    // embedded in BrnCustomRendererManager.h. Three TUs already recorded the fork and named
    // exactly this fix (GameBridgeGameStateToX.cpp:56,
    // GameBridgeGameStateToX_EventFlowGuiEvents.cpp:81, BrnCrashNavIconRenderer_wK_01.cpp:83).
    // The demangled home now carries the same two typed fields this definition had, so the
    // renderer's CopyTexture reads the identical record.

    class NetworkPlayerImageRenderer : public CgsGui::CustomRenderComponentInterface
    {
    public:
        // DWARF BrnNetworkPlayerImageRenderer.h:56
        static const s32 KI_MAX_NUM_TEXTURES_TO_DISPLAY = 3;

    private:
        // .rdata constants (DWARF BrnNetworkPlayerImageRenderer.cpp:31..37).
        static const u32 KU_TEXTURE_WIDTH = 160;
        static const u32 KU_TEXTURE_HEIGHT = 120;
        static const s32 KI_INITIAL_COPY_TO_TEXTURE = 2;
        static const s32 KI_INITIAL_RENDER_FROM_TEXTURE = 1;
        static const s32 KI_CLEAR_TEXTURES_NOT_SET = -1;     // 0xFFFFFFFF
        static const s32 KI_NUM_TEXTURES_TO_BUFFER = 3;      // DWARF h:127

        // Prepare staging state machine (DWARF h:58).
        enum EPrepareStage
        {
            E_PREPARESTAGE_START                = 0,
            E_PREPARESTAGE_TEXTURES             = 1,
            E_PREPARESTAGE_LOAD_DEFAULT_TEXTURE = 2,
            E_PREPARESTAGE_INIT_DEFAULT_TEXTURE = 3,
            E_PREPARESTAGE_DONE                 = 4,
        };

        // Release staging state machine (DWARF h:67).
        enum EReleaseStage
        {
            E_RELEASESTAGE_START    = 0,
            E_RELEASESTAGE_TEXTURES = 1,
            E_RELEASESTAGE_DONE     = 2,
        };

    public:
        // Construct @ 0x82445A50 : chain the base construct then zero every buffer /
        // locked descriptor / render flag and seed the buffer-index + frame-count state.
        virtual void Construct();

        // SetFlaptRenderer @ 0x82445C... (DWARF cpp:121) : latch the Flapt renderer the
        // SwapBuffers pass pushes the special-texture shader program to.
        void SetFlaptRenderer(BrnFlapt::FlaptRenderer* lpFlaptRenderer);

        // Prepare @ 0x82451560 : the multi-stage async prepare -- allocate the triple
        // buffer of normal / YUY2 / compressed textures, register the default texture
        // resource, then load+init it. Returns true once E_PREPARESTAGE_DONE is reached.
        virtual bool Prepare(CgsGui::GuiEventQueueSmall* lpEventQueue,
                             rw::IResourceAllocator* lpHeapAllocator,
                             rw::IResourceAllocator* lpTextureAllocator);

        // Release @ 0x82445B28 : the staged teardown -- destruct every buffered texture
        // and reset the buffer-index + frame-count state.
        virtual bool Release();

        // Destruct @ 0x82445C58 : chain the base destruct and re-seed the buffer-index +
        // frame-count state.
        virtual void Destruct();

        // RecvEvent @ 0x82449CA0 : dispatch a GUI module event -- bind the GuiCache
        // (E_GUI_CACHE), copy a new player image / arm a texture clear
        // (E_NETWORK_PLAYER_IMAGE), or force the default texture (E_USE_DEFAULT).
        virtual void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType);

        // Update @ 0x82449C80 : count down the arm-to-clear timer and, on expiry, clear
        // the texture surfaces.
        virtual void Update();

        // GetID @ 0x82445CA8 : the renderer's CgsID (constant component id).
        virtual CgsID GetID() const;

        // GetRenderOutput @ 0x82445CC0 (vtable "GetComponentTexture" slot) : pick the
        // texture to display for liTextureIndex -- the default texture, or the current
        // normal / compressed / YUY2 buffer entry; writes the shader-select flag through
        // lpiShaderProgram (1 => YUY2 path).
        virtual renderengine::Texture* GetRenderOutput(s32 liTextureIndex,
                                                       s32* lpiShaderProgram,
                                                       CgsGui::ImRendererSet* lpRendererSet);

        // SwapBuffers @ 0x82445E58 : advance the render / copy-to buffer indices (mod 3),
        // re-bind the now-current frame as the Flapt special texture and push its shader.
        void SwapBuffers();

        // GetNumTextures @ 0x82445C... (DWARF cpp:725) : the display texture count.
        virtual s32 GetNumTextures() const;

    private:
        // RenderComponent @ 0x82445C... (DWARF cpp:507) : per-component render hook (no-op
        // for this renderer; the Flapt movie consumes the special texture instead).
        virtual void RenderComponent(CgsGui::ImRendererSet* lpRendererSet);

        // CopyTexture @ 0x82445F30 : unpack lpEvent's NetworkTexture into the
        // liCopyToTexture slot, routing by pixel format (DXT1 -> compressed buffer,
        // G8B8 -> YUY2 buffer, else -> normal buffer) and arming the matching render flag.
        void CopyTexture(const GuiEventNetworkPlayerImage* lpEvent, s32 liCopyToTexture);

        // ClearTextures @ 0x824460E0 : zero every locked surface (normal + YUY2 +
        // compressed) of the whole buffer set.
        void ClearTextures();

        // SetClearTextures @ 0x82445... (DWARF cpp:738) : arm the clear timer.
        void SetClearTextures();

        // PrepareDefaultTexture @ 0x824461A0 : fetch the baked default texture from the
        // GuiCache into mpDefaultTexture.
        void PrepareDefaultTexture();

        // --- state (X360 layout above) -----------------------------------------
        // Array index order is [displayIndex][bufferSlot]: dim0 = which displayed
        // player image (0..KI_MAX_NUM_TEXTURES_TO_DISPLAY), dim1 = the triple-buffer
        // slot (0..KI_NUM_TEXTURES_TO_BUFFER) selected by miCurrentRender/CopyTexture.
        // Both extents are 3; the X360 row stride is 3 (the asm computes 3*displayIndex
        // + bufferSlot for the flag/locked arrays). The compressed buffer is keyed by
        // displayIndex only (single-buffered).
        renderengine::Texture2D* mpDefaultTexture;                              // +0x008

        renderengine::Texture2D* maapTextureBuffer[KI_MAX_NUM_TEXTURES_TO_DISPLAY]   // +0x00C
                                                  [KI_NUM_TEXTURES_TO_BUFFER];
        renderengine::Texture::Locked maaLockedTextures[KI_MAX_NUM_TEXTURES_TO_DISPLAY] // +0x030
                                                       [KI_NUM_TEXTURES_TO_BUFFER];

        renderengine::Texture2D* mapCompressedTextureBuffer[KI_MAX_NUM_TEXTURES_TO_DISPLAY];    // +0x12C
        renderengine::Texture::Locked maLockedCompressedTextures[KI_MAX_NUM_TEXTURES_TO_DISPLAY]; // +0x138

        // asm-attested (not DWARF-listed): the YUY2/G8B8 unpack buffer set.
        renderengine::Texture2D* maapYUY2TextureBuffer[KI_MAX_NUM_TEXTURES_TO_DISPLAY]   // +0x18C
                                                      [KI_NUM_TEXTURES_TO_BUFFER];
        renderengine::Texture::Locked maaLockedYUY2Textures[KI_MAX_NUM_TEXTURES_TO_DISPLAY] // +0x1B0
                                                           [KI_NUM_TEXTURES_TO_BUFFER];

        bool maabRenderTexture[KI_MAX_NUM_TEXTURES_TO_DISPLAY][KI_NUM_TEXTURES_TO_BUFFER];           // +0x2AC
        bool maabRenderCompressedTexture[KI_MAX_NUM_TEXTURES_TO_DISPLAY][KI_NUM_TEXTURES_TO_BUFFER]; // +0x2B5
        bool maabRenderYUY2Texture[KI_MAX_NUM_TEXTURES_TO_DISPLAY][KI_NUM_TEXTURES_TO_BUFFER];       // +0x2BE

        EPrepareStage mePrepareStage;       // +0x2C8
        EReleaseStage meReleaseStage;       // +0x2CC

        rw::IResourceAllocator* mpHeapAllocator;     // +0x2D0
        rw::IResourceAllocator* mpTextureAllocator;  // +0x2D4

        BrnGui::GuiCache*       mpGuiCache;          // +0x2D8
        BrnFlapt::FlaptRenderer* mpFlaptRenderer;    // +0x2DC

        s32  miCurrentRenderTexture;        // +0x2E0
        s32  miCurrentCopyToTexture;        // +0x2E4
        s32  miClearTexturesFrameCount;     // +0x2E8
        bool mbRenderTexture;               // +0x2EC
        bool mbUseDefaultTexture;           // +0x2ED
    };
}

#endif // BRN_NETWORK_PLAYER_IMAGE_RENDERER_H
