import argparse,ast,hashlib,json,pathlib,subprocess,sys,xml.etree.ElementTree as ET
P=pathlib.Path; H=P(__file__).resolve().parent; WAVE=H.parent; B=WAVE/'r161-batch'; R=WAVE/'r160-intake'; L=WAVE/'r161-link'; W=P('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'); BASE='9a78c87e3e672f25e5717a718937218033037442'; BASE_TREE='8a22a7667f80a25663db24c362c61f019deab02e'
def sha(p):
 with P(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(P(p).read_text(encoding='utf-8-sig'))
def git(*args):return subprocess.check_output(['git','--no-optional-locks',*args],cwd=W).decode().strip()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--manifest-sha256',required=True);ap.add_argument('--patch-sha256',required=True);ap.add_argument('--candidate',required=True);ap.add_argument('--tree',required=True);args=ap.parse_args()
 for h in (args.manifest_sha256,args.patch_sha256):assert len(h)==64 and all(c in '0123456789abcdef' for c in h)
 config=P('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r161-execution-id-preflight/execution-id-raw.xml');assert sha(config)=='1f0cfbc288529b9ed060e371dd674a1137bf33c48cb6f9b80d22af955728048b';xml=ET.parse(config).getroot();assert xml.tag=='xex' and len(xml)==2 and xml[0].tag=='titleid' and xml[0].attrib=={'id':'0x45410806'} and xml[1].tag=='format' and [x.tag for x in xml[1]]==['raw','unencrypted'] and all(not x.attrib and len(x)==0 for x in xml[1])
 manifest=B/'MANIFEST.json';patch=B/'r161.patch';assert sha(manifest)==args.manifest_sha256 and sha(patch)==args.patch_sha256
 m=read(manifest);assert m['base']==BASE and m['base_tree']==BASE_TREE and m['patch']['sha256']==args.patch_sha256 and m['expected_total_native']==3710
 assert git('rev-parse','HEAD')==BASE and git('rev-parse',BASE+'^{tree}')==BASE_TREE and not git('status','--porcelain=v1') and not git('branch','--show-current')
 assert m['candidate']==args.candidate and m['projected_tree']==args.tree and git('rev-list','--parents','-n','1',m['candidate']).split()==[m['candidate'],BASE] and git('rev-parse',m['candidate']+'^{tree}')==m['projected_tree']
 entries={x['path']:x for x in m['final_paths']}; assert len(entries)==len(m['final_paths'])==PENDING_PATH_COUNT and all(x['new'] for x in entries.values()) and {p for p,x in entries.items() if x['old'] is None}==set()
 expected=PENDING_EXPECTED_PATHS
 assert set(entries)==expected==set(git('diff','--name-only',BASE,m['candidate']).splitlines())
 for path,x in entries.items():assert (git('ls-tree',BASE,'--',path).split('\t')[0] or None)==x['old'] and git('ls-tree',m['candidate'],'--',path).split('\t')[0]==x['new']
 subprocess.run(['git','--no-optional-locks','-c','core.whitespace=cr-at-eol','apply','--check','--index',str(patch)],cwd=W,check=True)
 linkpins_path=WAVE/'r160-link/ROOT_LINK_PINS.json';assert sha(linkpins_path)=='b20d0c36bd34353316d09b7cc1d447a5f1825146c56a1aeda62d7b558bf473d0';linkpins=read(linkpins_path);assert linkpins['head']==BASE and linkpins['tree']==BASE_TREE
 for item in linkpins['pins'].values():assert sha(item['path'])==item['sha256'],item['path']
 assert sha(R/'completion.json')=='caa5d8b2941e270bf3fda49bd2e808871b303b7be2576658ecd5074f056589e5'
 done=read(R/'completion.json');assert done['head']==BASE and done['status']=='SEALED_NO_REGRESSIONS' and done['passed']==done['requested']==3710 and done['failed']==0 and sha(R/'composite-active-native.json')==done['report_sha256'] and sha(R/'composite-artifact-audit.json')==done['audit_sha256'] and read(R/'composite-artifact-audit.json')['head']==BASE
 assert P(m['base_compile']['path'])==R/'completion.json' and P(m['base_link']['path'])==WAVE/'r160-link/result.json' and P(m['base_seal']['path'])==WAVE/'r160-link/seal.json' and P(m['base_diagnostics']['path'])==WAVE/'r160-link/diagnostic-parsed.json'
 assert m['base_compile']['sha256']=='caa5d8b2941e270bf3fda49bd2e808871b303b7be2576658ecd5074f056589e5' and m['base_link']['sha256']=='eb60f4578264cca05cbb2fda28f6a24b0c5e130dcfb1ef54f614d419be18ee18' and m['base_seal']['sha256']=='9bae3435dad1c4e52311e555431c5ced4ba8943d34bb4572db1be25216a71e23' and m['base_diagnostics']['sha256']=='c49174d4ef7741b9a15a3698f57ab5a51feb368276a6c067f486f7eed4df6e7a'
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
 assert sha(H/'guard-baseline.json')=='93c8e719af36876dca535ff3a07ba0d0b760f3abd401b622e145e3b32305892b'
 for g in read(H/'guard-baseline.json')['rows']:
  assert g['gate_satisfied'] and sha(g['origin'])==g['origin_sha256'] and sha(g['source'])==g['source_sha256']
  for path,h in list(g['actual_included_headers'].items())+list(g['artifacts'].items()):assert sha(path)==h,path
 provider=read(WAVE/'r147-xmedia-provider-proof/PROVIDER_SELECTION.json');assert sha(WAVE/'r147-xmedia-provider-proof/PROVIDER_SELECTION.json')=='c5c92ccf8c16714ff97a55d668503d9bcec7279687c1a54cd6ec5dca6e2c9833'
 prior_result=read(WAVE/'r160-link/result.json');prior_diag=read(WAVE/'r160-link/diagnostic-parsed.json');assert prior_result['exit_code']==0 and prior_result['all_post_checks_pass'] and not prior_diag['unique_symbols'] and not prior_diag['duplicate_messages']
 prior=read(WAVE/'r160-link/seal.json');assert prior['head']==BASE and prior['provider_cohort']==provider['sources'] and len(prior['selected'])==3589 and len(prior['excluded'])==121
 oldrows={x['source'].lower():x for x in read(R/'composite-active-native.json')['results']};excluded={x['source'].lower():x for x in prior['excluded']}
 for x in provider['sources']:
  row=oldrows[x['source'].lower()];ex=excluded[x['source'].lower()];assert P(x['object'])==P(row['object'])==P(ex['object']) and x['sha256']==ex['sha256']==sha(x['object'])
 for kind,key in [('netconn','r26_origin_sha256'),('loading','r29_origin_sha256'),('entropy','r28_origin_sha256')]:
  origin=read(H/(kind+'-retention-origin.json'));binding=read(H/(kind+'-check-binding.json'));assert binding[key]==sha(H/(kind+'-retention-origin.json')) and origin['completion_sha256']==sha(R/'completion.json') and origin['report_sha256']==sha(origin['report'])
 for name in ('prepare_seal_v3.py','run_once_v3.py','release_reviewed_v3.py'):assert (L/name).is_file(), L/name
 assert sha(P('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r161-execution-id-preflight/verify_native_title_xex.py'))=='b759f3cb40476040cccbbe43561d9e0e2d232230c7e5f294715687e1e4ebcb6b'
 assert not (H/'projection.json').exists() and not (H/'PREPARATION_BINDING.json').exists()
 subprocess.run([sys.executable,'-X','utf8','-B',str(H/'project.py'),args.manifest_sha256],check=True)
 p=read(H/'projection.json');assert p['base']==BASE and p['manifest_sha256']==args.manifest_sha256 and set(p['changed_paths'])==expected and p['native_count']==p['previous_native_count']==3710
 assert p['new_native']==[] and len(p['selected_sources'])==PENDING_SELECTED_COUNT, 'Candidate closure drift'
 assert {g['name'] for g in p['named_guard_impacts'] if g['changed_source'] or g['changed_actual_headers']}==PENDING_GUARD_SET
 cohort={x['source'].lower() for x in provider['sources']};assert not cohort & {x.lower() for x in p['selected_sources']}
 for tr in p['prior_include_trace_origins']:assert sha(tr['path'])==tr['sha256']
 script=H/'integrate.py';text=script.read_text();assert text.count('PENDING_MANIFEST_SHA256')==text.count('PENDING_PATCH_SHA256')==1;text=text.replace('PENDING_MANIFEST_SHA256',args.manifest_sha256).replace('PENDING_PATCH_SHA256',args.patch_sha256);ast.parse(text);script.write_text(text,encoding='utf-8')
 files=sorted(list(H.glob('*.py'))+list(H.glob('*-binding.json'))+list(H.glob('*-origin.json'))+[H/'guard-baseline.json',H/'projection.json']+[L/'prepare_seal_v3.py',L/'run_once_v3.py',L/'release_reviewed_v3.py',L/'verify_xcontent_provider.py',P('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r161-execution-id-preflight/verify_native_title_xex.py'),config])
 for f in files:
  if f.suffix=='.py':ast.parse(f.read_text(),filename=str(f))
 result={'base':BASE,'projected_tree':m['projected_tree'],'manifest_sha256':sha(manifest),'projection_sha256':sha(H/'projection.json'),'patch_sha256':sha(patch),'native_target':p['native_count'],'selected_count':len(p['selected_sources']),'retained_count':p['native_count']-len(p['selected_sources']),'prior_trace_count':len(p['prior_include_trace_origins']),'named_guards':p['named_guard_impacts'],'prepared_files':{str(f):sha(f) for f in files}}
 with (H/'PREPARATION_BINDING.json').open('x',encoding='utf-8') as f:json.dump(result,f,indent=2);f.write('\n')
 print('READY',sha(H/'PREPARATION_BINDING.json'),'selected',result['selected_count'],'retained',result['retained_count'],flush=True)
if __name__=='__main__':main()
