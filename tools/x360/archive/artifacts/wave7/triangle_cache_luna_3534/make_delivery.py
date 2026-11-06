import hashlib, json, re, subprocess
from pathlib import Path

work=Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\triangle_cache_luna_3534')
art=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\triangle_cache_luna_3534')
exports=Path(r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX')
xdk=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK')
source_rel='src/GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManager_Update.cpp'
source=work/source_rel
compile_dir=art/'repair-pass1'/'objects'
caller_dir=art/'caller-provider-closure'/'objects'
baseline_dir=art/'baseline'/'objects'
compile_report=json.loads((compile_dir/'compile-results.json').read_text(encoding='utf-8'))
caller_report=json.loads((caller_dir/'compile-results.json').read_text(encoding='utf-8'))
baseline_report=json.loads((baseline_dir/'compile-results.json').read_text(encoding='utf-8'))
assert compile_report['state']=='complete' and compile_report['requested']==1 and compile_report['passed']==1 and compile_report['failed']==0
assert caller_report['state']=='complete' and caller_report['requested']==3 and caller_report['passed']==3 and caller_report['failed']==0
assert baseline_report['state']=='complete' and baseline_report['failed']==1

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git_bytes(*args): return subprocess.check_output(['git',*args],cwd=work)
def read_export(address):
 p=exports/(address+'.json'); return p,json.loads(p.read_text(encoding='utf-8'))
def asm_symbols(path):
 pub,ext=set(),set()
 for line in Path(path).read_text(encoding='utf-8',errors='replace').splitlines():
  f=line.strip().split()
  if len(f)<2: continue
  if f[0]=='PUBLIC': pub.add(f[1])
  elif f[0]=='EXTRN': ext.add(f[1].split(':')[0])
 return pub,ext

current_rows={r['source']:r for r in compile_report['results']}
caller_rows={r['source']:r for r in caller_report['results']}
row=next(iter(compile_report['results']))
caller_source='src/GameShared/GameClasses/SceneManager/CgsSceneManagerModule.cpp'
interface_source='src/GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.cpp'
render_source='src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.cpp'
assert set(caller_rows)=={caller_source,interface_source,render_source}
main_pub,main_ext=asm_symbols(row['assembly'])
caller_pub,caller_ext=asm_symbols(caller_rows[caller_source]['assembly'])
interface_pub,interface_ext=asm_symbols(caller_rows[interface_source]['assembly'])
render_pub,render_ext=asm_symbols(caller_rows[render_source]['assembly'])
start_sym=next(s for s in main_pub if s.startswith('?StartUpdateTriangleCaches@TriangleCacheManager@CgsSceneManager@@'))
end_sym=next(s for s in main_pub if s.startswith('?EndUpdateTriangleCaches@TriangleCacheManager@CgsSceneManager@@'))
assert start_sym in caller_ext and end_sym in caller_ext
for sym in ['??0DebugInterface@CgsDev@@QAA@XZ','??1DebugInterface@CgsDev@@QAA@XZ','?Get2dRender@DebugInterface@CgsDev@@QAAAAVDebugRender@2@XZ']:
 assert sym in main_ext and sym in interface_pub
assert '?Draw2DText@DebugRender@CgsDev@@QAAXPBDMMMI@Z' in main_ext
assert '?Draw2DText@DebugRender@CgsDev@@QAAXPBDMMMI@Z' in render_pub
assert 'sprintf' in main_ext

text=source.read_text(encoding='utf-8')
for removed in ['getenv','sqrtf','std::snprintf','s_uTriCacheProbeFrame','CgsDev::Log','gpDebugPrint']:
 assert removed not in text, removed
assert '::sprintf(lacMessage' in text

addresses=['0x828BECF8','0x828BF150','0x82C0CB70']
artist={}
for addr in addresses:
 p,d=read_export(addr)
 artist[addr]={'path':str(p),'sha256':sha(p),'name':d.get('name'),'prototype':d.get('prototype'),'pseudocode':d.get('pseudocode'),'xrefs_to':d.get('xrefs_to'),'assembly':d.get('assembly')}
original_end=artist['0x828BF150']['assembly'].splitlines()
original_relevant=[ln for ln in original_end if any(t in ln for t in ['0x828BF5','0x828BF6','sub_82C0CB70','Draw2DText','82BF64','82BF65','82BF66','82BF67'])]
crt_header=xdk/'include/xbox/stdio.h'
header_lines=crt_header.read_text(encoding='utf-8',errors='replace').splitlines()
sprintf_declarations=[{'line':i+1,'text':ln.strip()} for i,ln in enumerate(header_lines) if 'sprintf' in ln and ('DEFINE_CPP_OVERLOAD_STANDARD_FUNC_0_1_ARGLIST' in ln or 'sprintf(' in ln)][:8]
assert sprintf_declarations

base_blob=git_bytes('show',f'3534f7f16103a58ff904bbd1604e18e9934868f4:{source_rel}')
receipt={
 'status':'private source/object repair, not pushed or integrated',
 'workspace':str(work),
 'base':'3534f7f16103a58ff904bbd1604e18e9934868f4',
 'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip(),
 'commit':subprocess.check_output(['git','log','-1','--format=%H%n%s%n%(trailers:key=Co-Authored-By,valueonly)'],cwd=work,text=True).strip().splitlines(),
 'changed_paths':subprocess.check_output(['git','show','--format=','--name-only','HEAD'],cwd=work,text=True).splitlines(),
 'clean':not subprocess.check_output(['git','status','--porcelain'],cwd=work,text=True).strip(),
 'source':{'path':source_rel,'sha256':sha(source),'base_blob_content_sha256':hashlib.sha256(base_blob).hexdigest()},
 'whole_cpp_compile':{'report':str(compile_dir/'compile-results.json'),'report_sha256':sha(compile_dir/'compile-results.json'),'requested':compile_report['requested'],'passed':compile_report['passed'],'failed':compile_report['failed'],'object':row['object'],'object_sha256':sha(row['object']),'assembly':row['assembly'],'assembly_sha256':sha(row['assembly']),'log':row['log'],'log_sha256':sha(row['log']),'machine':row['machine']},
 'relevant_caller_provider_compile':{'report':str(caller_dir/'compile-results.json'),'report_sha256':sha(caller_dir/'compile-results.json'),'requested':caller_report['requested'],'passed':caller_report['passed'],'failed':caller_report['failed'],'artifacts':[],'symbol_bindings':{'StartUpdateTriangleCaches':start_sym,'EndUpdateTriangleCaches':end_sym,'DebugInterface_ctor_dtor_Get2dRender':'provider symbols all match','Draw2DText':'CgsDebugRender provider matches update-TU external'}},
 'crt_format_evidence':{
  'artist_overlay':'ARTIST EndUpdateTriangleCaches formats into the local string object by calling its variadic CRT formatter then passes that string to the debug render.',
  'original_end_relevant_assembly':original_relevant,
  'original_crt_export':{'path':artist['0x82C0CB70']['path'],'sha256':artist['0x82C0CB70']['sha256'],'prototype':artist['0x82C0CB70']['prototype'],'xrefs_to':artist['0x82C0CB70']['xrefs_to'],'role':'CRT variadic output routine called by EndUpdateTriangleCaches at 0x828BF644'},
  'xdk_stdio_header':{'path':str(crt_header),'sha256':sha(crt_header),'sprintf_declarations':sprintf_declarations},
  'compiled_call':{'external_symbol':'sprintf','assembly_external_and_call_lines':[ln.strip() for ln in Path(row['assembly']).read_text(encoding='utf-8',errors='replace').splitlines() if 'sprintf' in ln or 'Draw2DText' in ln], 'argument_order':'buffer r3, format r4, four double-promoted sphere values r5-r8, overflow int r9; compiled listing and ARTIST call sequence agree'},
  'format':'TRI CACHE OVERFLOW WITH SPHERE AT %.2f, %.2f, %.2f, RADIUS %.2f, OVERFLOW %d',
 },
 'removed_pc_probe_residue':{'in_base_source':True,'current_source_absence':['getenv','sqrtf','std::snprintf','s_uTriCacheProbeFrame','CgsDev::Log'],'summary':'This exact 3534 source had a write-only probe frame counter and probe-only includes/comments; no getenv/log/sqrt calls were present in executable code. Those unattested PC-only remnants were removed.'},
 'preserved_baseline_failure':{'report':str(baseline_dir/'compile-results.json'),'report_sha256':sha(baseline_dir/'compile-results.json'),'requested':baseline_report['requested'],'passed':baseline_report['passed'],'failed':baseline_report['failed'],'log':baseline_report['results'][0]['log'],'log_sha256':sha(baseline_report['results'][0]['log'])},
 'artist_exports':{a:{'path':artist[a]['path'],'sha256':artist[a]['sha256'],'name':artist[a]['name']} for a in addresses},
 'limits':['XDK compile, assembly and decorated-symbol evidence only; no final executable link, game, or hardware run.','No shared header or provider implementation was changed; the actual debug interface/render provider TUs were compile-checked.','The triangle cache empty-registry behavior and all cache/stream/perfmon/debug-overlay paths were otherwise preserved.']
}
for rel,r in caller_rows.items():
 receipt['relevant_caller_provider_compile']['artifacts'].append({'source':rel,'object':r['object'],'object_sha256':sha(r['object']),'assembly':r['assembly'],'assembly_sha256':sha(r['assembly']),'log':r['log'],'log_sha256':sha(r['log']),'machine':r['machine']})
path=art/'delivery.json'
path.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('delivery',path,'sha256',sha(path),'head',receipt['head'],'clean',receipt['clean'])
print('whole CPP',compile_report['passed'],'/',compile_report['requested'],'caller/provider',caller_report['passed'],'/',caller_report['requested'])
