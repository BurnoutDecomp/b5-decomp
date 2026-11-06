import argparse,ast,hashlib,json,pathlib,subprocess,sys
P=pathlib.Path; H=P(__file__).resolve().parent; WAVE=H.parent; B=WAVE/'r160-batch'; R=WAVE/'r159-intake-v2'; L=WAVE/'r160-link'; W=P('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'); BASE='2fcdc0e829479059db82e0973d28db450c204869'; BASE_TREE='213435d590050d1fadd612a2e923ce1b58720e0a'
def sha(p):
 with P(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(P(p).read_text(encoding='utf-8-sig'))
def git(*args):return subprocess.check_output(['git','--no-optional-locks',*args],cwd=W).decode().strip()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--manifest-sha256',required=True);ap.add_argument('--patch-sha256',required=True);ap.add_argument('--candidate',required=True);ap.add_argument('--tree',required=True);args=ap.parse_args()
 for h in (args.manifest_sha256,args.patch_sha256):assert len(h)==64 and all(c in '0123456789abcdef' for c in h)
 manifest=B/'MANIFEST.json';patch=B/'r160.patch';assert sha(manifest)==args.manifest_sha256 and sha(patch)==args.patch_sha256
 m=read(manifest);assert m['base']==BASE and m['base_tree']==BASE_TREE and m['patch']['sha256']==args.patch_sha256 and m['expected_total_native']==3710
 assert git('rev-parse','HEAD')==BASE and git('rev-parse',BASE+'^{tree}')==BASE_TREE and not git('status','--porcelain=v1') and not git('branch','--show-current')
 assert m['candidate']==args.candidate and m['projected_tree']==args.tree and git('rev-list','--parents','-n','1',m['candidate']).split()==[m['candidate'],BASE] and git('rev-parse',m['candidate']+'^{tree}')==m['projected_tree']
 entries={x['path']:x for x in m['final_paths']}; assert len(entries)==len(m['final_paths'])==4 and all(x['new'] for x in entries.values()) and {p for p,x in entries.items() if x['old'] is None}==set()
 expected={'src/GameSource/Game/BrnGameModule.hpp','src/GameSource/Game/GameBridgeGUIToX_GameState.cpp','src/GameShared/GameClasses/System/FileSystem/CgsPatchManager.cpp','src/GameShared/GameClasses/System/FileSystem/CgsPatchManager.h'}
 assert set(entries)==expected==set(git('diff','--name-only',BASE,m['candidate']).splitlines())
 for path,x in entries.items():assert (git('ls-tree',BASE,'--',path).split('\t')[0] or None)==x['old'] and git('ls-tree',m['candidate'],'--',path).split('\t')[0]==x['new']
 subprocess.run(['git','--no-optional-locks','-c','core.whitespace=cr-at-eol','apply','--check','--index',str(patch)],cwd=W,check=True)
 linkpins_path=WAVE/'r159-link/ROOT_LINK_PINS.json';assert sha(linkpins_path)=='0079e9f5142a38ce932b2e0197bf3ce1c2fb1eb01745b98648342ddb48154b8e';linkpins=read(linkpins_path);assert linkpins['head']==BASE and linkpins['tree']==BASE_TREE
 for item in linkpins['pins'].values():assert sha(item['path'])==item['sha256'],item['path']
 assert sha(R/'completion.json')=='2a4159cfcafc437ea0f9b8f5399cad4dd91286ec42a0ba7da4af6bb9fb39b205'
 done=read(R/'completion.json');assert done['head']==BASE and done['status']=='SEALED_NO_REGRESSIONS' and done['passed']==done['requested']==3710 and done['failed']==0 and sha(R/'composite-active-native.json')==done['report_sha256'] and sha(R/'composite-artifact-audit.json')==done['audit_sha256'] and read(R/'composite-artifact-audit.json')['head']==BASE
 assert P(m['base_compile']['path'])==R/'completion.json' and P(m['base_link']['path'])==WAVE/'r159-link/result.json' and P(m['base_seal']['path'])==WAVE/'r159-link/seal.json' and P(m['base_diagnostics']['path'])==WAVE/'r159-link/diagnostic-parsed.json'
 assert m['base_compile']['sha256']=='2a4159cfcafc437ea0f9b8f5399cad4dd91286ec42a0ba7da4af6bb9fb39b205' and m['base_link']['sha256']=='ea166dad2d1d54a4be6ad89c5bcb9701aa45c8bf9f87ed5a455d5afdd4d90696' and m['base_seal']['sha256']=='2e4d481d697a38e2bc65ae8b89cca92ae76b127bc1895b5f5e2079dd51e1e475' and m['base_diagnostics']['sha256']=='65280cf02e87ca107425247d13375b1e776af2c85e2eefec6909538b3421b7aa'
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
 assert sha(H/'guard-baseline.json')=='a97fead481771c7d8c3b3f9d9b0199ef10c6348fed59341db0060e35d7d705ca'
 for g in read(H/'guard-baseline.json')['rows']:
  assert g['gate_satisfied'] and sha(g['origin'])==g['origin_sha256'] and sha(g['source'])==g['source_sha256']
  for path,h in list(g['actual_included_headers'].items())+list(g['artifacts'].items()):assert sha(path)==h,path
 provider=read(WAVE/'r147-xmedia-provider-proof/PROVIDER_SELECTION.json');assert sha(WAVE/'r147-xmedia-provider-proof/PROVIDER_SELECTION.json')=='c5c92ccf8c16714ff97a55d668503d9bcec7279687c1a54cd6ec5dca6e2c9833'
 prior_result=read(WAVE/'r159-link/result.json');prior_diag=read(WAVE/'r159-link/diagnostic-parsed.json');assert prior_result['exit_code']==0 and prior_result['all_post_checks_pass'] and not prior_diag['unique_symbols'] and not prior_diag['duplicate_messages']
 prior=read(WAVE/'r159-link/seal.json');assert prior['head']==BASE and prior['provider_cohort']==provider['sources'] and len(prior['selected'])==3589 and len(prior['excluded'])==121
 oldrows={x['source'].lower():x for x in read(R/'composite-active-native.json')['results']};excluded={x['source'].lower():x for x in prior['excluded']}
 for x in provider['sources']:
  row=oldrows[x['source'].lower()];ex=excluded[x['source'].lower()];assert P(x['object'])==P(row['object'])==P(ex['object']) and x['sha256']==ex['sha256']==sha(x['object'])
 for kind,key in [('netconn','r26_origin_sha256'),('loading','r29_origin_sha256'),('entropy','r28_origin_sha256')]:
  origin=read(H/(kind+'-retention-origin.json'));binding=read(H/(kind+'-check-binding.json'));assert binding[key]==sha(H/(kind+'-retention-origin.json')) and origin['completion_sha256']==sha(R/'completion.json') and origin['report_sha256']==sha(origin['report'])
 for name in ('prepare_seal_v3.py','run_once_v3.py','release_reviewed_v3.py'):assert (L/name).is_file(), L/name
 assert not (H/'projection.json').exists() and not (H/'PREPARATION_BINDING.json').exists()
 subprocess.run([sys.executable,'-X','utf8','-B',str(H/'project.py'),args.manifest_sha256],check=True)
 p=read(H/'projection.json');assert p['base']==BASE and p['manifest_sha256']==args.manifest_sha256 and set(p['changed_paths'])==expected and p['native_count']==p['previous_native_count']==3710
 assert p['new_native']==[] and len(p['selected_sources'])==47, 'Candidate closure drift'
 assert {g['name'] for g in p['named_guard_impacts'] if g['changed_source'] or g['changed_actual_headers']}==set()
 cohort={x['source'].lower() for x in provider['sources']};assert not cohort & {x.lower() for x in p['selected_sources']}
 for tr in p['prior_include_trace_origins']:assert sha(tr['path'])==tr['sha256']
 script=H/'integrate.py';text=script.read_text();assert text.count('PENDING_MANIFEST_SHA256')==text.count('PENDING_PATCH_SHA256')==1;text=text.replace('PENDING_MANIFEST_SHA256',args.manifest_sha256).replace('PENDING_PATCH_SHA256',args.patch_sha256);ast.parse(text);script.write_text(text,encoding='utf-8')
 files=sorted(list(H.glob('*.py'))+list(H.glob('*-binding.json'))+list(H.glob('*-origin.json'))+[H/'guard-baseline.json',H/'projection.json']+[L/'prepare_seal_v3.py',L/'run_once_v3.py',L/'release_reviewed_v3.py',L/'verify_xcontent_provider.py'])
 for f in files:
  if f.suffix=='.py':ast.parse(f.read_text(),filename=str(f))
 result={'base':BASE,'projected_tree':m['projected_tree'],'manifest_sha256':sha(manifest),'projection_sha256':sha(H/'projection.json'),'patch_sha256':sha(patch),'native_target':p['native_count'],'selected_count':len(p['selected_sources']),'retained_count':p['native_count']-len(p['selected_sources']),'prior_trace_count':len(p['prior_include_trace_origins']),'named_guards':p['named_guard_impacts'],'prepared_files':{str(f):sha(f) for f in files}}
 with (H/'PREPARATION_BINDING.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
 print('READY',sha(H/'PREPARATION_BINDING.json'),'selected',result['selected_count'],'retained',result['retained_count'],flush=True)
if __name__=='__main__':main()
