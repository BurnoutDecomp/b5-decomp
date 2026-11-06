"""R167 raw score helper and retained typed assignment in final linked PPC PE."""
import argparse,hashlib,json,pathlib,runpy,subprocess
P=pathlib.Path;wave=P('D:/Reverse/BP-X360-Experimental-Artifacts/wave20');folder=wave/'r167-link';root=P('C:/Users/adri1/.codex/worktrees/bp360/native_lion');m=runpy.run_path(str(wave/'r156-layout-linked/verify_layout_linked.py'))['m'];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();read=lambda p:json.loads(p.read_text(encoding='utf8'));tree='54cb3dd0e5592344bdc40ea616b6c5800ba6c674';parent='135948fd62910b9dc8911e865cf476555d05460e';candidate='aa0986c0b7b33cc978b2945dd8fbc4577f181cf7';donor='19fb6d45b4d8f21153a0b7a7f2a3425d21d036da'
if not __debug__:raise RuntimeError('assertions required')
def verify(pinsha):
 pins=folder/'ROOT_LINK_PINS.json';assert sha(pins)==pinsha; pin=read(pins);head=pin['head'];assert pin['tree']==tree and pin['parent']==parent
 for k in ('completion','seal','invocation','result','pe','map','xex','image','diagnostics'):
  item=pin['pins'][k];assert sha(P(item['path']))==item['sha256'],k
 git=lambda *a:subprocess.check_output(['git','-C',str(root),*a]).decode().strip();assert git('rev-parse',head+'^{tree}')==tree and git('rev-parse',head+'^')==parent and git('rev-parse',candidate+'^{tree}')==tree
 baseline=wave/'r167-score-raw-preflight/PROOF.json';assert sha(baseline)=='bf104ace67295642938dfbba9388a16103e6d260882f780cd2571ad27ff7d8b8';proof=read(baseline);assert proof['state']=='pass'
 inv=read(folder/'invocation.json');result=read(folder/'result.json');image=read(folder/'image-verification.json');inventory=read(folder/'source-inventory.json');assert inventory['head']==head and result['exit_code']==0 and result['all_post_checks_pass'] and image['head']==head and image['magic']=='XEX2' and image['image_sha256']==pin['pins']['xex']['sha256']
 files=['src/GameSource/GameState/'+x for x in ('BrnGameStateSharedIO.h','BrnGameStateSharedIO.cpp','BrnGameStateModuleIO.h','BrnGameStateModuleIO.cpp')];rows={}
 for path in files:
  blob=git('rev-parse',head+':'+path);assert blob==git('rev-parse',donor+':'+path);item,=[x for x in inventory['files'] if x['path'].replace('\\','/').lower().endswith(path.lower())];f=P(item['path']);assert sha(f)==item['sha256']; assert f.read_bytes().replace(b'\r\n',b'\n')==subprocess.check_output(['git','-C',str(root),'show',head+':'+path]);rows[path]=blob
 address,load=m['linked_reader'](folder);funcs=[('raw_copy','src/GameSource/GameState/BrnGameStateSharedIO.cpp','?CopyFromNativeRecord@CarScoreData@GameStateModuleIO@BrnGameState@@QAAXPBE@Z',proof['copy_body_sha256']),('typed_assignment','src/GameSource/GameState/BrnGameStateSharedIO.cpp','??4CarScoreData@GameStateModuleIO@BrnGameState@@QAAAAV012@ABV012@@Z',proof['prior_assignment_body_sha256']),('unbound_accessor','src/GameSource/GameState/BrnGameStateModuleIO.cpp','?GetUnboundPlayerScoreBytes@OutputBuffer@GameStateModuleIO@BrnGameState@@QBAPBEXZ',None)]
 out={}
 for label,source,symbol,digest in funcs:
  item,=[x for x in inv['objects'] if x['source']==source];obj=P(item['object']);assert sha(obj)==item['sha256'];code,rels=m['collect'](obj,symbol);assert not rels
  if digest:assert hashlib.sha256(code).hexdigest()==digest
  else:assert code==bytes.fromhex('3c6300033863a3904e800020')
  va=address(symbol);linked=load(va,len(code));assert linked==code
  out[label]={'symbol':symbol,'va':hex(va),'bytes':len(code),'body_sha256':hashlib.sha256(code).hexdigest(),'selected_object_sha256':sha(obj)}
 assert proof['copied_bytes']==289 and proof['holes']==['0x46','0x47','0xbd','0xbe','0xbf','0xda','0xdb'] and proof['native_record_offset']=='0x2a390'
 return {'state':'pass','head':head,'tree':tree,'parent':parent,'root_pins_sha256':pinsha,'donor_proof_sha256':sha(baseline),'source_blobs':rows,'functions':out,'copied_bytes':289,'untouched_offsets':proof['holes'],'native_record_offset':'0x2a390','scope':'selected COFF and linked PE; no runtime or bridge-call-site proof'}
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--root-pins-sha',required=True);p=a.parse_args();result=verify(p.root_pins_sha);dest=P(__file__).parent/'R167_SCORE_LINKED.json'
 with dest.open('x',encoding='utf8') as f:json.dump(result,f,indent=2);f.write('\n')
 print(dest,sha(dest))
