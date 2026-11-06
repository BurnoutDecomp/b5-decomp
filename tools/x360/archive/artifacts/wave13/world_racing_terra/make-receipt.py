import hashlib,json,subprocess,pathlib,re,datetime
root=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\ai_accessors_terra')
a=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave13\world_racing_terra')
gate=a/'native-gate-20261103-100320-8488'
base='2beeee2e9b4445c51d2e7113820c6a3b5c1b1e5b'
def git(*args): return subprocess.check_output(['git',*args],cwd=root).decode().strip()
def digest(b): return hashlib.sha256(b).hexdigest()
def filehash(p): return digest(pathlib.Path(p).read_bytes())
r=json.loads((gate/'compile-results.json').read_text())
assert r['passed']==5 and r['failed']==0 and all(x['machine']==0x1f2 for x in r['results'])
includes={}; rows=[]; missing=[]
for row in r['results']:
    src=row['source']; p=root/src
    old=subprocess.check_output(['git','cat-file','blob',f'{base}:{src}'],cwd=root)
    filtered=subprocess.check_output(['git','cat-file','--filters',f'{base}:{src}'],cwd=root)
    post=subprocess.check_output(['git','cat-file','blob',f'HEAD:{src}'],cwd=root)
    log=pathlib.Path(row['log']); bindings=[]
    for line in log.read_text(encoding='utf-8').splitlines():
        m=re.match(r'Note: including file:\s*(.+?)\s*$',line)
        if not m: continue
        q=pathlib.Path(m.group(1)); key=str(q)
        if key not in includes:
            if q.is_file(): includes[key]={'sha256':filehash(q),'size':q.stat().st_size}
            else: missing.append(key); includes[key]={'error':'missing after gate'}
        bindings.append(key)
    rows.append({'source':src,'pre_git_blob':git('rev-parse',f'{base}:{src}'),'pre_git_content_sha256':digest(old),'pre_checkout_filtered_sha256':digest(filtered),'pre_checkout_hash_note':'reconstructed by git checkout filters after the gate, not measured before edits','post_git_blob':git('rev-parse',f'HEAD:{src}'),'post_git_content_sha256':digest(post),'post_raw_worktree_sha256':filehash(p),'post_raw_lf_count':p.read_bytes().count(b'\n'),'post_raw_crlf_count':p.read_bytes().count(b'\r\n'),'command':row['command'],'returncode':row['returncode'],'machine':row['machine'],'object':{'path':row['object'],'sha256':filehash(row['object'])},'assembly':{'path':row['assembly'],'sha256':filehash(row['assembly'])},'log':{'path':row['log'],'sha256':filehash(row['log'])},'showIncludes':bindings})
assert not missing,missing
ip=a/'resolved-includes.json';ip.write_text(json.dumps(includes,indent=2)+'\n',encoding='utf-8')
compiler=pathlib.Path(r['sdk'])/'bin/win32/cl.exe'
receipt={'scope':'five native source compile gates; no link or runtime or complete parity claim','base':base,'previous_detached_head':'1c6d90e1553f53c17aa25c05da20810d07236451','preserved_ref':'codex/experimental-x360-r17-world-racing-previous','branch':git('branch','--show-current'),'final':git('rev-parse','HEAD'),'tree':git('rev-parse','HEAD^{tree}'),'clean_worktree':not bool(git('status','--porcelain=v1')),'gate':{'report':str(gate/'compile-results.json'),'report_sha256':filehash(gate/'compile-results.json'),'passed':r['passed'],'failed':r['failed'],'target':r['target'],'jobs':1,'timeout_seconds':180,'showIncludes':True,'runner':str(a/'run-gate.ps1'),'runner_sha256':filehash(a/'run-gate.ps1'),'compiler':str(compiler),'compiler_sha256':filehash(compiler),'sdk':r['sdk'],'include_manifest':str(ip),'include_manifest_sha256':filehash(ip),'unique_resolved_include_count':len(includes)},'sources':rows,'evidence':['ARTIST 0x82779328–B0, 0x82786CF4–D58, 0x82786EF8–F58: packed fan normalization and origin updates','ARTIST 0x827873A0, 0x82787704–90, 0x827878B0–82787940: traffic packed operations and sqrt refinements','ARTIST 0x82787968, 0x82787E58, 0x827881B0, 0x82788738; raw 0x82787BE8 with sentinels 0x82815570=7C8A2378, 0x82F264A8=00000004','ARTIST 0x82777840, 0x82777BA0–C20, 0x82783AA8: corner packing, interpolation search and edge writes','ARTIST 0x827767F0, 0x827815F8, 0x8278F188, 0x8278F408, 0x8278E930: Hermite, projection and packed interpolation'], 'limits':['Compile-only milestone: link, runtime, and complete parity remain unverified.','Pre-existing scalarized math, floating-point fusion/rounding and upper-lane behavior remain unverified where not explicitly recovered.','HardNoGoMap square-to-world and rendering retain baseline zero-W/float-lerp behavior; ARTIST uses packed operations and stack-derived W, not certified equivalent.','Generator retained scalarized normalizations and other pre-existing arithmetic and zero-upper-lane behavior; no full parity claim.','Source pre-checkout raw hashes were not captured before editing; checkout-filtered baseline hashes are reconstructed, explicitly distinguished from measured post-gate raw hashes.','Include hashes and source raw hashes were measured after the gate; source edits were frozen during the gate.'], 'prior_baseline_logs':'D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r15-full-20261103/full-native/','notes':'The five-source native gate passed on first attempt; original baseline failed logs remain intact.'}
out=a/'DELIVERY.json';out.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8');print('receipt',out,'sha256',filehash(out),'include_count',len(includes),'clean',receipt['clean_worktree'])
