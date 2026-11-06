import hashlib,json,pathlib,subprocess
H=pathlib.Path(__file__).resolve().parent; B=H.parent/"r164-batch"; W=pathlib.Path("C:/Users/adri1/.codex/worktrees/bp360/wave4-intake"); CAN=pathlib.Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp"); P=CAN.parent; L=H.parent.parent/"wave8/repair-r21-layout-intake-20261103"
read=lambda p:json.loads(pathlib.Path(p).read_text()); sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def git(*args,cwd=W):return subprocess.check_output(["git","--no-optional-locks",*args],cwd=cwd)
a=read(H/"ROOT_RELEASE.json"); m=read(B/"MANIFEST.json"); proj=read(H/"projection.json"); entries={x["path"]:x for x in m["final_paths"]}; base=m["base"]
assert a["approved"] is True and a["manifest_sha256"]==sha(B/"MANIFEST.json")=="547f1ecd4e75a05d76bbbabfbf56e87dfd89c147ec01420d9a3ddac1434c2a0b" and a["patch_sha256"]==sha(B/"r164.patch")=="a9a459312365a8232f02bc0728f434ab9e79a47c4d407b5771478fb871535552" and a["projection_sha256"]==sha(H/"projection.json") and a["review_sha256"]==sha(a["review_path"]) and a["guard_baseline_sha256"]==sha(pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r164-guard-baseline/guard-baseline.json')) and a["netconn_binding_sha256"]==sha(H/"netconn-check-binding.json") and a["entropy_binding_sha256"]==sha(H/"entropy-check-binding.json") and a["loading_binding_sha256"]==sha(H/"loading-check-binding.json")
review=read(a["review_path"]); assert str(review["verdict"]).lower()=="pass" and review["manifest_sha256"]==a["manifest_sha256"] and review["patch_sha256"]==a["patch_sha256"] and review["base"]==base
assert base=="b21b1aaaaf795d81aaf304b808ac47d7b98fe59c" and len(entries)>0 and len(entries)==len(m["final_paths"]) and all(x["new"] for x in entries.values()) and {p for p,x in entries.items() if x["old"] is None}==set() and git("rev-parse","HEAD").decode().strip()==base and not git("status","--porcelain=v1").strip() and not git("branch","--show-current").strip()
assert git("rev-parse","HEAD",cwd=CAN).decode().strip()=="3534f7f16103a58ff904bbd1604e18e9934868f4" and not git("status","--porcelain=v1",cwd=CAN).strip()
assert git("ls-files","--stage","b5-decomp",cwd=P).decode().strip()==read(read(L/"campaign.json")["plan"])["parent_gitlink_index"]
for path,x in entries.items():
 line=git("ls-files","--stage","--",path).decode().strip(); expected=x["old"]
 if expected is None: assert not line and not (W/path).exists(),path
 else: assert line.split()[:2]==[expected.split()[0],expected.split()[-1]],path
git("-c","core.whitespace=cr-at-eol","apply","--check","--index",str(B/"r164.patch"));git("-c","core.whitespace=cr-at-eol","apply","--index",str(B/"r164.patch"))
assert set(git("diff","--cached","--name-only").decode().splitlines())==set(entries)
assert git("write-tree").decode().strip()==m["projected_tree"]
postimage_proof={}
for path,x in entries.items():
 line=git("ls-files","--stage","--",path).decode().strip(); assert line.split()[:2]==[x["new"].split()[0],x["new"].split()[-1]],path
 raw=git("hash-object","--no-filters","--",path).decode().strip(); clean=git("hash-object","--path="+path,"--",path).decode().strip(); assert x["new"].split()[-1] in (raw,clean),path; postimage_proof[path]={"index_blob":x["new"].split()[-1],"raw_blob":raw,"clean_filtered_blob":clean}
(H/"postimage-blob-proof.json").write_text(json.dumps(postimage_proof,indent=2)+"\n")
msg=H/"COMMIT_MESSAGE.txt"; msg.write_text("Integrate experimental X360 R164 batch\n\nCo-Authored-By: Codex GPT-6 <noreply@openai.com>\n")
git("commit","--file",str(msg)); head=git("rev-parse","HEAD").decode().strip(); assert git("rev-list","--parents","-n","1",head).decode().split()==[head,base] and git("rev-parse",head+"^{tree}").decode().strip()==m["projected_tree"] and not git("status","--porcelain=v1").strip()
binding={"base":base,"head":head,"paths":sorted(entries),"manifest_sha256":sha(B/"MANIFEST.json"),"patch_sha256":sha(B/"r164.patch"),"projection_sha256":sha(H/"projection.json"),"review_sha256":a["review_sha256"],"guard_baseline_sha256":a["guard_baseline_sha256"]}; (H/"commit-binding.json").write_text(json.dumps(binding,indent=2)+"\n");print("INTEGRATED",head,flush=True)
