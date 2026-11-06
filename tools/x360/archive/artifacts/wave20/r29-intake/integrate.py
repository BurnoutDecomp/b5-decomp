import hashlib,json,pathlib,subprocess
H=pathlib.Path(__file__).resolve().parent; B=H.parent/"r29-batch"; W=pathlib.Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake"); CAN=pathlib.Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp"); P=CAN.parent; L=H.parent.parent/"wave8/repair-r21-layout-intake-20261103"
read=lambda p:json.loads(pathlib.Path(p).read_text()); sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def git(*args,cwd=W):return subprocess.check_output(["git","--no-optional-locks",*args],cwd=cwd)
a=read(H/"ROOT_RELEASE.json"); m=read(B/"MANIFEST.json"); proj=read(H/"projection.json"); entries={x["path"]:x for x in m["final_paths"]}; base=m["base"]
assert a["approved"] is True and a["manifest_sha256"]==sha(B/"MANIFEST.json")=="365267ec4acaeb8b8441e10fc32f1565363c09faf7bf509cdb552014880f7992" and a["patch_sha256"]==sha(B/"r29.patch")=="3085a5875905f2b3bdc6145a7153155cc0e826afb0cf8c838a180414cc9bf14d" and a["projection_sha256"]==sha(H/"projection.json") and a["review_sha256"]==sha(a["review_path"]) and a["guard_baseline_sha256"]==sha(H/"guard-baseline.json") and a["netconn_binding_sha256"]==sha(H/"netconn-check-binding.json") and a["entropy_binding_sha256"]==sha(H/"entropy-check-binding.json") and a["loading_binding_sha256"]==sha(H/"loading-check-binding.json")
assert base=="fa84cd2a4423d5258a88eff385c5ad61e1707a88" and len(entries)==22 and git("rev-parse","HEAD").decode().strip()==base and not git("status","--porcelain=v1").strip() and not git("branch","--show-current").strip()
assert git("rev-parse","HEAD",cwd=CAN).decode().strip()=="3534f7f16103a58ff904bbd1604e18e9934868f4" and not git("status","--porcelain=v1",cwd=CAN).strip()
assert git("ls-files","--stage","b5-decomp",cwd=P).decode().strip()==read(read(L/"campaign.json")["plan"])["parent_gitlink_index"]
for path,x in entries.items():
 line=git("ls-files","--stage","--",path).decode().strip(); expected=x["old"]
 if expected is None: assert not line and not (W/path).exists(),path
 else: assert line.split()[:2]==[expected.split()[0],expected.split()[-1]],path
git("apply","--check","--index",str(B/"r29.patch"));git("apply","--index",str(B/"r29.patch"))
assert set(git("diff","--cached","--name-only").decode().splitlines())==set(entries)
postimage_proof={}
for path,x in entries.items():
 line=git("ls-files","--stage","--",path).decode().strip(); assert line.split()[:2]==[x["new"].split()[0],x["new"].split()[-1]],path
 raw=git("hash-object","--no-filters","--",path).decode().strip(); clean=git("hash-object","--path="+path,"--",path).decode().strip(); assert x["new"].split()[-1] in (raw,clean),path; postimage_proof[path]={"index_blob":x["new"].split()[-1],"raw_blob":raw,"clean_filtered_blob":clean}
(H/"postimage-blob-proof.json").write_text(json.dumps(postimage_proof,indent=2)+"\n")
msg=H/"COMMIT_MESSAGE.txt"; msg.write_text("Integrate experimental X360 R29 batch\n\nCo-Authored-By: GPT-6.1 Terra <noreply@openai.com>\n")
git("commit","--file",str(msg)); head=git("rev-parse","HEAD").decode().strip(); assert not git("status","--porcelain=v1").strip()
binding={"base":base,"head":head,"paths":sorted(entries),"manifest_sha256":sha(B/"MANIFEST.json"),"patch_sha256":sha(B/"r29.patch"),"projection_sha256":sha(H/"projection.json"),"review_sha256":a["review_sha256"],"guard_baseline_sha256":a["guard_baseline_sha256"]}; (H/"commit-binding.json").write_text(json.dumps(binding,indent=2)+"\n");print("INTEGRATED",head,flush=True)
