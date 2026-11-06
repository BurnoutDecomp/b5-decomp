from pathlib import Path
import difflib, hashlib, json, subprocess
ROOT=Path('D:/codex-native/replay-base-sol-r3');OUT=Path(__file__).resolve().parent
name='src/GameSource/Replays/Serialisers/BrnReplaySoundSerialiser.cpp'
old=subprocess.check_output(['git','show','HEAD:'+name],cwd=ROOT).decode().replace('\r\n','\n')
new=old
for axis in ['xAxis','yAxis','zAxis']:
 for lane in 'xyz':
  token='lBasis.'+axis+'.'+lane+' ='
  assert new.count(token)==1
  new=new.replace(token,'lBasis.'+axis+'.mV.'+lane+' =')
patch=''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile='a/'+name,tofile='b/'+name))
p=OUT/'SOUND_NINE_LANES-v2.patch';assert not p.exists();p.write_bytes(patch.encode())
subprocess.run(['git','apply','--cached','--whitespace=nowarn',str(p)],cwd=ROOT,check=True)
row=next(r for r in json.loads((OUT/'name-void-v2/compile-results.json').read_text())['results']if r['source']==name)
evidence=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82658EC8.json')
report={'path':name,'member_tokens':9,'receiver':'lBasis.xAxis/yAxis/zAxis are rw::math::vpu::Vector3 rows in the actual Matrix33 definition; real Vector3 storage is VectorIntrinsic mV. Quaternion x/y/z/w and all unrelated POD/scalar data remain untouched.', 'native_input_lanes':'ARTIST 82658EF4/82658F00/82658F0C load source rows at +0/+10/+20. Existing nine assignments retain exact source lane indices0,1,2;4,5,6;8,9,10. No quaternion/math recovery or algorithm parity claimed.', 'primary_export':str(evidence),'primary_sha256':hashlib.sha256(evidence.read_bytes()).hexdigest(),'patch_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'baseline':'name-void-v1: sole nine compiler Vector3 member diagnostics at208..210.','combined_gate':row,'combined_binding':'name-void-v2/source-object-binding.json','limits':'Minimal consumer syntax prerequisite only. The passing whole Sound TU is built in the in-flight typed void/name closure, not a claim that all existing replay sound algorithms match ARTIST. No type/storage/constructor/lane expressions are changed.'}
p=OUT/'SOUND_NINE_LANES_PREREQUISITE.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n')
print(p,hashlib.sha256(p.read_bytes()).hexdigest())
