// b5-decomp/src/SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronarenderer.h
#ifndef RENDERENGINE_CORONARENDERER_H
#define RENDERENGINE_CORONARENDERER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // Vector2, Vector4, Matrix44
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // ProgramBufferData, ProgramVariableHandle
#include "pc/gcm/renderengine/VertexDescriptor.h"                        // VertexDescriptorData

// =================================================================================================
// renderengine::CoronaRenderer -- the corona (light-flare) render pass.
//
// DECLARATION SHAPE IS THE DecFIGS DWARF
// (references/DecFIGS/dwarfdump/SDKs/RenderEngineClub/MAIN/components/include/coronas/
//  rwgcoronarenderer.h), gated on the X360 ledger:
//     :73  void Initialize(rw::IResourceAllocator&)                          X360 0x822850F8
//     :74  void Release(rw::IResourceAllocator&)                             X360 -- not attested, declared only
//     :75  void SetBlendState(const BlendState*)                             INLINED into BrnCoronaManager::Construct
//     :77  void SetDepthStencilStates(const DepthStencilState*, const DepthStencilState*)  same
//     :80  void SetTextureAtlas(const TextureState*, uint32_t, Vector2*)     INLINED into BrnCoronaManager::SetTextureAtlas
//     :202 void Begin<Device>(const RenderParameters&)                       X360 0x823FF2C0
//     :235 void End<Device>()                                                INLINED (BrnCoronaManager::Render's tail)
//     :246 void Dispatch<Device>(const BatchParameters&, CoronaBuffer*)      X360 0x82404F30
// The private statics are the console's file-scope corona globals; the DWARF names them, so the
// invented spellings the first reconstruction used (gsuCoronaRenderStateWord etc.) are gone:
//     s_numSubImages            dword_82FAB6A8   s_vertexDescriptor  dword_82FAB6AC
//     s_pixelProgram            dword_82FAB6B0   s_vertexProgram     dword_82FAB6B4
//     s_atlasUVs                dword_82FAB6B8   s_textureState      dword_82FAB6BC
//     s_blendState              dword_82FAB6C0   s_enabledZTestState dword_82FAB6C4
//     s_disabledZTestState      dword_82FAB6C8   s_beginLock         byte_82FAB695
//     s_viewXyScaleHandle       byte_82FAB618    s_viewProjectionMatrixHandle byte_82FAB61C
//     s_cameraPositionHandle    byte_82FAB620
// (blend-state cite: `stw r11, dword_82FAB6C0@l(r9)` @0x823FCDDC in BrnCoronaManager::Construct.)
//
// THE THREE TEMPLATE ENTRY POINTS ARE NOT TEMPLATES HERE. On the console they are
// `template <class Device> static void Begin(...)` instantiated once, for `shadow::Device`; this
// tree has exactly one device layer (GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h)
// and the .cpp calls it by name, so the single instantiation is spelled as a plain static. That is
// the same de-templating the committed post-fx / immediate-mode reconstructions use, and it keeps
// the body out of this header (which BrnRendererModule.h transitively includes -- see the
// BrnCoronaManager.h fan-out note).
// =================================================================================================

namespace rw { struct IResourceAllocator; }

namespace renderengine
{
    class  TextureState;          // pc/gcm/renderengine/renderstates.h
    class  DepthStencilState;     // pc/gcm/renderengine/renderstates.h
    struct BlendMaterialState;    // SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h
    class  CoronaBuffer;          // GameSource/Graphics/BrnCoronaManager.h

    class CoronaRenderer
    {
    public:
        // rwgcoronarenderer.h:25. Read by Begin at +0x10 / +0x20 / +0x60 -- confirmed against the
        // X360 asm of BrnCoronaManager::Render @0x824075D8, which stages exactly those three
        // lanes and NEVER writes m_flags (see the .cpp banner).
        struct RenderParameters
        {
            static const u32 FLAG_USE_DIRECTION        = 1;   // :26
            static const u32 FLAG_CALCULATE_ROTATION   = 2;   // :27
            static const u32 FLAG_CALCULATE_INTENSITY  = 4;   // :28

            u32      muFlags;                        // +0x00  :35
            Vector4  mvCameraPositionPlusWhiteLevel; // +0x10  :36
            Matrix44 mViewProjectionMatrix;          // +0x20  :37
            Vector4  mvViewXyScale;                  // +0x60  :38

            void SetFlags(u32 luFlags)                          { muFlags = luFlags; }                     // :30
            void SetCameraPositionPlusWhiteLevel(const Vector4& lrValue) { mvCameraPositionPlusWhiteLevel = lrValue; } // :31
            void SetViewProjectionMatrix(const Matrix44& lrValue)        { mViewProjectionMatrix = lrValue; }          // :32
            void SetViewXyScale(const Vector4& lrValue)         { mvViewXyScale = lrValue; }               // :33
        };

        // rwgcoronarenderer.h:42
        struct BatchParameters
        {
            static const u32 FLAG_OCCLUSION_ZTEST            = 1;  // :43
            static const u32 FLAG_OCCLUSION_CONDITIONALRENDER = 2;  // :44

            u32 muNumCoronas;   // +0x00  :49
            u32 muFlags;        // +0x04  :50

            void SetNumCoronas(u32 luNumCoronas) { muNumCoronas = luNumCoronas; }  // :46
            void SetFlags(u32 luFlags)           { muFlags = luFlags; }            // :47
        };

        static void Initialize(rw::IResourceAllocator& lrAllocator);                       // :73
        static void SetBlendState(const BlendMaterialState* lpBlendState);                 // :75
        static void SetDepthStencilStates(const DepthStencilState* lpEnabledZTest,
                                          const DepthStencilState* lpDisabledZTest);       // :77
        static void SetTextureAtlas(const TextureState* lpTextureState, u32 luNumSubImages,
                                    const Vector2* lpAtlasUVs);                            // :80

        static void Begin(const RenderParameters& lrParameters);                           // :202
        static void End();                                                                 // :235
        static void Dispatch(const BatchParameters& lrParameters, CoronaBuffer* lpBuffer);  // :246

        // Is the pass usable? True only once Initialize adopted BOTH programs and built the
        // vertex descriptor. NOT an X360 function: on the console Initialize cannot fail, on PC
        // it can (no device yet / no authored program pair mounted), and the ONLY honest answer
        // to that is to refuse to draw rather than draw a degenerate pass (AGENTS.md rule 9).
        static bool IsReady();                                                             // [FLAG PC bring-up]

    private:
        static u32                       s_numSubImages;               // :102  dword_82FAB6A8
        static bool                      s_beginLock;                  // :103  byte_82FAB695
        static bool                      s_beginBatchLock;             // :104  (no attested writer -- see .cpp)
        static VertexDescriptorData*     s_vertexDescriptor;           // :105  dword_82FAB6AC
        static ProgramBufferData*        s_pixelProgram;               // :106  dword_82FAB6B0
        static ProgramBufferData*        s_vertexProgram;              // :107  dword_82FAB6B4
        static ProgramVariableHandle     s_cameraPositionHandle;       // :108  byte_82FAB620
        static ProgramVariableHandle     s_viewProjectionMatrixHandle; // :109  byte_82FAB61C
        static ProgramVariableHandle     s_viewXyScaleHandle;          // :110  byte_82FAB618
        static const Vector2*            s_atlasUVs;                   // :115  dword_82FAB6B8
        static const TextureState*       s_textureState;               // :116  dword_82FAB6BC
        static const BlendMaterialState* s_blendState;                 // :117  dword_82FAB6C0
        static const DepthStencilState*  s_enabledZTestState;          // :118  dword_82FAB6C4
        static const DepthStencilState*  s_disabledZTestState;         // :119  dword_82FAB6C8
        // :111/:112/:113 s_vertexDescriptorResource / s_pixelProgramResource / s_vertexProgramResource
        // are the three FIVE-WORD rw::Resource handle arrays the console's allocator route fills
        // (dword_82FAC1B8 / dword_82FAB850 / dword_82FAC1CC). The PC route adopts pre-built images
        // and carves the descriptor from the immediate-mode leaf's own arena, so no resource is ever
        // issued -- they are deliberately NOT declared here rather than declared and left null.
        // (Same call as BrnPostFxShader.cpp:910, "a slot built by ProgramBufferPC_Adopt owns NOTHING
        // the allocator ever issued".)
    };
}

#endif // RENDERENGINE_CORONARENDERER_H
