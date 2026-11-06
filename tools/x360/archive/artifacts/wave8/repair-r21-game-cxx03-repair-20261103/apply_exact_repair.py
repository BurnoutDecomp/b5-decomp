from pathlib import Path
import hashlib,json,subprocess
H=Path(__file__).resolve().parent;B=H.parent/'repair-r21-layout-intake-20261103';C=json.loads((B/'campaign.json').read_text());W=Path(C['workspace']);BASE='860f0004641400b9d446b805eaa3d5faaac52efe';SOURCE='src/GameSource/Game/BrnGameModule.cpp'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
def git(*a):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=W)
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()and not git('branch','--show-current').strip()
assert git('rev-parse','codex/experimental-x360-native-r21-checkpoint').decode().strip()==BASE
snapshot=json.loads((B/'source-metadata.json').read_text())['source_snapshot'];file=W/SOURCE;before=file.read_bytes();assert hashlib.sha256(before).hexdigest()==snapshot['hashes'][SOURCE]
changes=[('const Vector2 lPosition = {200.0f, 40.0f};','const Vector2 lPosition(200.0f, 40.0f);')]
for axis in ['xAxis','yAxis','zAxis','wAxis']:
 changes.append((f'const Vector3 lAggregateValue = {{lrView.{axis}.x, lrView.{axis}.y, lrView.{axis}.z, lrView.{axis}.w}};',f'const Vector3 lAggregateValue(lrView.{axis}.mV);'))
after=before
for a,b in changes:
 assert after.count(a.encode())==1;after=after.replace(a.encode(),b.encode(),1)
headers=['src/SDKs/EATech/include/rw/math/vpu/vector_intrinsic.h','src/SDKs/EATech/include/rw/math/vpu/vector3.h','src/SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h','vendor/renderware/include/rw/math/vpu/types.h']
assert 'typedef __vector4 VectorIntrinsic;'in(W/headers[0]).read_text()
ctor=(W/headers[2]).read_text();assert 'inline Vector3::Vector3(VectorIntrinsicInParam v)\n{\n    mV = v;\n}'in ctor
assert 'Vector3(VectorIntrinsicInParam v);'in(W/headers[1]).read_text()and 'Vector2(float lfX, float lfY)'in(W/headers[3]).read_text()
save(H/'ROOT_AUTHORIZATION_AND_REPAIR_PLAN.json',{'root_explicit_authorization':True,'base':BASE,'source':SOURCE,'approved_exact_five_changes':[{'before':a,'after':b}for a,b in changes],'prior_checkpoint_preserved':'codex/experimental-x360-native-r21-checkpoint','source_preimage_sha256':hashlib.sha256(before).hexdigest(),'planned_postimage_sha256':hashlib.sha256(after).hexdigest(),'native_raw_register_constructor':'VectorIntrinsic=__vector4;Vector3(VectorIntrinsicInParam v){mV=v;} full128-bit copy includingW;not the scalar3 constructor that zeroesW.','actual_header_sha256':{p:sha(W/p)for p in headers},'self_review':'Root-approved trivial gate-only;exact five native API construction substitutions. Blocks,assignments,deadcounter,allothercode/guards/headers unchanged.','required_gate':'ONE actual Game nativecompile all9fallbacks/showIncludes/DoutputsTEMP;reuse priorverified products and trueorigins,prove onlyGame source changed/noincomingCPPconsumers.','exclusions':'No extra source/header/layout/guard change,repeat179,owner/guardcohort,link,R22,canonical,index,main/dev/ledger/origin/publication.'})
file.write_bytes(after)
assert git('diff','--name-only').decode().splitlines()==[SOURCE]
diff=git('diff','--binary');assert len([x for x in diff.decode().splitlines()if x.startswith('-')and not x.startswith('---')])==5 and len([x for x in diff.decode().splitlines()if x.startswith('+')and not x.startswith('+++')])==5
assert b'siTakenDownLinesLeft = 30;'in after and b'NativeGameModuleSize[sizeof(BrnGameModule) == 0x9A1300 ? 1 : -1]'in after and b'NativeNetworkOffset[offsetof(BrnGameModule, mNetworkModule) == 0x8C3600 ? 1 : -1]'in after
with(H/'exact-repair.diff').open('xb')as f:f.write(diff)
message='Use native vector constructors in Game module\n\nReplace five non-aggregate initializers with the real C++03 vector constructors. Raw-register Vector3 construction preserves all four lanes; layout guards, assignments and the existing diagnostic counter remain unchanged.\n\nCo-Authored-By: Codex GPT-6 <noreply@openai.com>\n'
with(H/'COMMIT_MESSAGE.txt').open('x',encoding='utf-8',newline='\n')as f:f.write(message)
subprocess.run(['git','add','--',SOURCE],cwd=W,check=True);assert git('diff','--cached','--name-only').decode().splitlines()==[SOURCE]
subprocess.run(['git','commit','--file',str(H/'COMMIT_MESSAGE.txt')],cwd=W,check=True);head=git('rev-parse','HEAD').decode().strip();assert not git('status','--porcelain=v1').strip()and git('rev-parse','HEAD^').decode().strip()==BASE
author='ba05880df217c6c75ec63b7a7534b6c2f9c31121';residual=git('diff',author,'HEAD','--',SOURCE)
with(H/'difference-from-author-Game.diff').open('xb')as f:f.write(residual)
assert len([x for x in residual.decode().splitlines()if x.startswith('+')and not x.startswith('+++')])==3 and not any(x.startswith('-')and not x.startswith('---')for x in residual.decode().splitlines())
assert b'siTakenDownLinesLeft = 30;'in residual
save(H/'repair-commit-binding.json',{'head':head,'base':BASE,'source':SOURCE,'parent_unchanged_frozen_checkpoint':True,'five_line_substitutions_only':True,'postimage_sha256':sha(file),'postimage_git_blob':git('rev-parse','HEAD:'+SOURCE).decode().strip(),'repair_diff_sha256':sha(H/'exact-repair.diff'),'author_Game_reference':author,'author_Game_source_blob':git('rev-parse',author+':'+SOURCE).decode().strip(),'only_author_difference':'Preserved original3lines dead crashcam comment/counter;noother differences.','author_difference_sha256':sha(H/'difference-from-author-Game.diff'),'source_clean':True,'source_detached':True})
print('R21_GAME_REPAIR_COMMITTED',head,flush=True)
