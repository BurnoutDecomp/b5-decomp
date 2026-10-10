#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <vector>
#include <cstring>
#include "pc/gcm/renderengine/Instancing.h"
#include "pc/gcm/renderengine/GeometryBindings.h"

static int checks, failures;
static void Check(bool ok, const char* text) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", text); } }
static ID3DBlob* Compile(const char* source, const char* entry, const char* target)
{
    ID3DBlob *code=nullptr,*error=nullptr;
    const HRESULT hr=D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,entry,target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_PACK_MATRIX_ROW_MAJOR,0,&code,&error);
    if(FAILED(hr)){std::printf("compile %08x %s\n",unsigned(hr),error?static_cast<const char*>(error->GetBufferPointer()):"");}
    if(error)error->Release();return code;
}
static std::vector<unsigned> Pixels(IDirect3DDevice9* device)
{
    IDirect3DSurface9 *source=nullptr,*copy=nullptr; std::vector<unsigned> result;
    if(FAILED(device->GetRenderTarget(0,&source)))return result;
    if(SUCCEEDED(device->CreateOffscreenPlainSurface(96,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&copy,nullptr))
        &&SUCCEEDED(device->GetRenderTargetData(source,copy)))
    {
        D3DLOCKED_RECT lock{};
        if(SUCCEEDED(copy->LockRect(&lock,nullptr,D3DLOCK_READONLY))){
            result.resize(96*64);
            for(unsigned y=0;y<64;++y)std::memcpy(result.data()+y*96,static_cast<char*>(lock.pBits)+y*lock.Pitch,96*4);
            copy->UnlockRect();
        }
    }
    if(copy)copy->Release();source->Release();return result;
}
int main()
{
    using namespace renderengine::InstancingPC;
    HWND window=CreateWindowA("STATIC","Native instancing checks",WS_OVERLAPPEDWINDOW,0,0,96,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    auto* d3d=Direct3DCreate9(D3D_SDK_VERSION);IDirect3DDevice9* device=nullptr;
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
    pp.BackBufferWidth=96;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    if(!d3d||FAILED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device)))return 2;
    const char* source=R"(
float4x4 world : register(c20);
float4 g_wheelConstants : register(c31);
float4 tint : register(c60);
struct Input { float3 position:POSITION; float2 uv:TEXCOORD0; };
struct Output { float4 position:POSITION; float4 colour:COLOR0; };
Output vs(Input v) { Output o; o.position=mul(float4(v.position,1),world);
 o.colour=float4(g_wheelConstants.x,v.uv,1)*tint; return o; }
float4 ps(Output v):COLOR0 { return v.colour; }
)";
    ID3DBlob* vsCode=Compile(source,"vs","vs_3_0");ID3DBlob* psCode=Compile(source,"ps","ps_3_0");
    if(!vsCode||!psCode)return 3;
    IDirect3DVertexShader9* vs=nullptr;IDirect3DPixelShader9* ps=nullptr;
    if(FAILED(device->CreateVertexShader(static_cast<DWORD*>(vsCode->GetBufferPointer()),&vs))
        ||FAILED(device->CreatePixelShader(static_cast<DWORD*>(psCode->GetBufferPointer()),&ps)))return 4;
    const D3DVERTEXELEMENT9 layout[]={
        {0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
        {0,12,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,2},D3DDECL_END()};
    IDirect3DVertexDeclaration9* declaration=nullptr;
    if(FAILED(device->CreateVertexDeclaration(layout,&declaration)))return 5;
    Program program;
    const auto* words=static_cast<DWORD*>(vsCode->GetBufferPointer());const size_t count=vsCode->GetBufferSize()/4;
    const std::vector<DWORD> before(words,words+count);
    Check(BuildProgram(words,count,layout,4,program)&&program.worldRegister==20
        &&program.wheelRegister==31&&program.fields==5,"compiled shader constants become a five-field native instance layout");
    Check(std::memcmp(words,before.data(),vsCode->GetBufferSize())==0,"building a variant preserves the shared source shader bytes");
    bool unique=true;unsigned mask=(1u<<0)|(1u<<2);
    for(size_t i=3;i+1<program.elements.size();++i){const unsigned bit=1u<<program.elements[i].UsageIndex;
        unique=unique&&!(mask&bit)&&program.elements[i].Stream==1;mask|=bit;}
    Check(unique&&program.elements.size()==9,"instance semantics avoid every source declaration semantic including unused attributes");
    Check(!BuildProgram(words,count-1,layout,4,program),"truncated shader bytecode is rejected without an out-of-bounds token walk");
    auto relative=before;bool changed=false;
    for(size_t at=1;at<relative.size()&&!changed;){const unsigned op=relative[at]&D3DSI_OPCODE_MASK;
        if(op==D3DSIO_END)break;const size_t n=op==D3DSIO_COMMENT?(relative[at]>>16)&0x7fff:(relative[at]>>24)&15;
        if(op!=D3DSIO_COMMENT&&op!=D3DSIO_DCL&&op!=D3DSIO_DEF)
            for(size_t p=1;p<=n;++p)if(RegisterType(relative[at+p])==D3DSPR_CONST){relative[at+p]|=D3DSHADER_ADDRESSMODE_MASK;changed=true;break;}
        at+=n+1;}
    Check(changed&&!BuildProgram(relative.data(),relative.size(),layout,4,program),"relative uniform addressing cannot silently select the wrong instance data");

    const float vertices[]={-.375f,-.375f,0, .25f,.5f, .375f,-.375f,0, .25f,.5f, 0,.375f,0, .25f,.5f};
    const WORD indices[]={0,1,2};IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;void* data=nullptr;
    device->CreateVertexBuffer(sizeof(vertices),D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&vb,nullptr);
    device->CreateIndexBuffer(sizeof(indices),D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib,nullptr);
    if(!vb||!ib)return 6;
    vb->Lock(0,0,&data,0);std::memcpy(data,vertices,sizeof(vertices));vb->Unlock();
    ib->Lock(0,0,&data,0);std::memcpy(data,indices,sizeof(indices));ib->Unlock();
    device->SetVertexShader(vs);device->SetPixelShader(ps);device->SetVertexDeclaration(declaration);
    device->SetStreamSource(0,vb,0,20);device->SetIndices(ib);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    const float tint[]={1,1,1,.5f};device->SetVertexShaderConstantF(60,tint,1);
    float matrices[5][16]{};
    for(unsigned i=0;i<5;++i){matrices[i][0]=matrices[i][5]=matrices[i][10]=matrices[i][15]=1;
        matrices[i][12]=-.5f+.25f*i;matrices[i][13]=(i&1)?.25f:-.25f;}
    matrices[3][0]=0;matrices[3][1]=.5f;matrices[3][4]=-.75f;matrices[3][5]=0;
    float wheel[5][4]={{.125f,0,0,2},{.375f,0,0,0},{.625f,0,0,3},{.875f,0,0,1},{1,0,0,4}};
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);device->BeginScene();
    for(unsigned i=0;i<5;++i){device->SetVertexShaderConstantF(20,matrices[unsigned(wheel[i][3])],4);
        device->SetVertexShaderConstantF(31,wheel[i],1);device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1);}
    device->EndScene();const auto reference=Pixels(device);
    unsigned coloured=0;for(unsigned pixel:reference)coloured+=(pixel&0xffffff)!=0;
    Check(reference.size()==96*64&&coloured>500,"the five individual reference draws produce visible overlapping geometry");
    Cache cache;
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);device->BeginScene();
    const bool began=cache.Begin(device,vs,declaration,matrices[0],wheel[0],5);
    HRESULT draw=E_FAIL;if(began)draw=device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1);
    cache.End();device->EndScene();const auto instanced=Pixels(device);
    Check(began&&SUCCEEDED(draw),"the real D3D9 device executes one indexed hardware-instanced draw");
    Check(began&&reference==instanced,"one instanced draw exactly matches five draws including matrix selection, wheel values and blending order");
    IDirect3DVertexBuffer9* padded=nullptr;
    device->CreateVertexBuffer(64+sizeof(vertices),D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&padded,nullptr);
    bool rebased=padded&&SUCCEEDED(padded->Lock(64,sizeof(vertices),&data,0));
    if(rebased){std::memcpy(data,vertices,sizeof(vertices));padded->Unlock();
        const auto window=renderengine::GeometryBindingsPC::Rebase(64,20,0);
        device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);device->BeginScene();
        rebased=cache.Begin(device,vs,declaration,matrices[0],wheel[0],5);
        renderengine::GeometryBindingsPC::gCache.BindVertex(device,padded,window.muOffset,20);
        renderengine::GeometryBindingsPC::gCache.BindIndex(device,ib);
        if(rebased)rebased=SUCCEEDED(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,window.miBase,0,3,0,1));
        cache.End();device->EndScene();rebased&=Pixels(device)==reference;
        renderengine::GeometryBindingsPC::gCache.BindVertex(device,vb,0,20);
    }
    Check(rebased,"rebasing a pooled vertex offset preserves independent instance-stream addressing");
    if(padded)padded->Release();
    UINT frequency0=0,frequency1=0,stride=0,offset=0;IDirect3DVertexBuffer9* extra=nullptr;
    device->GetStreamSourceFreq(0,&frequency0);device->GetStreamSourceFreq(1,&frequency1);
    device->GetStreamSource(1,&extra,&offset,&stride);
    Check(frequency0==1&&frequency1==1&&!extra,"ending the batch restores both frequencies and releases the instance stream binding");if(extra)extra->Release();
    IDirect3DVertexShader9* restoredVs=nullptr;IDirect3DVertexDeclaration9* restoredDeclaration=nullptr;
    device->GetVertexShader(&restoredVs);device->GetVertexDeclaration(&restoredDeclaration);
    Check(restoredVs==vs&&restoredDeclaration==declaration,"ending the batch restores the original shader and declaration");
    if(restoredVs)restoredVs->Release();if(restoredDeclaration)restoredDeclaration->Release();
    bool repeat=true;device->BeginScene();
    for(unsigned i=0;i<250;++i){repeat=cache.Begin(device,vs,declaration,matrices[0],wheel[0],5)&&repeat;
        if(repeat)repeat=SUCCEEDED(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1));cache.End();}
    device->EndScene();
    Check(repeat,"cached shader/declaration reuse and dynamic instance-buffer wrap stay valid with queued GPU work");
    // Valid matrix opcodes have implicit register operands. Exercise spans
    // crossing INTO and OUT OF the per-instance world constant range.
    for(unsigned kind=0;kind<2;++kind)
    {
        std::vector<DWORD> matrixCode{D3DVS_VERSION(3,0)};
        for(size_t at=1;at<before.size();){const unsigned op=before[at]&D3DSI_OPCODE_MASK;
            if(op==D3DSIO_END)break;const size_t n=op==D3DSIO_COMMENT?(before[at]>>16)&0x7fff:(before[at]>>24)&15;
            if(op==D3DSIO_COMMENT)matrixCode.insert(matrixCode.end(),before.begin()+at,before.begin()+at+n+1);
            at+=n+1;}
        const auto declare=[&](unsigned type,unsigned index,unsigned usage){
            matrixCode.insert(matrixCode.end(),{DWORD((2u<<24)|D3DSIO_DCL),DWORD(0x80000000u|usage),
                RegisterBits(type,index)|D3DSP_WRITEMASK_ALL});};
        declare(D3DSPR_INPUT,0,D3DDECLUSAGE_POSITION);
        declare(D3DSPR_OUTPUT,0,D3DDECLUSAGE_POSITION);
        declare(D3DSPR_OUTPUT,1,D3DDECLUSAGE_COLOR);
        if(kind)matrixCode.insert(matrixCode.end(),{DWORD((2u<<24)|D3DSIO_MOV),
            RegisterBits(D3DSPR_OUTPUT,0)|D3DSP_WRITEMASK_ALL,RegisterBits(D3DSPR_CONST,60)|D3DSP_NOSWIZZLE});
        matrixCode.insert(matrixCode.end(),{DWORD((3u<<24)|(kind?D3DSIO_M3x2:D3DSIO_M4x4)),
            RegisterBits(D3DSPR_OUTPUT,0)|(kind?D3DSP_WRITEMASK_0|D3DSP_WRITEMASK_1:D3DSP_WRITEMASK_ALL),
            RegisterBits(D3DSPR_INPUT,0)|D3DSP_NOSWIZZLE,RegisterBits(D3DSPR_CONST,kind?23:19)|D3DSP_NOSWIZZLE});
        matrixCode.insert(matrixCode.end(),{DWORD((2u<<24)|D3DSIO_MOV),
            RegisterBits(D3DSPR_OUTPUT,1)|D3DSP_WRITEMASK_ALL,RegisterBits(D3DSPR_CONST,31)|D3DSP_NOSWIZZLE,DWORD(D3DSIO_END)});
        IDirect3DVertexShader9* matrixShader=nullptr;
        const HRESULT created=device->CreateVertexShader(matrixCode.data(),&matrixShader);
        Check(SUCCEEDED(created),kind?"the original cross-boundary m3x2 shader is valid on D3D9":"the original cross-boundary m4x4 shader is valid on D3D9");
        bool same=false;
        if(matrixShader)
        {
            float selectedMatrices[5][16]{};
            for(unsigned i=0;i<5;++i){selectedMatrices[i][1]=1;selectedMatrices[i][3]=-.5f+.25f*i;
                selectedMatrices[i][11]=1;selectedMatrices[i][12]=.5f+.125f*i;}
            const float shared19[]={1,0,0,0}, shared24[]={0,1,0,0}, shared60[]={0,0,0,1};
            device->SetVertexShader(matrixShader);device->SetVertexDeclaration(declaration);
            device->SetVertexShaderConstantF(19,shared19,1);device->SetVertexShaderConstantF(24,shared24,1);
            device->SetVertexShaderConstantF(60,shared60,1);
            device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);device->BeginScene();
            for(unsigned i=0;i<5;++i){device->SetVertexShaderConstantF(20,selectedMatrices[unsigned(wheel[i][3])],4);
                device->SetVertexShaderConstantF(31,wheel[i],1);device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1);}
            device->EndScene();const auto matrixReference=Pixels(device);
            unsigned visible=0;for(unsigned pixel:matrixReference)visible+=(pixel&0xffffff)!=0;
            device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);device->BeginScene();
            const bool ready=cache.Begin(device,matrixShader,declaration,selectedMatrices[0],wheel[0],5);
            const HRESULT submitted=ready?device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1):E_FAIL;
            cache.End();device->EndScene();
            same=ready&&SUCCEEDED(submitted)&&visible>0&&matrixReference==Pixels(device);
            matrixShader->Release();
        }
        Check(same,kind?"m3x2 retains the uniform following the instance matrix":"m4x4 retains the uniform preceding the instance matrix");
    }
    device->SetVertexShader(vs);
    cache.RetireDeclaration(declaration);device->BeginScene();
    const bool recreated=cache.Begin(device,vs,declaration,matrices[0],wheel[0],5);cache.End();device->EndScene();
    Check(recreated,"retiring a source declaration releases its variant and allows clean recreation");
    wheel[0][3]=8;
    Check(!cache.Begin(device,vs,declaration,matrices[0],wheel[0],5),"invalid matrix indices never read outside the original five-entry array");
    cache.Release();
    ib->Release();vb->Release();declaration->Release();ps->Release();vs->Release();vsCode->Release();psCode->Release();
    device->Release();d3d->Release();DestroyWindow(window);
    std::printf("PCInstancing: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
