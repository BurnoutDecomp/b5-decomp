import json,re,hashlib
from pathlib import Path
root=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/fma_scalar_luna_526adc')
outroot=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/fma_native_luna')
exports=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
sources={
 'inputpads':'src/GameShared/GameClasses/System/Input/CgsInputPads.cpp',
 'gpu_perfmon':'src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp'}
reports={k:outroot/k/'compile-results.json' for k in sources}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def extract_block(text, marker):
    start=text.find(marker)
    if start<0:raise AssertionError('missing asm function marker: '+marker)
    proc=text.find(' PROC NEAR',start)
    end=text.find(' ENDP',proc)
    if proc<0 or end<0:raise AssertionError('missing PROC/ENDP for '+marker)
    return text[proc:end+5]
def insns(block):
    out=[]
    for line in block.splitlines():
        m=re.match(r'\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]{8}\s+([a-z][a-z0-9]*)\s+(.*)',line)
        if m:out.append((m.group(1).lower(),m.group(2),line.strip()))
    return out
out={'base':'526adc27662c0c2aedf417980dcf824c989bb035','reports':{},'whole_cpp_codegen':{},'original_functions':{}}
for k,rel in sources.items():
    r=json.loads(reports[k].read_text(encoding='utf-8')); assert r['requested']==1 and r['passed']==1 and r['failed']==0
    row=r['results'][0]; assert row['passed'] and row['machine']==498
    asm=Path(row['assembly']).read_text(encoding='utf-8',errors='replace')
    out['reports'][k]={
      'report':str(reports[k]),'report_sha256':sha(reports[k]),'source':rel,'source_sha256':sha(root/rel),
      'object':row['object'],'object_sha256':sha(row['object']),'coff_machine':row['machine'],
      'assembly':row['assembly'],'assembly_sha256':sha(row['assembly']),'log':row['log'],'log_sha256':sha(row['log']),
      'compiler_command':row.get('command')}
    if k=='inputpads':
        fma=extract_block(asm,'?UpdateJoltEnvelope@InputPads@CgsInput@@')
        ops=insns(fma); fused=[x for x in ops if x[0]=='fmadds']
        assert len(fused)==2,('UpdateJoltEnvelope fmadds count',len(fused))
        names=[x[0] for x in ops]
        assert names.index('fdivs')<names.index('fmadds')
        assert names.index('fneg')<names.index('fdivs',names.index('fneg'))<names.index('fmadds',names.index('fneg'))
        caller=extract_block(asm,'?UpdatePadRumble@InputPads@CgsInput@@')
        calls=[x for x in insns(caller) if x[0]=='bl' and 'UpdateJoltEnvelope' in x[1]]
        assert len(calls)==4,('UpdatePadRumble direct call count',len(calls))
        out['whole_cpp_codegen'][k]={
         'mangled_signature':'?UpdateJoltEnvelope@InputPads@CgsInput@@AAAMABUJoltEnvelope@InputIO@2@M@Z',
         'compiled_fmadds':[x[2] for x in fused],
         'compiled_local_order':names,
         'direct_caller':'UpdatePadRumble in same CPP','compiled_direct_call_count':len(calls),
         'ARTIST_function':'0x828E7618','ARTIST_caller':'0x828EFC58',
         'ARTIST_fmadds':['0x828E76A0','0x828E76EC'],
         'original_PPC_ABI':'r3=this, r4=envelope pointer, f1=time; caller consumes result from f1'}
    else:
        table=extract_block(asm,'?RenderPerformanceTable@DebugComponentPerfMonGpu@CgsDev@@')
        graph=extract_block(asm,'?RenderPerformanceGraph@DebugComponentPerfMonGpu@CgsDev@@')
        hud=extract_block(asm,'?RenderHUD@DebugComponentPerfMonGpu@CgsDev@@')
        tabs=[x for x in insns(table) if x[0]=='fnmsubs']
        gops=[x for x in insns(graph) if x[0]=='fmadds']
        assert len(tabs)==1,('table fnmsubs count',len(tabs))
        assert len(gops)==2,('graph fmadds count',len(gops))
        hcalls=[x for x in insns(hud) if x[0] in ('b','bl') and ('RenderPerformanceGraph' in x[1] or 'RenderPerformanceTable' in x[1])]
        assert len(hcalls)==2,('RenderHUD direct branch count',len(hcalls))
        out['whole_cpp_codegen'][k]={
         'compiled_table_fnmsubs':[x[2] for x in tabs],
         'compiled_graph_fmadds':[x[2] for x in gops],
         'compiled_RenderHUD_direct_branches':[x[2] for x in hcalls],
         'ARTIST_functions':{'table':'0x8282CDF0','graph':'0x8282CEA0','RenderHUD':'0x8282F520'},
         'ARTIST_operations':{'table_fnmsubs':'0x8282CE58','graph_fmadds':['0x8282CF7C','0x8282D184']},
         'ABI':'unchanged member methods; RenderHUD in same CPP dispatches to both'}
for addr in ('0x828E7618','0x828EFC58','0x8282CDF0','0x8282CEA0','0x8282F520'):
    p=exports/(addr+'.json'); out['original_functions'][addr]={'path':str(p),'sha256':sha(p),'name':json.loads(p.read_text(encoding='utf-8')).get('name')}
ver=outroot/'whole_cpp_codegen_validation.json'
ver.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8')
print('PASS whole CPP XDK objects: InputPads and GPU PerfMon')
print('InputPads: 2 fmadds, ordered fneg/div/fmadds release path, 4 compiled direct caller bls')
print('PerfMon: 1 fnmsubs, 2 fmadds, RenderHUD branches to graph/table')
print('validation',ver,'sha256',sha(ver))
