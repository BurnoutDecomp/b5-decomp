import argparse,ast,hashlib,json,pathlib,subprocess,sys
P=pathlib.Path; H=P(__file__).resolve().parent; WAVE=H.parent; B=WAVE/'r158-batch'; R=WAVE/'r157-intake'; L=WAVE/'r158-link'; W=P('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'); BASE='e08c99a8a0902da7998408f8e324843ec86a3fae'; BASE_TREE='41e8700b3dbef3308250b2da6c0be090e9563b0a'
def sha(p):
 with P(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(P(p).read_text(encoding='utf-8-sig'))
def git(*args):return subprocess.check_output(['git','--no-optional-locks',*args],cwd=W).decode().strip()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--manifest-sha256',required=True);ap.add_argument('--patch-sha256',required=True);ap.add_argument('--candidate',required=True);ap.add_argument('--tree',required=True);args=ap.parse_args()
 for h in (args.manifest_sha256,args.patch_sha256):assert len(h)==64 and all(c in '0123456789abcdef' for c in h)
 manifest=B/'MANIFEST.json';patch=B/'r158.patch';assert sha(manifest)==args.manifest_sha256 and sha(patch)==args.patch_sha256
 m=read(manifest);assert m['base']==BASE and m['base_tree']==BASE_TREE and m['patch']['sha256']==args.patch_sha256 and m['expected_total_native']==3710
 assert git('rev-parse','HEAD')==BASE and git('rev-parse',BASE+'^{tree}')==BASE_TREE and not git('status','--porcelain=v1') and not git('branch','--show-current')
 assert m['candidate']==args.candidate and m['projected_tree']==args.tree and git('rev-list','--parents','-n','1',m['candidate']).split()==[m['candidate'],BASE] and git('rev-parse',m['candidate']+'^{tree}')==m['projected_tree']
 entries={x['path']:x for x in m['final_paths']}; assert len(entries)==len(m['final_paths'])==9 and all(x['new'] for x in entries.values()) and {p for p,x in entries.items() if x['old'] is None}==set()
 expected={'src/GameSource/Resource/BrnDLCManager.cpp', 'src/GameSource/Resource/BrnGameDataModule.cpp', 'src/GameSource/Resource/BrnDLCManagerMonitor.cpp', 'src/GameSource/Resource/BrnGameDataModule.h', 'src/GameShared/GameClasses/System/FileSystem/CgsDeviceAsyncOp.cpp', 'src/GameShared/GameClasses/System/FileSystem/CgsDeviceAsyncOp.h', 'src/GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceAsyncOp.cpp', 'src/GameSource/Resource/BrnDLCManager.h', 'src/GameSource/Game/GameBridgeGUIToX_GameState.cpp'}
 assert set(entries)==expected==set(git('diff','--name-only',BASE,m['candidate']).splitlines()) and sum(path.endswith('.h') for path in entries)==3
 for path,x in entries.items():assert (git('ls-tree',BASE,'--',path).split('\t')[0] or None)==x['old'] and git('ls-tree',m['candidate'],'--',path).split('\t')[0]==x['new']
 subprocess.run(['git','--no-optional-locks','-c','core.whitespace=cr-at-eol','apply','--check','--index',str(patch)],cwd=W,check=True)
 assert sha(R/'completion.json')=='138c67b429421794d64b5c7a60afee91f3bac5de3ea853a94ad6a30fcfdf2e60'
 done=read(R/'completion.json');assert done['head']==BASE and done['status']=='SEALED_NO_REGRESSIONS' and done['passed']==done['requested']==3710 and done['failed']==0 and sha(R/'composite-active-native.json')==done['report_sha256'] and sha(R/'composite-artifact-audit.json')==done['audit_sha256'] and read(R/'composite-artifact-audit.json')['head']==BASE
 assert P(m['base_compile']['path'])==R/'completion.json' and P(m['base_link']['path'])==WAVE/'r157-link/result.json' and P(m['base_seal']['path'])==WAVE/'r157-link/seal.json' and P(m['base_diagnostics']['path'])==WAVE/'r157-link/diagnostic-parsed.json'
 assert m['base_compile']['sha256']=='138c67b429421794d64b5c7a60afee91f3bac5de3ea853a94ad6a30fcfdf2e60' and m['base_link']['sha256']=='8e72a98ac54b2c47e3d7ff426c758b4353c8a099720d158f412b43ff2efe669d' and m['base_seal']['sha256']=='c9136354e375d71c6f3b699d8f38eb380a47e6aae3be9d854ce7eae88474262a' and m['base_diagnostics']['sha256']=='7354ec782b43fb97b49d4e1d948a39c64aca47ff1cc8a8eb5e964f81bc65097d'
 refs=[m[k] for k in ('base_compile','base_link','base_seal','base_diagnostics','generated_loading_contract','base_loading_coff','original_launch_trace','inherited_sdk_provider')]+m.get('composed_receipts',[])+[v for g in m['groups'] for v in [g['patch']]+g.get('receipts',[])]
 for x in refs:assert sha(x['path'])==x['sha256'],x['path']
 target_path=B/'TARGETED_OBJECT_BINDING.json'
 assert any(P(x['path'])==target_path for x in refs)
 targeted=read(target_path)
 assert targeted['head']==m['candidate'] and targeted['tree']==m['projected_tree']
 report=targeted['compile_report'];assert sha(report['path'])==report['sha256']
 compiled=read(report['path']);assert compiled['target']=='xbox360-ppc-be' and compiled['requested']==compiled['passed'] and compiled['passed']>0 and compiled['failed']==0
 assert len(targeted['results'])==len({row['source'] for row in targeted['results']})==compiled['passed']
 by_source={r['source']:r for r in compiled['results']};assert set(by_source)=={row['source'] for row in targeted['results']}
 for row in targeted['results']:
  actual=by_source[row['source']];assert actual['passed'] and actual['returncode']==0 and actual['machine']==row['machine']==498
  assert git('rev-parse',m['candidate']+':'+row['source'])==row['git_blob']
  assert set(row['artifacts'])=={'object','assembly','log'}
  for kind,artifact in row['artifacts'].items():assert P(actual[kind])==P(artifact['path']) and sha(artifact['path'])==artifact['sha256']
 assert sha(H/'guard-baseline.json')=='5286d8edb9dd979ac26ea6ba005dd41c5c88a3d0ed7f8f6ed45878c96f61152a'
 for g in read(H/'guard-baseline.json')['rows']:
  assert g['gate_satisfied'] and sha(g['origin'])==g['origin_sha256'] and sha(g['source'])==g['source_sha256']
  for path,h in list(g['actual_included_headers'].items())+list(g['artifacts'].items()):assert sha(path)==h,path
 provider=read(WAVE/'r147-xmedia-provider-proof/PROVIDER_SELECTION.json');assert sha(WAVE/'r147-xmedia-provider-proof/PROVIDER_SELECTION.json')=='c5c92ccf8c16714ff97a55d668503d9bcec7279687c1a54cd6ec5dca6e2c9833'
 prior_result=read(WAVE/'r157-link/result.json');prior_diag=read(WAVE/'r157-link/diagnostic-parsed.json');assert prior_result['exit_code']==0 and prior_result['all_post_checks_pass'] and not prior_diag['unique_symbols'] and not prior_diag['duplicate_messages']
 prior=read(WAVE/'r157-link/seal.json');assert prior['head']==BASE and prior['provider_cohort']==provider['sources'] and len(prior['selected'])==3589 and len(prior['excluded'])==121
 oldrows={x['source'].lower():x for x in read(R/'composite-active-native.json')['results']};excluded={x['source'].lower():x for x in prior['excluded']}
 for x in provider['sources']:
  row=oldrows[x['source'].lower()];ex=excluded[x['source'].lower()];assert P(x['object'])==P(row['object'])==P(ex['object']) and x['sha256']==ex['sha256']==sha(x['object'])
 for kind,key in [('netconn','r26_origin_sha256'),('loading','r29_origin_sha256'),('entropy','r28_origin_sha256')]:
  origin=read(H/(kind+'-retention-origin.json'));binding=read(H/(kind+'-check-binding.json'));assert binding[key]==sha(H/(kind+'-retention-origin.json')) and origin['completion_sha256']==sha(R/'completion.json') and origin['report_sha256']==sha(origin['report'])
 for name in ('prepare_seal_v2.py','run_once_v2.py','release_reviewed_v2.py'):assert (L/name).is_file(), L/name
 assert not (H/'projection.json').exists() and not (H/'PREPARATION_BINDING.json').exists()
 subprocess.run([sys.executable,'-X','utf8','-B',str(H/'project.py'),args.manifest_sha256],check=True)
 p=read(H/'projection.json');assert p['base']==BASE and p['manifest_sha256']==args.manifest_sha256 and set(p['changed_paths'])==expected and p['native_count']==p['previous_native_count']==3710
 assert p['new_native']==[] and len(p['selected_sources'])==47, 'Candidate closure drift'
 assert {g['name'] for g in p['named_guard_impacts'] if g['changed_source'] or g['changed_actual_headers']}==set()
 cohort={x['source'].lower() for x in provider['sources']};assert not cohort & {x.lower() for x in p['selected_sources']}
 for tr in p['prior_include_trace_origins']:assert sha(tr['path'])==tr['sha256']
 script=H/'integrate.py';text=script.read_text();assert text.count('PENDING_MANIFEST_SHA256')==text.count('PENDING_PATCH_SHA256')==1;text=text.replace('PENDING_MANIFEST_SHA256',args.manifest_sha256).replace('PENDING_PATCH_SHA256',args.patch_sha256);ast.parse(text);script.write_text(text,encoding='utf-8')
 files=sorted(list(H.glob('*.py'))+list(H.glob('*-binding.json'))+list(H.glob('*-origin.json'))+[H/'guard-baseline.json',H/'projection.json']+[L/'prepare_seal_v2.py',L/'run_once_v2.py',L/'release_reviewed_v2.py',L/'verify_xcontent_provider.py'])
 for f in files:
  if f.suffix=='.py':ast.parse(f.read_text(),filename=str(f))
 result={'base':BASE,'projected_tree':m['projected_tree'],'manifest_sha256':sha(manifest),'projection_sha256':sha(H/'projection.json'),'patch_sha256':sha(patch),'native_target':p['native_count'],'selected_count':len(p['selected_sources']),'retained_count':p['native_count']-len(p['selected_sources']),'prior_trace_count':len(p['prior_include_trace_origins']),'named_guards':p['named_guard_impacts'],'prepared_files':{str(f):sha(f) for f in files}}
 with (H/'PREPARATION_BINDING.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
 print('READY',sha(H/'PREPARATION_BINDING.json'),'selected',result['selected_count'],'retained',result['retained_count'],flush=True)
if __name__=='__main__':main()
