#include <cstdio>
#include <cmath>
#include <vector>
#include "types.hpp"
static int assertions=0;
#define CGS_ASSERT(c,m) do { if(!(c)) ++assertions; } while(0)
template<class T,int N> struct Stack {
    std::vector<T> a;
    void Push(T v){a.push_back(v);}
    void Pop(){a.pop_back();}
    T& Peek(){return a.back();}
    T& operator[](u32 i){return a.at(i);}
    u32 GetLength()const{return static_cast<u32>(a.size());}
    bool IsEmpty()const{return a.empty();}
};
namespace renderengine {struct Texture {};}
namespace CgsGraphics {
struct Vec2 {float x=0,y=0;};
struct Vec4 {float x=0,y=0,z=0,w=0;};
struct Im2dTransform {Vec4 mOriginXYZ,mRightUp,mColourScale,mColourShift;};
struct Basic2dColouredTexturedVertex {Vec2 mv2Pos;Vec4 mv4Colour;Vec2 mv2Tex0UV;};
#include "playtest_flapt_logical.inc"
}
struct Buffer {
    int depth=0,pops=0,underflows=0;
    int transforms=0;
    CgsGraphics::Basic2dColouredTexturedVertex corners[2];
    renderengine::Texture* texture=nullptr;
    void PopMask(){++pops;if(depth) --depth;else ++underflows;}
    void SetTransform(const CgsGraphics::Im2dTransform&){++transforms;}
    void PushMask(renderengine::Texture* t,const CgsGraphics::Basic2dColouredTexturedVertex* v){
        texture=t;corners[0]=v[0];corners[1]=v[1];++depth;}
};
struct RenderSet {Buffer* mpIm2dRenderBuffer;};
namespace BrnFlapt {
struct Mesh {u32 muNumVerts=4,muVertOffset=0;};
struct FlaptFile {
    using GuiTexture=renderengine::Texture;
    using GuiVertex=CgsGraphics::Basic2dColouredTexturedVertex;
    GuiVertex* mpaVerts;
};
struct FlaptRenderer {
    u8 mxFlags=0;
    Stack<u16,2> mMaskMeshCounts;
    Stack<CgsGraphics::Im2dTransform,33> maTransformStack;
    RenderSet* mpImRenderSet;
    void StartDrawingMask();
    void PopMask();
    void RenderMask(const Mesh*,const FlaptFile*,const FlaptFile::GuiTexture*);
};
#include "playtest_flapt_mask_count.inc"
}
int main(){
    int checks=0,failures=0;
    auto ck=[&](bool pass,const char*label){++checks;if(!pass){++failures;std::printf("FAIL %s\n",label);}};
    Buffer buffer;RenderSet set{&buffer};BrnFlapt::FlaptRenderer r;r.mpImRenderSet=&set;
    for(int meshes=0;meshes<=2;++meshes){
        buffer={};r.mxFlags=0x40;r.StartDrawingMask();
        ck(r.mxFlags==0x41&&r.mMaskMeshCounts.GetLength()==1&&r.mMaskMeshCounts.Peek()==0,
           "start retains unrelated flags and creates an empty tally");
        for(int i=0;i<meshes;++i){++r.mMaskMeshCounts.Peek();++buffer.depth;}
        r.mxFlags &= ~1u;r.PopMask();
        ck(buffer.pops==meshes&&buffer.depth==0&&buffer.underflows==0,
           "pop count equals submitted mesh count, including an empty animated layer");
        ck(r.mMaskMeshCounts.IsEmpty()&&r.mxFlags==0x40,"closing layer removes only its tally");
    }
    // An empty nested layer must preserve the actual parent's mask.
    buffer={};r.mxFlags=0;r.StartDrawingMask();++r.mMaskMeshCounts.Peek();++buffer.depth;
    r.mxFlags &= ~1u;r.StartDrawingMask();r.mxFlags &= ~1u;r.PopMask();
    ck(buffer.depth==1&&buffer.pops==0&&r.mMaskMeshCounts.GetLength()==1,
       "empty nested layer leaves parent's GPU mask and tally intact");
    r.PopMask();ck(buffer.depth==0&&buffer.pops==1&&buffer.underflows==0,"parent closes without underflow");
    // Two populated nested layers each close exactly their own mesh.
    buffer={};r.StartDrawingMask();++r.mMaskMeshCounts.Peek();++buffer.depth;r.mxFlags &= ~1u;
    r.StartDrawingMask();++r.mMaskMeshCounts.Peek();++buffer.depth;r.mxFlags &= ~1u;
    r.PopMask();ck(buffer.depth==1&&buffer.pops==1,"inner populated mask leaves outer active");
    r.PopMask();ck(buffer.depth==0&&buffer.pops==2&&r.mMaskMeshCounts.IsEmpty(),"both populated layers close");
    // Independent pixel-space expectations for original diagonal mask math.
    // The actual native-to-logical adapter is extracted alongside RenderMask.
    struct Case {float ox,oy,sx,sy,x0,y0,x1,y1;};
    const Case cases[]={
        {-.5f,.25f,1.f/640,-1.f/360,330,290,370,350},
        {0,0,2.f/640,-3.f/360,660,420,740,600},
        {.125f,-.5f,-2.f/640,.5f/360,700,530,620,500},
        {-5,-8,.5f,.25f,640,1440,13440,-3960}};
    CgsGraphics::Basic2dColouredTexturedVertex verts[5]={};
    verts[1].mv2Pos={10,20};verts[2].mv2Pos={50,20};verts[3].mv2Pos={10,80};verts[4].mv2Pos={50,80};
    verts[1].mv4Colour={1,2,3,4};verts[4].mv4Colour={5,6,7,8};
    verts[1].mv2Tex0UV={.125f,.25f};verts[4].mv2Tex0UV={.75f,.875f};
    BrnFlapt::FlaptFile file{verts};BrnFlapt::Mesh mesh;mesh.muVertOffset=1;
    renderengine::Texture texture;
    for(const auto& c:cases){
        buffer={};r.mxFlags=0;r.StartDrawingMask();CgsGraphics::Im2dTransform transform;
        transform.mOriginXYZ={c.ox,c.oy,0,0};transform.mRightUp={c.sx,0,0,c.sy};
        r.maTransformStack.Push(transform);r.RenderMask(&mesh,&file,&texture);
        ck(std::fabs(buffer.corners[0].mv2Pos.x-c.x0)<.0001f && std::fabs(buffer.corners[0].mv2Pos.y-c.y0)<.0001f &&
           std::fabs(buffer.corners[1].mv2Pos.x-c.x1)<.0001f && std::fabs(buffer.corners[1].mv2Pos.y-c.y1)<.0001f,
           "mask corners receive scale times vertex plus translation exactly once");
        ck(buffer.texture==&texture&&buffer.corners[0].mv4Colour.w==4&&buffer.corners[1].mv4Colour.w==8&&
           buffer.corners[0].mv2Tex0UV.x==.125f&&buffer.corners[1].mv2Tex0UV.y==.875f,
           "mask preserves selected extrema colour, UV and texture");
        ck(buffer.transforms==0,"screen-space mask writer does not replace ordinary mesh transform state");
        r.mxFlags &= ~1u;r.PopMask();r.maTransformStack.Pop();
        ck(buffer.depth==0&&buffer.pops==1&&buffer.underflows==0,"actual RenderMask tally closes once");
    }
    ck(assertions==0,"valid mask sequences remain assertion free");
    std::printf("PlaytestFlaptMaskCount: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
