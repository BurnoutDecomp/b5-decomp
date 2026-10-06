"""Exercise the shipped read-only observer against bounded native-API fixtures.

No D3D device or game is created. The three attested installed bytecodes are
supplied as an optional fixture, solely for matching GetFunction results.
Run from the workflow root with --shader pointing to the audited 274C49FB.fxo.
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW


FIXTURE = r'''
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
using u8=uint8_t; using u16=uint16_t; using u32=uint32_t; using f32=float;
enum { D3DDECLTYPE_FLOAT3=2, D3DDECLTYPE_UBYTE4=5, D3DDECLTYPE_UBYTE4N=8,
       D3DDECLTYPE_UNUSED=17, D3DDECLMETHOD_DEFAULT=0, D3DDECLUSAGE_POSITION=0,
       D3DDECLUSAGE_BLENDWEIGHT=1, D3DDECLUSAGE_BLENDINDICES=2, MAXD3DDECLLENGTH=64 };
struct D3DVERTEXELEMENT9 { WORD Stream,Offset; BYTE Type,Method,Usage,UsageIndex; };
std::string gLog; u32 gCalls=0,gReleases=0,gPresent=100;
namespace CgsDev { namespace Log { void WriteToLog(const char* p) { gLog+=p; } } }
namespace renderengine { u32 GetDispatchPresentCountPC() { return gPresent; } }
void LogOnce(const char*,const char* p) { gLog+=p; }
struct IDirect3DVertexShader9 {
    std::vector<u8> code; HRESULT result=S_OK;
    HRESULT GetFunction(void* p,UINT* n) { ++gCalls; if(FAILED(result))return result;
        if(p)std::memcpy(p,code.data(),code.size()); *n=UINT(code.size()); return S_OK; }
    void Release() { ++gReleases; }
};
struct IDirect3DVertexDeclaration9 {
    std::vector<D3DVERTEXELEMENT9> elements; HRESULT result=S_OK;
    HRESULT GetDeclaration(D3DVERTEXELEMENT9* p,UINT* n) { ++gCalls;
        if(FAILED(result))return result; std::memcpy(p,elements.data(),elements.size()*sizeof(*p));
        *n=UINT(elements.size());return S_OK; }
    void Release() { ++gReleases; }
};
struct IDirect3DDevice9 {
    IDirect3DVertexShader9 vs; IDirect3DVertexDeclaration9 decl;
    float palette[512]={},wvp[16]={},world[16]={};
    HRESULT paletteResult=S_OK,wvpResult=S_OK,worldResult=S_OK,vsResult=S_OK,declResult=S_OK;
    HRESULT GetVertexShader(IDirect3DVertexShader9** p) { ++gCalls;
        if(FAILED(vsResult))return vsResult; *p=&vs;return S_OK; }
    HRESULT GetVertexDeclaration(IDirect3DVertexDeclaration9** p) { ++gCalls;
        if(FAILED(declResult))return declResult; *p=&decl;return S_OK; }
    HRESULT GetVertexShaderConstantF(UINT base,float* p,UINT count) { ++gCalls;
        const HRESULT hr=base==0?paletteResult:base==140?wvpResult:worldResult;
        if(FAILED(hr))return hr;
        std::memcpy(p,base==0?palette:base==140?wvp:world,size_t(count)*16u);return S_OK; }
};
u16 sau16LastDeclBlendOffset[2]={12,16};
u8 sau8LastDeclBlendType[2]={D3DDECLTYPE_UBYTE4,D3DDECLTYPE_UBYTE4N};
u32 suLastDeclSourceStride=24,suLastDeclPositionSourceOffset=0,suVertexStride=24;
u8 su8LastDeclPositionType=D3DDECLTYPE_FLOAT3;
u32 suVertexDec3nCount=0; u16 sau16VertexDec3nOffsets[16]={};
struct IndexHeader { u32 muIndexCount; } gIndexHeader={3};
const IndexHeader* spIndexSource=&gIndexHeader;
const void* spVertexSource=&gIndexHeader;
const char* spCurrentTechniqueName="0ehicle_Opaque_PlasticMatt_Damaged";
IDirect3DVertexShader9* spRealVs=nullptr;
IDirect3DVertexShader9* spMeshVertexShader=nullptr;
IDirect3DVertexDeclaration9* spMeshDeclaration=nullptr;
bool sbRealProgramsBound=true,spResetEnabled=false;u32 suResetIndex=65535;
#include "observer.inc"
int main(int argc,char** argv) {
    if(argc!=3)return 90;
    const std::string mode=argv[1];
    _putenv_s("BRN_SILL_NATIVE_PROBE",mode=="off"?"":"100");
    IDirect3DDevice9 d;
    std::ifstream shader(argv[2],std::ios::binary);
    d.vs.code=std::vector<u8>(std::istreambuf_iterator<char>(shader),{});
    if(d.vs.code.size()!=1872)return 91;
    spRealVs=spMeshVertexShader=&d.vs; spMeshDeclaration=&d.decl;
    d.decl.elements={{0,0,2,0,0,0},{0,12,5,0,2,0},{0,16,8,0,1,0},{255,0,17,0,0,0}};
    d.palette[104*4]=0.5f;d.palette[105*4+1]=0.75f;
    for(u32 i=0;i<16;++i){d.wvp[i]=float(i)+0.25f;d.world[i]=float(i)+0.5f;}
    if(mode=="expanded"){
        sau16LastDeclBlendOffset[0]=16;sau16LastDeclBlendOffset[1]=20;
        suVertexDec3nCount=1;sau16VertexDec3nOffsets[0]=12;suVertexStride=32;
        d.decl.elements[1].Offset=24;d.decl.elements[2].Offset=28;
    }
    u8 vertices[4*24]={};
    for(u32 v=0;v<4;++v){
        const float pos[3]={float(v),2.0f,3.0f};std::memcpy(vertices+v*24,pos,12);
        u8* idx=vertices+v*24+sau16LastDeclBlendOffset[0];
        u8* w=vertices+v*24+sau16LastDeclBlendOffset[1];
        idx[0]=v==2?105:104;idx[1]=101;idx[2]=104;w[0]=v==1?0:255;
        if(v==3){idx[0]=101;w[0]=0;w[2]=255;}
    }
    // Base vertex1: vertex0 (row104) is unreferenced; vertex1 has zero row104 weight;
    // vertex3 only names row104 in Z. Only the actually referenced vertex2 is eligible.
    u16 indices[3]={2,0,1};u32 indices32[3]={2,0,1};u32 base=1,start=0,count=3;
    if(mode=="both")vertices[24+sau16LastDeclBlendOffset[1]]=255;
    if(mode=="early")gPresent=99;
    if(mode=="pristine")std::memset(d.palette,0,sizeof(d.palette));
    if(mode=="unmatched")d.vs.code.back()^=1;
    if(mode=="decl_mismatch")d.decl.elements[1].Offset++;
    if(mode=="decl_stream")d.decl.elements[1].Stream=1;
    if(mode=="palette_fail")d.paletteResult=E_FAIL;
    if(mode=="world_fail")d.worldResult=E_FAIL;
    if(mode=="vs_fail")d.vsResult=E_FAIL;
    if(mode=="decl_fail")d.decl.result=E_FAIL;
    if(mode=="code_fail")d.vs.result=E_FAIL;
    if(mode=="nan")d.palette[5]=NAN;
    if(mode=="invalid_index")indices[0]=20;
    if(mode=="invalid_run")count=4;
    if(mode=="bad_layout")sau16LastDeclBlendOffset[0]=23;
    if(mode=="absent"){indices[2]=2;}
    if(mode=="reset"){spResetEnabled=true;indices[0]=65535;}
    if(mode=="sliced"){start=1;count=2;}
    const u8* run=mode=="index32"?reinterpret_cast<const u8*>(indices32):reinterpret_cast<const u8*>(indices+start);
    auto call=[&] { SillNativeProbe_AtDraw(&d,vertices,24,4,run,mode=="index32",base,start,count,true); };
    if(mode=="budget"){
        const char* names[]={"0ehicle_Opaque_PlasticMatt_Damaged","0ehicle_Opaque_PaintGloss_Textured_Damaged","0ehicle_Opaque_Chrome_Damaged_Damaged"};
        for(u32 frame=0;frame<6;++frame){gPresent=100+frame*600;
            for(const char* name:names){spCurrentTechniqueName=name;call();call();}}
    }else call();
    std::printf("calls=%u releases=%u\n%s",gCalls,gReleases,gLog.c_str());
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--shader", type=Path, required=True)
    args = parser.parse_args()
    shader = args.shader.resolve()
    source = (REPO / "src/pc/gcm/renderengine/XenonD3D9Shims.cpp").read_text(encoding="utf-8-sig")
    methods = "\n".join(definition(source, name) for name in (
        "bool ScratchProbe_ReadSkinPair(", "void SillNativeProbe_AtDraw("))
    # The native fixture alone cannot attest the draw integration or read-only API scope.
    assert not re.search(r"->(?:Set|Draw|Lock|Create)\w*\s*\(", methods)
    scratch = definition(source, "void ScratchProbe_AtDraw(")
    assert scratch.index("SillNativeProbe_AtDraw(") < scratch.index('std::getenv("BRN_SCRATCH_PROBE")')
    fast_gate = source[source.index("static const bool sbSkipInactiveDamageProbes"):
                       source.index("if (!sbSkipInactiveDamageProbes)")]
    assert '"BRN_SILL_NATIVE_PROBE"' in fast_gate
    draw = definition(source, "void WorldDraw_IndexedUP(")
    assert "static_cast<const u8*>(lpIndexData) + size_t(luStartIndex) * luIndexSize" in draw
    with tempfile.TemporaryDirectory(prefix="brn_sill_native_") as directory:
        out = Path(directory)
        (out / "observer.inc").write_text(methods, encoding="utf-8")
        (out / "fixture.cpp").write_text(FIXTURE, encoding="utf-8")
        command = "cl " + " ".join(settings("msvc_flags.txt")) + ' fixture.cpp /Fe:fixture.exe'
        script = out / "compile.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + '\nexit /b %ERRORLEVEL%\n', encoding="utf-8", newline="\r\n")
        subprocess.run(["cmd", "/c", str(script)], cwd=out, check=True)
        environment = dict(os.environ)
        environment.pop("NoDefaultCurrentDirectoryInExePath", None)
        qualified = ("qualified", "expanded", "both", "index32", "reset", "sliced")
        failures = ("unmatched", "decl_mismatch", "decl_stream", "palette_fail", "world_fail",
                    "vs_fail", "decl_fail", "code_fail", "nan", "invalid_index", "invalid_run", "bad_layout")
        results = {}
        for mode in ("off", "early", "pristine", "absent", "budget") + qualified + failures:
            result = subprocess.run([str(out / "fixture.exe"), mode, str(shader)], cwd=out,
                                    env=environment, capture_output=True, text=True, check=True)
            results[mode] = result.stdout
        for mode in ("off", "early", "pristine", "absent"):
            assert "record=" not in results[mode], (mode, results[mode])
        assert "calls=0 releases=0" in results["off"]
        assert "calls=0 releases=0" in results["early"]
        for mode in qualified:
            text = results[mode]
            assert "status=native-constants-decl/source-reference" in text, (mode, text)
            assert "bytes=1872 fnv1a32=E1623B6B shaderMatch=1" in text
            assert "GPU-input-bytes=UNREADABLE-PRE-SUBMIT" in text
            assert "CPU-reference row=105" in text and "sourceVertex=2" in text
            assert "nativeC105=[0,0.75,0,0]" in text
            assert "nativeC140=[0.25,1.25,2.25,3.25]" in text
            assert "nativeC151=[12.5,13.5,14.5,15.5]" in text
            assert "releases=2" in text
        assert "CPU-reference row=104" not in results["qualified"]
        assert "CPU-reference row=104 runIndex=1 sourceVertex=1" in results["both"]
        assert "offset:24 type:5" in results["expanded"]
        for mode in failures:
            assert "status=INCONCLUSIVE" in results[mode], (mode, results[mode])
        assert "nativeC0=" not in results["palette_fail"]
        assert "nativeC148=" not in results["world_fail"]
        assert "nativeDecl[" not in results["decl_fail"]
        assert results["budget"].count("status=") == 12
        assert "record=13" not in results["budget"]
        assert "cap=12 reached" in results["budget"]
        for mode, result in results.items():
            print(f"PASS {mode}: {result.splitlines()[0]}")
        print(f"{len(results)} scenarios passed; production bodies; no D3D device/game created")


if __name__ == "__main__":
    main()
