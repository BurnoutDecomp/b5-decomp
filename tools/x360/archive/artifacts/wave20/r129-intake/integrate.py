import hashlib,json,pathlib,subprocess
H=pathlib.Path(__file__).resolve().parent; B=H.parent/"r129-batch"; W=pathlib.Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake"); CAN=pathlib.Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp"); P=CAN.parent; L=H.parent.parent/"wave8/repair-r21-layout-intake-20261103"
read=lambda p:json.loads(pathlib.Path(p).read_text()); sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def git(*args,cwd=W):return subprocess.check_output(["git","--no-optional-locks",*args],cwd=cwd)
a=read(H/"ROOT_RELEASE.json"); m=read(B/"MANIFEST.json"); proj=read(H/"projection.json"); entries={x["path"]:x for x in m["final_paths"]}; base=m["base"]
assert a["approved"] is True and a["manifest_sha256"]==sha(B/"MANIFEST.json")=="f3c75d774959efeb3efcf5fae3c2dab58741b09f85371433d18eb25047d15df9" and a["patch_sha256"]==sha(B/"r129.patch")=="0b2894926484b3e9b3e3e38eae43a907af9d3b5e88cd59f29e8b671e0928d03d" and a["projection_sha256"]==sha(H/"projection.json") and a["review_sha256"]==sha(a["review_path"]) and a["guard_baseline_sha256"]==sha(H/"guard-baseline.json") and a["netconn_binding_sha256"]==sha(H/"netconn-check-binding.json") and a["entropy_binding_sha256"]==sha(H/"entropy-check-binding.json") and a["loading_binding_sha256"]==sha(H/"loading-check-binding.json")
review=read(a["review_path"]); assert str(review["verdict"]).lower()=="pass" and review["manifest_sha256"]==a["manifest_sha256"] and review["patch_sha256"]==a["patch_sha256"] and review["base"]==base
assert base=="e837d96358345d64722b81ccd5631bdd977e03db" and len(entries)==2 and len(entries)==len(m["final_paths"]) and all(x["new"] for x in entries.values()) and {p for p,x in entries.items() if x["old"] is None}==set() and git("rev-parse","HEAD").decode().strip()==base and not git("status","--porcelain=v1").strip() and not git("branch","--show-current").strip()
assert git("rev-parse","HEAD",cwd=CAN).decode().strip()=="3534f7f16103a58ff904bbd1604e18e9934868f4" and not git("status","--porcelain=v1",cwd=CAN).strip()
assert git("ls-files","--stage","b5-decomp",cwd=P).decode().strip()==read(read(L/"campaign.json")["plan"])["parent_gitlink_index"]
for path,x in entries.items():
 line=git("ls-files","--stage","--",path).decode().strip(); expected=x["old"]
 if expected is None: assert not line and not (W/path).exists(),path
 else: assert line.split()[:2]==[expected.split()[0],expected.split()[-1]],path
git("apply","--check","--index",str(B/"r129.patch"));git("apply","--index",str(B/"r129.patch"))
assert set(git("diff","--cached","--name-only").decode().splitlines())==set(entries)
assert git("write-tree").decode().strip()==m["projected_tree"]
postimage_proof={}
for path,x in entries.items():
 line=git("ls-files","--stage","--",path).decode().strip(); assert line.split()[:2]==[x["new"].split()[0],x["new"].split()[-1]],path
 raw=git("hash-object","--no-filters","--",path).decode().strip(); clean=git("hash-object","--path="+path,"--",path).decode().strip(); assert x["new"].split()[-1] in (raw,clean),path; postimage_proof[path]={"index_blob":x["new"].split()[-1],"raw_blob":raw,"clean_filtered_blob":clean}
(H/"postimage-blob-proof.json").write_text(json.dumps(postimage_proof,indent=2)+"\n")
msg=H/"COMMIT_MESSAGE.txt"; msg.write_text("Integrate experimental X360 R129 batch\n\nCo-Authored-By: GPT-6.1 Terra <noreply@openai.com>\n")
git("commit","--file",str(msg)); head=git("rev-parse","HEAD").decode().strip(); assert not git("status","--porcelain=v1").strip()
binding={"base":base,"head":head,"paths":sorted(entries),"manifest_sha256":sha(B/"MANIFEST.json"),"patch_sha256":sha(B/"r129.patch"),"projection_sha256":sha(H/"projection.json"),"review_sha256":a["review_sha256"],"guard_baseline_sha256":a["guard_baseline_sha256"]}; (H/"commit-binding.json").write_text(json.dumps(binding,indent=2)+"\n");print("INTEGRATED",head,flush=True)
