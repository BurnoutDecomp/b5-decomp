from pathlib import Path
import re
root=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_lion/src/SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime')
def load(p): return p.read_bytes().decode('utf-8').replace('\r\n','\n')
def save(p,s): p.write_bytes(s.replace('\n','\r\n').encode('utf-8'))
def cut(s,start,end):
 a=s.index(start); b=s.index(end,a); return s[:a]+s[b:]
p=root/'include/LionFX.cpp'; s=load(p)
s=cut(s,'    // ---- [lionfx] witness.', '}\n\n// ================================================================================================\n// cLionFX::Update')
s=cut(s,'    // ------------------------------------------------------------------------\n    // The one assumption the whole .lef graph rests on', '    // Self-relative byte offset -> pointer.')
s=cut(s,'    // ---- [lionload] witness.', '    return lpDefinition;')
s=s.replace('#include <cstdio>\n','')
s=re.sub(r'#include "GameShared/GameClasses/Development/Log/CgsLog.h"[^\n]*\n','',s)
save(p,s)
p=root/'include/ParticleEmitter.cpp'; s=load(p)
s=cut(s,'// ------------------------------------------------------------------------------------------\n// [diag] DELETE-WHEN-STABLE', '// The console walks the behaviour chain link by link FIVE times')
a=s.index('                    // ⛔ BRN_LION_NOBLEND=1'); b=s.index('\n                }\n            }',a)
s=s[:a]+'''                    // ARTIST 0x8290F9B8..0x8290F9C0: interpolate, then publish the temp layer.
                    mpTempBehaviour->Lerp(lpLo, lpHi, lfFraction);
                    lpChosen = mpTempBehaviour;'''+s[b:]
s=cut(s,'    // [lionspawn] ONE-SHOT bring-up witness.', '    // asm 0x82914EDC..0x82914F68')
s=s.replace('        return cVector{ arV.x, arV.y, arV.z, arV.w };','        const cVector lResult = { arV.x, arV.y, arV.z, arV.w };\n        return lResult;')
s=s.replace('    arParticleNucleus.mvLifeTimeAndFrameTimeAndFPSAndBirthTime = cVector{ 0.0f, 0.0f, 0.0f, 0.0f };','    const cVector lZero = { 0.0f, 0.0f, 0.0f, 0.0f };\n    arParticleNucleus.mvLifeTimeAndFrameTimeAndFPSAndBirthTime = lZero;')
# Retain aggregate cVector shape; stage assignment through a named C++03 aggregate.
s=re.sub(r'(?m)^(\s*)([A-Za-z][A-Za-z0-9_.]*) = (\{[^;]+\});',lambda m:m[1]+'{ const cVector lRow = '+m[3]+'; '+m[2]+' = lRow; }',s)
s=s.replace('#include <cstdio>\n','#include <vectorintrinsics.h>   // native VMX round-to-nearest (vrfin)\n')
s=s.replace('    const f32 lfRounded = std::nearbyintf(afAngle * KF_ONE_OVER_2PI);','    const __vector4 lScaled = { afAngle * KF_ONE_OVER_2PI, 0.0f, 0.0f, 0.0f };\n    const __vector4 lRounded = __vrfin(lScaled);\n    const f32 lfRounded = lRounded.x;')
s=re.sub(r'#include "GameShared/GameClasses/Development/Log/CgsLog.h"[^\n]*\n','',s)
save(p,s)
p=root/'source/ParticleRender/ParticleRender.cpp'; s=load(p)
a=s.index('    {\n        static u32 suLastBatches'); b=s.index('    // Nothing to replay',a);s=s[:a]+s[b:]
a=s.index('        // ---- [liontex]');b=s.index('\n    }\n\n    mpRenderer->RenderGroupEnd();',a);s=s[:a]+s[b:]
a=s.index('        {\n            // [lionfx] ONE-SHOT');b=s.index('        EmitterRender(arVertexBuffer',a);s=s[:a]+s[b:]
s=re.sub(r'    u32 luLive =[^\n]*\n    u32 luCullRange =[^\n]*\n\n','',s)
s=re.sub(r'^\s*\+\+lu(?:Live|Cell|Inactive|Rendered);\n','',s,flags=re.M)
s=re.sub(r'            \+\+luCulled; \+\+luCull(?:Range|Frustum|Near);\n            if \(!sbNoCull\) continue;','            continue;',s)
s=s.replace('#include <cstdio>\n','').replace('#include <cstdlib>   // [lionfx] getenv -- the BRN_LIONFX_NOCULL bring-up bypass    // snprintf -- the [lionfx] bring-up witness\n','')
a=s.index('// --- Platform D3D device'); b=s.index('// ----------------------------------------------------------------------------\n// cParticleRender::Dispatch',a)
s=s[:a]+'''// Native XDK APIs carry the real D3D types and the 64-bit PendingMask3 argument.
#include <d3d9.h>
extern D3DDevice* gpD3DDevice;

static const D3DPRIMITIVETYPE KU_PARTICLE_PRIMITIVE_TYPE = D3DPT_QUADLIST;

'''+s[b:]
s=s.replace('gpD3DDevice, 0, apVertexBuffer,','gpD3DDevice, 0, reinterpret_cast<D3DVertexBuffer*>(apVertexBuffer),')
save(p,s)
