"""Read-only preparation of the exact R13 source graph; creates D artifacts only."""
from pathlib import Path
from datetime import datetime,timezone
import subprocess,json,hashlib
H=Path(__file__).resolve().parent;A=H.parent.parent;B=H.parent/'repair-r12-resume-20261103';W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake');P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
BASE='b92f3e9aaf87cafc07bd80a9748064f66204c603';ANCESTOR='e572f7db4c533546832e42c6f3f73149071b5f82';PUB='3534f7f16103a58ff904bbd1604e18e9934868f4'
groups=[('audio_cxx03_terra',['bdab51617b2bc4f27322c4a813d7fb08e506e9c5']),('scene_native_luna',['a0fef7a55e3046df7b7b043f6b3d48a08936a50e']),('d3d_native_terra',['b67796b2e903e470dd6ebb739915b5ca1d6e0635']),('guide-native-root',['250c001a7f60f9e5eb3d10928bd957af7030340d']),('world_native_luna',['ea9603d0f59ca98971d73bb4e99f6fd16161dd22']),('particles_native_terra',['e960cf1bbcc24ded829607a063c8bfe2a70617df','07298ed77310b3a4ca5f650c822fe020a5a41c35']),('gamestate_native_luna',['8803360c113d60f8c28a762560a4de74e1150d89','307d91681963d0a3fd6f5bd1087bc800e86a2181']),('camera_native_luna',['0a050ccdedce30073310cf47259c638670464163']),('gui_render_luna',['30528efaa546c64daddef7b7af20fd8ae5bb7a7b']),('debug_ui_native_luna',['80aed135fc97187b979dfe1e79b9887df2f7c1c5','bdccba575bc9ef7aaee4a1a46aeece47fc6e30df']),('geometry_native_luna',['506f0fefe8d76e51734ddd3b34b951b44bb888e4'])]
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def read(p):return json.loads(Path(p).read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
assert sha(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R12.json')=='c740af2f5a768bc6ebc6967126a217a2cd94224d9e2e0457329e0b47fa7d59bb'
assert sha(B/'native-parallel-wave8-r12-composite-effective.json')=='b9279b962abb678e4cad9e1c63a45015b8c7e155cd1d6b7f52312bfa608bf2e2'
assert sha(B/'composite-artifact-audit.json')=='883ab47f28445b0258b0f1dddd51375dd6d1a50ce1ce9d96b1d6c4ab4fb9a1aa'
baseline=read(B/'native-parallel-wave8-r12-composite-effective.json');old={r['source']:r for r in baseline['results']};images={};candidates=[];owners={}
for label,commits in groups:
 for i,c in enumerate(commits):
  parent=git('rev-parse',c+'^').decode().strip();assert parent==(commits[i-1]if i else ANCESTOR)
  receipt=A/'wave10'/label/('DELIVERY_SUCCESSOR.json'if i else 'DELIVERY.json');j=read(receipt)
  assert c in [j.get(k)for k in ['commit','final_commit','finalCommit']]
  changes=[]
  for line in git('diff-tree','--no-commit-id','--raw','--no-abbrev','-r',c).decode().splitlines():
   info,p=line.split('\t',1);_,_,a,b,status=info.split();assert status=='M'and p.endswith('.cpp')
   if p in images:assert owners[p]==label and images[p]['final']==a
   else:
    assert git('rev-parse',BASE+':'+p).decode().strip()==a and not old[p]['passed'];images[p]={'baseline':a,'final':a};owners[p]=label
   images[p]['final']=b;changes.append({'path':p,'preimage':a,'postimage':b,'status':status})
  candidates.append({'group':label,'commit':c,'parent':parent,'paths':changes,'receipt':str(receipt),'receipt_sha256':sha(receipt)})
paths=sorted(images);assert len(paths)==48 and len(candidates)==14 and len(groups)==11
plan={'state':'PREPARED_ONLY_NOT_AUTHORIZED','base':BASE,'candidate_ancestor':ANCESTOR,'published_base':PUB,'collect_now':[c['commit']for c in candidates],'candidates':candidates,'planned_source_images':images,'unique_paths':paths,'selected_sources':paths,'changed_headers':[],'new_paths':[],'removed_paths':[],'groups':11,'original_commits':14,'selected_R12_passes':0,'selected_R12_failures':48,'expected_fresh':48,'expected_retained':3568,'expected_passed':3333,'expected_failed':283,'native_inventory':3616,'text_inputs':6302,'jobs':4,'parent_gitlink_index':'160000 209fb3b94f7266506dff4ffd569e8553f1b3af93 0\tb5-decomp','required_chains':[cs for _,cs in groups if len(cs)>1],'limitations':['World/GUI author gates omitted some/all requested canonical fallback flags; fresh integrated48 must use all9 canonical vendor roots.','DebugUI80aed initial local256 buffer is rejected; exactbdcc successor restores the genuine global and must follow before any native compile.','Physics17path3commit cohort is outside R13.','No historical published artifact rehash/comparison,optional full sweep,link or owner suite.','Await exact finite final preflight seal and explicit Root authorization before any intake mutation.'],'created_utc':datetime.now(timezone.utc).isoformat()}
save(H/'R13_PREPARED_PLAN.json',plan)
C={'campaign':'R13 exact48 CPP-only batch','state':'PREPARATION_ONLY','base':BASE,'published_base':PUB,'workspace':str(W),'canonical':str(P/'b5-decomp'),'artifact_root':str(H),'plan':str(H/'R13_PREPARED_PLAN.json'),'authorization_required':str(H/'R13_ROOT_AUTHORIZATION.json'),'baseline_seal':str(B/'FROZEN_COMPOSITE_DIAGNOSTIC_R12.json'),'baseline_report':str(B/'native-parallel-wave8-r12-composite-effective.json'),'baseline_artifact_manifest':str(B/'composite-artifact-audit.json'),'baseline_metadata':str(B/'native-parallel-wave8-r12-targeted-stage1-patch.json'),'baseline_driver_command':str(B/'targeted-stage1-command.json'),'baseline_safeguard_receipt':str(B/'native-safeguard-current-reuse.json'),'baseline_passed':3285,'baseline_failed':331,'baseline_requested':3616,'fresh48':True,'no_published_comparison':True}
for key in ['plan','baseline_seal','baseline_report','baseline_artifact_manifest','baseline_metadata','baseline_driver_command','baseline_safeguard_receipt']:C[key+'_sha256']=sha(C[key])
save(H/'campaign.json',C);save(H/'preparation-receipt.json',{'state':'PREPARATION_ONLY','head':BASE,'intake_clean_detached':True,'plan_sha256':C['plan_sha256'],'campaign_sha256':sha(H/'campaign.json'),'groups':11,'commits':14,'CPP':48,'no_headers_or_inventory_changes':True,'baseline_selected48_all_fail':True,'source_modified':False,'compiler_started':False,'awaiting':['Root-approved final finite preflight path+SHA256','Explicit Root authorization record binding this plan and all14commits']})
print('R13_PREPARED',len(candidates),'commits',len(paths),'CPP','plan',C['plan_sha256'])
