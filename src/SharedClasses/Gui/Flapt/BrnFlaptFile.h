#ifndef BRN_FLAPT_FILE_H
#define BRN_FLAPT_FILE_H

#include "types.hpp"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"

namespace rw { struct Resource; }
namespace renderengine { class Texture; }

// Serialised FLApt movie data.  The declarations below are the full DecFIGS
// member set, widened with the native x64 ABI used by the PC bundle converter.
// ARTIST remains the behavioural authority for FixUp/FixDown and the timeline
// accessors; tools/assets/bundles/flapt_widen.py emits this exact layout.
namespace BrnFlapt
{
    struct FlaptFile;

    struct Vector2
    {
        f32 mfX;
        f32 mfY;
    };

    struct HashedString
    {
        u32         muHash;
        const char* mpacDEBUGString;
    };

    struct TextField
    {
        HashedString mName;
        u16          muInitialStringId;
        u8           muFontStyleIndex;
        u8           mxFlags;
        u8           muAlignment;
        Vector2      mTopLeft;
        Vector2      mBottomRight;
    };

    struct FontStyle
    {
        char* mpacFontName;
        u32   muColour;
        f32   mfFontHeight;
    };

    struct Mesh
    {
        s8  miTextureId;
        u8  muNumVerts;
        u16 muVertOffset;
    };

    struct FloatVector4
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };

    // The authored transform and colour-transform records are both two packed
    // Vector4 lanes (32 bytes, 16-byte aligned) in the serialised image. They
    // are distinct records in the DecFIGS declaration and feed different rows
    // of the live Im2dTransform.
    struct alignas(16) Transform
    {
        FloatVector4 mRightUp;
        FloatVector4 mOriginXYZ;
    };

    struct alignas(16) ColourTransform
    {
        FloatVector4 mScale;
        FloatVector4 mTranslate;
    };

    struct RenderLayer
    {
        u8 mxFlags;
        u8 muMovieClipOffset;
        u8 muMovieClipCount;
        u8 muMeshOffset;
        u8 muMeshCount;
        u8 muTextFieldOffset;
        u8 muTextFieldCount;
    };

    struct RenderLayerKeyFrame
    {
        u32 mxEnabledMovieClips;
        u32 mxEnabledMeshes;
        u32 mxEnabledTextFields;
    };

    struct FScriptCommand
    {
        enum TriggerTypes
        {
            E_TRIGGER_SOUND = 0,
            E_TRIGGER_TRANSITIONCOMPLETE = 1,
            E_TRIGGERTYPE_COUNT = 2,
        };

        u8  muCommand;
        u8  muParam8;
        u16 muParam16;
    };

    struct KeyFrameAnims
    {
        u32             maxPlacedChildren[3];
        u8              muCommandOffset;
        u8              muCommandCount;
        u8              muNumTransforms;
        u8              muNumColourTransforms;
        u8*             mpauTransformObjects;
        Transform*      mpaTransforms;
        u8*             mpauColourTransformObjects;
        ColourTransform* mpaColourTransforms;
    };

    struct IndexPath
    {
        u8 muDepth;
        u8 mauPath[32];
    };

    struct TriggerParameters
    {
        const char* mapcParameters[4];
    };

    struct FileDebugData
    {
        u32          muNumStrings;
        const char** mpapStrings;
    };

    struct MovieClip
    {
        u32 GetKeyframeForFrame(u32 luFrame) const;
        s32 FindLabelledFrameIndex(u32 luLabelId, const char* lpcLabelText) const;
        void FixUp(uintptr_t luBase);
        void FixDown(uintptr_t luBase);

        u8  mxFlags;
        u8  muNumChildren;
        u8  muNumMeshes;
        u8  muNumTextFields;
        u8  muNumRenderLayers;
        u8  muNumLabelledFrames;
        u8  muNumFScriptCommands;
        u8  muPad07;
        u16 muNumFramesInTimeline;
        u16 muNumKeyFrames;

        FlaptFile*          mpFile;
        u16*                mpauFrameToKeyFrameMap;
        RenderLayer*        mpaRenderLayers;
        RenderLayerKeyFrame* mpaKeyFrames;
        KeyFrameAnims*      mpaKeyFrameAnims;
        FScriptCommand*     mpaFScriptStream;
        u16*                mpauChildMovieClips;
        HashedString*       mpaChildNames;
        Mesh*               mpaMeshes;
        TextField*          mpaTextFields;
        HashedString*       mpaTextFieldNames;
        HashedString*       mpaFrameLabels;
        u16*                mpauLabelledFrameIds;
        const char*         mpcComponentName;
    };

    struct FlaptFile
    {
        typedef renderengine::Texture                       GuiTexture;
        typedef CgsGraphics::Basic2dColouredTexturedVertex GuiVertex;

        void FixUp(const rw::Resource& lrResource);
        void FixDown(const rw::Resource& lrResource);
        static void SetSpecialTexture(GuiTexture* lpFLAptCustomTexture,
                                      const char* lpcSpecialTextureName);

        static FlaptFile* mpFlaptFile;

        u8   muVersion;
        u8   mau8Pad01[3];
        u32  muSizeInBytes;
        f32  mfTimePerFrame;
        u32  muNumMovieClips;
        MovieClip* mpaMovieClips;

        u32         muNumTextures;
        GuiTexture** mpapTextures;
        u32         muNumVerts;
        GuiVertex*  mpaVerts;
        u32         muNumFontStyles;
        FontStyle*  mpaFontStyles;

        u32          muNumComponents;
        HashedString* mpaComponentNames;
        IndexPath*   mpaComponentPaths;
        u32          muNumTriggerParameters;
        TriggerParameters* mpaTriggerParameters;
        u32          muNumStrings;
        const CgsUnicode::CgsUtf8** mpapStrings;
        u32          muNumSpecialTextures;
        const char** mpapSpecialTextureNames;
        FileDebugData mDEBUGData;
    };

    static_assert(sizeof(Vector2) == 0x08, "FLApt x64 Vector2 layout");
    static_assert(sizeof(HashedString) == 0x10, "FLApt x64 HashedString layout");
    static_assert(sizeof(TextField) == 0x28, "FLApt x64 TextField layout");
    static_assert(sizeof(FontStyle) == 0x10, "FLApt x64 FontStyle layout");
    static_assert(sizeof(Mesh) == 0x04, "FLApt x64 Mesh layout");
    static_assert(sizeof(Transform) == 0x20, "FLApt x64 Transform layout");
    static_assert(sizeof(ColourTransform) == 0x20, "FLApt x64 ColourTransform layout");
    static_assert(sizeof(RenderLayer) == 0x07, "FLApt RenderLayer layout");
    static_assert(sizeof(RenderLayerKeyFrame) == 0x0C, "FLApt RenderLayerKeyFrame layout");
    static_assert(sizeof(KeyFrameAnims) == 0x30, "FLApt x64 KeyFrameAnims layout");
    static_assert(sizeof(IndexPath) == 0x21, "FLApt IndexPath layout");
    static_assert(sizeof(TriggerParameters) == 0x20, "FLApt x64 TriggerParameters layout");
    static_assert(sizeof(FileDebugData) == 0x10, "FLApt x64 FileDebugData layout");
    static_assert(sizeof(MovieClip) == 0x80, "FLApt x64 MovieClip layout");
    static_assert(sizeof(FlaptFile) == 0xA0, "FLApt x64 FlaptFile layout");
}

#endif // BRN_FLAPT_FILE_H
