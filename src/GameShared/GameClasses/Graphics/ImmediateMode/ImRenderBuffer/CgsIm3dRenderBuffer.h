#pragma once

#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h"
#include "BrnCommonTypes.h"

namespace CgsGraphics {
struct Im3d;
template<class V> struct Im3dBase;

// FLAG PC-platform leaf: native homes of the original immediate-state entries
// used by buffered3D; the GUI's existing2D identity checks share these pointers.
void PrepareIm3dStateLibraryPC();
const renderengine::DepthStencilState* GetIm3dDepthStencilZBufferOnPC();
renderengine::Texture* GetImWhiteTexturePC();

// ARTIST8244FC48/827E1878: one view-projection matrix (command16), or
// model-to-world plus view-projection (command20). Matrix alignment makes
// their payload begin at16, as in the console command stream.
struct ImCommandSetTransform3dVp : ImCommand { Matrix44 mViewProjectionMatrix; };
struct ImCommandSetTransform3dMtwVp : ImCommand
{
    Matrix44 mModelToWorldMatrix;
    Matrix44 mViewProjectionMatrix;
};
static_assert(sizeof(ImCommandSetTransform3dVp) == 80, "ARTIST transform command");
static_assert(sizeof(ImCommandSetTransform3dMtwVp) == 144, "ARTIST paired transform command");

template<class V>
class Im3dRenderBufferBase : public ImRenderBuffer<V>
{
public:
    Im3dRenderBufferBase() { this->Construct(); }
    void Dispatch(Im3dBase<V>* lpRenderer) const;
    void SetTransform(Matrix44::InParam lViewProjection);
    void SetTransform(Matrix44::InParam lModelToWorld, Matrix44::InParam lViewProjection);
protected:
    virtual bool HandleCommand(const ImCommand* lpCommand, Im3dBase<V>* lpRenderer) const;
    void PostCommand3d(u32 luType, const ImCommand* lpCommand, u32 luBytes);
};

// DecFIGS CgsIm3dRenderBuffer.h:130; the CPU vertex is32bytes and the
// immediate renderer writes the packed24-byte native stream.
class Im3dRenderBuffer : public Im3dRenderBufferBase<BasicColouredTexturedVertex>
{
protected:
    bool HandleCommand(const ImCommand* lpCommand, Im3dBase<BasicColouredTexturedVertex>* lpRenderer) const override;
};
}
