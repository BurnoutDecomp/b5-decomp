from pathlib import Path
import hashlib,json,re,subprocess
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root')
HERE=Path(__file__).resolve().parent
def git(*a):return subprocess.check_output(['git',*a],cwd=ROOT,text=True).strip()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
head=git('rev-parse','HEAD');base=git('rev-parse','HEAD^')
assert head.startswith('3227eb950') and base=='fdf52c73c6bbebd03d89003dab4686ee75701a50'
assert not git('status','--porcelain')
rp=HERE/'native-stage1/compile-results.json';report=json.loads(rp.read_text())
bp=HERE/'native-stage1/source-object-binding.json';binding=json.loads(bp.read_text())
assert (report['requested'],report['passed'],report['failed'])==(3,3,0)
assert binding['head']==base
assert all(sha(ROOT/p)==h for p,h in binding['sources'].items())
assert all(sha(p)==h for p,h in binding['actual_headers'].items())
paths={};checks={}
for row in report['results']:
 p=row['source'];old=git('rev-parse',base+':'+p);r5=git('rev-parse','04ed276fcd79c0d91e5e91db9cd08438c4e0689d:'+p)
 assert old==r5
 assert Path(row['object']).read_bytes()[:2]==bytes.fromhex('f201')
 paths[p]={'preimage':old,'R5_preimage':r5,'postimage':git('rev-parse',head+':'+p),
           'source_sha256':sha(ROOT/p),'object':row['object'],'object_sha256':sha(row['object'])}
 before=subprocess.check_output(['git','show',base+':'+p],cwd=ROOT,text=True)
 after=(ROOT/p).read_text()
 if 'StuntManager' in p:
  names=['DealWithStunt','RequestTip','CheckForSpecialCarUnlocks','SendGameCompletionResults',
         'AddEvent','AddStuntElement','OnCollectAllStunts']
  calls={n:{'before':len(re.findall(r'\b'+n+r'\s*\(',before)),
            'after':len(re.findall(r'\b'+n+r'\s*\(',after))}for n in names}
  assert all(v['before']==v['after']for v in calls.values())
  assert re.findall(r'GameStateModuleIO::E_ACTION_[A-Z_]+',before)==re.findall(r'GameStateModuleIO::E_ACTION_[A-Z_]+',after)
  checks[p]={'native_call_occurrences_retained':calls,'native_event_constants_in_same_order':True,
            'native_boost_guard_and_repeat_early_return':('gUI_00' not in p or ('if (lbPostedBoostAction)'in after and 'if (lbAlreadyDone)\n    {\n        return;\n    }'in after))}
  assert checks[p]['native_boost_guard_and_repeat_early_return']
 else:
  assert after==before.replace('const Vector3 lResult = {','const rw::math::vpu::VectorIntrinsic lResult = {').replace('return lResult;','return Vector3(lResult);')
  checks[p]={'only_delta':'Same four scalar lane expressions initialize genuine VectorIntrinsic, then explicit genuine Vector3 intrinsic constructor.'}
delivery={'commit':head,'parent':base,'R5_preimages_equal':True,'paths':paths,
 'compile':{'requested':3,'passed':3,'failed':0,'target':'PPC32 big-endian Xbox 360',
            'report':str(rp),'report_sha256':sha(rp),'binding':str(bp),'binding_sha256':sha(bp),
            'actual_header_count':len(binding['actual_headers'])},
 'source_checks':checks,
 'scope':'Remove only added PC stunt diagnostic helpers/branches/counters and unused host headers; correct C++03 vector construction in PolygonSoup. No native game feature was removed.',
 'limits':['Whole three translation units compile with the actual XDK. Source call/guard retention is an inspection check, not a runtime proof.',
           'PolygonSoup existing arithmetic and all four lane expressions retained; cited original828AF0D0 export was unavailable, so no new whole-function byte or behavioral parity claim.',
           'No header changes, new engine stubs, fake scalar members, or layout guard bypasses.',
           'No link, image, launch, complete gameplay or overall byte-match proof. Latest full R5 counts unchanged.',
           'This commit is later than and excluded from exact R6 fourteen-original cutoff; collect separately if authorized.'],
 'canonical_intake_main_dev_ledger_untouched':True,
 'artifacts':{p.name:sha(p)for p in [HERE/'cleanup_stunt.py',HERE/'run_native_gate.py',HERE/'stunt-pc-removal.json']}}
out=HERE/'DELIVERY.json';assert not out.exists();out.write_text(json.dumps(delivery,indent=2)+'\n',encoding='utf-8')
print('DELIVERY',sha(out));print(head,'3/3 native CPP pass')
