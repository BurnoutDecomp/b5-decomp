from pathlib import Path
import hashlib,json,subprocess
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root')
HERE=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git',*a],cwd=ROOT,text=True).strip()
head=git('rev-parse','HEAD');assert head=='7a3b9fe6178b95534973434dd034b7496f05b14c'
assert not git('status','--porcelain')
paths=git('diff-tree','--no-commit-id','--name-only','-r',head).splitlines();assert len(paths)==10
reports={};rows={};headers={}
for name in ['native-stage2','eventinfo-constants-final','roadproviders-final','hud-final']:
 p=HERE/name/'compile-results.json';d=json.loads(p.read_text());b=HERE/name/'source-object-binding.json';bind=json.loads(b.read_text())
 reports[name]={'report':str(p),'report_sha256':sha(p),'binding':str(b),'binding_sha256':sha(b)}
 for r in d['results']:rows[r['source']]={'result':r,'source_sha256':bind['sources'][r['source']]}
 for p,h in bind['actual_headers'].items():assert sha(p)==h,p;headers[p]=h
for p,r in rows.items():
 assert sha(ROOT/p)==r['source_sha256'],p
 if r['result']['passed']:
  obj=Path(r['result']['object']);assert obj.read_bytes()[:2]==bytes.fromhex('f201')
  r['object_sha256']=sha(obj)
assert len(rows)==12 and sum(r['result']['passed']for r in rows.values())==11
assert [p for p,r in rows.items()if not r['result']['passed']]==['src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp']
for name,n in [('mugshot-native-array-final.json',324)]:
 d=json.loads((HERE/name).read_text());assert d['cases']==n and not d['mismatches']
d=json.loads((HERE/'roadproviders-runtime-binding.json').read_text());assert all(not x['different_runtime_sections']for x in d['products'])
assert json.loads((HERE/'layout_probe-final.json').read_text())['expected_negative_retained']
cpp='src/GameSource/Gui/Flow/HUD/Components/BrnEventInfo.cpp'
aliases=[]
for original,alias in [('a3633b1049ee5a5e66376dcd7a503025886a2fa9','918541007db15a1f5570dcd63d8369cf57fc01f5'),('75bbef15366ccfcffbcb0f06f7a1fc7d58a2bb7b',git('rev-parse','HEAD^'))]:
 assert git('rev-parse',original+':'+cpp)==git('rev-parse',alias+':'+cpp)
 aliases.append({'original':original,'private_alias':alias,'same_CPP_postimage':True})
delivery={'commit':head,'parent':git('rev-parse','HEAD^'),'source_paths':{},'prerequisite_aliases':aliases,
 'scope':'Recover native raw325 mugshot payload, three real HUD methods, original string tables, road-shot cache fields/setup and actual sat-nav zoom write. Remove PC deferral helpers. Correct native RoadSign inheritance/VMX distance and preserve full camera vector lane input. EventInfo empty HandleTrigger is original ICF single-blr, not an invented engine stub.',
 'compile':{'unique_selected_sources':12,'passed':11,'failed':1,'reports':reports,'final_rows':rows,'unique_actual_headers':len(headers)},
 'proofs':{str(p):sha(p)for p in [HERE/'mugshot-native-array-final.json',HERE/'layout_probe-final.json',HERE/'mugshot_tables.json',HERE/'roadproviders-runtime-binding.json',HERE/'roadproviders-current-binding.json',Path('D:/Reverse/BP-X360-Experimental-Artifacts/roadrule-native-terra/FILE_HANDOFF_FINAL.json'),Path('D:/Reverse/BP-X360-Experimental-Artifacts/roadrule-native-terra/distance-comparison-final.json')]},
 'evidence':'324 actual three-method ARTIST/current-object comparisons; 264 RoadRule bounded distance cases rebound by complete non-debug section/data/named-relocation equality, 757+117 sections. Typed payload/cache/member guards pass.',
 'limits':['All three mugshot methods compare real bodies; rendering/text/animation/RoadRuleShot services are controlled, logging filter0/cache nonnull, software CPU. No actual UI/network/render/boot guarantee.',
           'Road distance uses controlled direct services and software reciprocal-square-root estimate; no physical GPU/hardware proof.',
           'Full natural HUD extent0x7920 remains different from original allocation request0x7940; only the final sizeof guard is negative. All other probe guards pass. No padding/alignment guessed and no negative removed.',
           'GameBridgeGameStateToX_StuntGuiEvents.cpp retains its prior C++03 auto/type failures; no new header error claimed. No full SetupState/startup or complete HUD parity claim.',
           'Raw whole COFF equality ignoring timestamp failed for metadata; separate every-runtime-section/data/relocation equality passes. Both records retained.',
           'No strict image link, launch, complete gameplay, or overall byte-match score. Canonical/main/dev/intake/production ledger/parent index untouched. This is later than R6.']}
for p in paths:
 before=git('rev-parse',head+'^:'+p)if p!='src/GameSource/Gui/Events/BrnGuiMugshotControlEvent.h'else None
 delivery['source_paths'][p]={'preimage':before,'postimage':git('rev-parse',head+':'+p),'working_sha256':sha(ROOT/p)}
out=HERE/'DELIVERY.json';assert not out.exists();out.write_text(json.dumps(delivery,indent=2)+'\n',encoding='utf-8')
print('Root GUI delivery',sha(out));print(head,'10paths,11/12 selected native CPP pass; one historical failure, allocation-size negative explicit')
