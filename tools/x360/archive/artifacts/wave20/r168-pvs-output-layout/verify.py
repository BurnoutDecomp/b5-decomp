import json,re,hashlib,struct,sys,subprocess
from pathlib import Path
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re'); import x360rd
root=Path(r'C:\Users\adri1\.codex\worktrees\bp360\resource_sentinel_terra'); out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r168-pvs-output-layout')
checks=[('BrnPVSModule.asm','?Construct@OutputBuffer@PVSIO@BrnWorld@@',0x822ee658,0x44),('BrnPVSModuleIO.asm','?Prepare@OutputBuffer@PVSIO@BrnWorld@@',0x822ee6a0,0x34),('BrnPVSModuleIO.asm','?Release@OutputBuffer@PVSIO@BrnWorld@@',0x822ee6d8,0x34),('BrnPVSModuleIO.asm','?Destruct@OutputBuffer@PVSIO@BrnWorld@@',0x822ee710,0x14)]
proof=[]
for fn,key,addr,n in checks:
 s=(out/fn).read_text(errors='replace'); start=s.index(key+'QAA') if False else s.index(key+'QAAXXZ PROC NEAR') if 'Construct@' in key or 'Destruct@' in key else s.index(key+'QAA_NXZ PROC NEAR'); end=s.index(' ENDP',start); chunk=s[start:end]; pairs=re.findall(r'^\s*([0-9a-f]{5})\s+([0-9a-f]{8})\s+',chunk,re.M); words=[int(y,16) for _,y in pairs]; orig=struct.unpack('>'+str(n//4)+'I',x360rd.rd(addr,n)); diffs=[]
 for ix,(a,b) in enumerate(zip(words,orig)):
  if a != b:
   isbranch=(a & 0xfc000000)==0x48000000 and (b & 0xfc000000)==0x48000000 and (a&3)==(b&3)
   diffs.append({'offset':hex(ix*4),'coff':hex(a),'artist':hex(b),'branch_relocation':isbranch})
 proof.append({'function':key,'artist':hex(addr),'native_words':len(words),'artist_words':len(orig),'diffs':diffs,'only_branch_relocations':len(words)==len(orig) and all(x['branch_relocation'] for x in diffs)})
print(json.dumps(proof,indent=2)); (out/'opcode-comparison.json').write_text(json.dumps(proof,indent=2))
comp=json.loads((out/'compile-results.json').read_text()); hashes={}
for p in sorted(out.glob('*')):
 if p.suffix in ('.obj','.asm','.log'): hashes[p.name]=hashlib.sha256(p.read_bytes()).hexdigest()
changed=subprocess.check_output(['git','diff-tree','--no-commit-id','--name-only','-r','HEAD'],cwd=root,text=True).splitlines(); paths={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in changed}
res={'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'parent':subprocess.check_output(['git','rev-parse','HEAD^'],cwd=root,text=True).strip(),'tree':subprocess.check_output(['git','rev-parse','HEAD^{tree}'],cwd=root,text=True).strip(),'source_sha256':paths,'artifacts_sha256':hashes,'compile_success':all(x['exit_code']==0 for x in comp),'comparisons':proof}; (out/'EVIDENCE.json').write_text(json.dumps(res,indent=2))
