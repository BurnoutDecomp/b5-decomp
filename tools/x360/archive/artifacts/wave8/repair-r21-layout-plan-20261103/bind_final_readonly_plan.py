from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess
H=Path(__file__).resolve().parent;A=H.parent.parent;W=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
plan=read(H/'R21_FINITE_LAYOUT_GATE_PLAN.json');composition=read(H/'ordered-composition-and-preimages.json');projection=read(H/'projected-native-inventory-and-closure.json')
manifest=A/'wave17/R21_LAYOUT_CANDIDATES_DRAFT.json';m=read(manifest)
assert [c for g in m['groups']for c in g['commits']]==plan['commits']
assert m['baseline']==plan['base']and set(m['unique_paths'])==set(plan['unique_paths'])
records=[]
for row in m['path_records']:
 p=row['path'];image=composition['source_images'][p]
 assert row['sealed_R20_blob']==image['baseline']and row['final_git_blob']==image['final']
 raw=subprocess.check_output(['git','--no-optional-locks','cat-file','blob',image['final']],cwd=W)
 assert hashlib.sha256(raw).hexdigest()==row['final_git_content_sha256']
 records.append({'path':p,'baseline_blob':image['baseline'],'final_blob':image['final'],'final_git_content_sha256':row['final_git_content_sha256']})
replay=A/'wave16/replay-module-recovery-sol/DELIVERY.json';assert sha(replay)=='cd4ca60457eda94e5c5e7df55d1d55dd92ce85e62126d808897481efab8ba2fe';r=read(replay)
assert r['commit']==plan['commits'][-1]and r['worktree_clean']and r['combined_context']['outer_guards_unchanged']and not r['combined_context']['header_binding']['stale_original_header_bindings']
assert r['combined_context']['measured_game_size']=='0x9A1300'and r['combined_context']['measured_network_offset']=='0x8C3600'
assert {p['path']for p in r['changed_paths']}=={p for p in plan['unique_paths']if p.startswith('src/GameSource/Replays/BrnReplayModule.')}
for row in r['changed_paths']:assert row['post_git_blob']==composition['source_images'][row['path']]['final']
ice=A/'wave16/ice-layout-recovery-sol/DELIVERY.json';assert sha(ice)=='f3de76563c75e7b7efd03cb649859013c5c242c279500f4633a3aefd86848f8e'
final=dict(plan);final['state']='READ_ONLY_PLAN_COMPLETE_AWAIT_ONEBATCH_REVIEW_AND_EXPLICIT_ROOT_COLLECTION_RELEASE';final['created_utc']=datetime.now(timezone.utc).isoformat();final['initial_plan_sha256']=sha(H/'R21_FINITE_LAYOUT_GATE_PLAN.json')
final['manifest']={'path':str(manifest),'sha256':sha(manifest),'state':m['state'],'exact_order_and_all22_final_rawGit_source_images_match':True}
final['final_source_hashes']=records
final['sealed_author_receipts']=[{'path':str(replay),'sha256':sha(replay),'scope':'Replay3/3 onR19 plus separate1/1typedGame overlay onba058. Not intakeproducts.'},{'path':str(ice),'sha256':sha(ice),'scope':'Controller/lifetime/Wrapper partials;remainingGameblocker addressed by finalReplayoverlay,not a substitute for integrated179.'}]
final['author_receipt_context_limits']=r['limits'];final['execution'][0]='Wait onebatchreview and explicitRootcollection/buildrelease. FinalReplay3937receipt nowsealed and exactsources matchmanifest.'
final['limits'][0]='Source/layout compile evidence only;no runtime,wholeclassparity,fullgamelink orpublication.'
final['expected_if_no_regressions']='3616PASS0FAIL/3616active native plus2SDKreferences=3618broad dispositions. OneoldGameFAIL repair candidate,no newprovider;notcounted until fresh179 actualgate.'
save(H/'FINAL_READONLY_PLAN.json',final)
guard=read(H/'native-safeguard-impact-plan.json');rows=[]
for c in composition['commits']:rows.append(f"| `{c['commit']}` | {len(c['paths'])} |")
text=f'''# R21 layout-only finite intake plan

Read-only plan; collection and builds await Root release after the one-batch review. Baseline is clean `{plan['base']}`: 3615 PASS / 1 FAIL / 3616 active native units, plus 2 separate SDK references.

| Ordered original commit | Paths |
|---|---:|
{chr(10).join(rows)}

The batch has **22 unique paths: 10 CPP and 12 headers** (23 ordered endpoints because Wrapper is changed twice). All sequential preimages and final Git-content hashes match. No files are added or removed. Native inventory remains **3616**, source text inventory **6305**. There is no new provider or source mount.

The earlier `22795` four-CPP recovery is already represented by exact R20 postimages. Both Main/DevTools paths also changed by `b856` use those exact preimages. Collect the full 16-path `b856` change; do not collect `22795` or its author preparation ancestors again.

Compile **179 fresh native sources**, retaining **3437 verified unaffected rows** with their real R20/R19/earlier origins. The gate comprises 178 conservative changed-header consumers plus 10 changed CPPs, with 9 overlapping. All 177 current-origin actual include-hit rows are covered. The independent path list equals Luna's 179-source closure; no unresolved external wrapper edge or changed nonliteral include was found.

The selected rows are 178 old PASS and the last Game FAIL. All 3615 old passes must survive. Expected only if the actual gate passes: **3616 PASS / 0 FAIL**, with two SDK references still separate and no denominator change. Private author objects are evidence, not fresh intake products.

The seven retained named safeguards have no actual changed-header dependency, so retain them with input proofs. The affected native Game source keeps the original size `0x9A1300` and Network offset `0x8C3600` guards active. The sealed author Replay receipt records 3/3 narrow native and a separate typed Game 1/1 overlay pass; integration must still rebuild all 179.

Use jobs8 maximum centrally, with at most four private jobs1 while central compilation runs. All nine canonical vendor fallbacks, `/showIncludes`, and D-only outputs/TEMP are required. Pin source, tools, external SDK/vendor context before and after; verify unchanged products/context once. Recompute the actual postcollection inventory/closure and stop on any unexpected path, header, conflict or regression.

Opaque storage intervals retain bounded lifetime contracts; their private identities remain unknown. This plan makes no runtime, full behavior, whole-class parity or game-link claim. No wave17 link-repair/R22 source, ledger, production, canonical, branch, publication or parent-index change is included.

Final plan SHA-256: `{sha(H/'FINAL_READONLY_PLAN.json')}`

Exact source images: `ordered-composition-and-preimages.json`; exact gate and retained lists: `projected-native-inventory-and-closure.json`; current named-guard impact: `native-safeguard-impact-plan.json`.
'''
with (H/'PLAN.md').open('x',encoding='utf-8',newline='\n')as f:f.write(text)
summary={'state':final['state'],'plan':str(H/'FINAL_READONLY_PLAN.json'),'plan_sha256':sha(H/'FINAL_READONLY_PLAN.json'),'markdown_sha256':sha(H/'PLAN.md'),'manifest_sha256':sha(manifest),'projection_sha256':sha(H/'projected-native-inventory-and-closure.json'),'fresh_gate':179,'retained':3437,'source_mutated':False,'compiler_started':False};save(H/'final-plan-binding.json',summary);print(json.dumps(summary,indent=2))
