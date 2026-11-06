from pathlib import Path
import hashlib,json
art=Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave9\camera_utils_luna_4277")
p=art/"preflight-input-binding.json"
m=json.loads(p.read_text(encoding="utf-8"))
sdk=Path(m["sdk"])/"include/xbox"
headers=m["headers"]
def sha(path):
 h=hashlib.sha256()
 with Path(path).open("rb") as f:
  for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
 return h.hexdigest()
for root,dirs,files in __import__('os').walk(sdk):
 for name in files:
  q=(Path(root)/name).resolve()
  headers[str(q)]=sha(q)
m["headers"]=headers;m["header_count"]=len(headers);m["xdk_include_all_files_root"]=str(sdk.resolve())
p.write_text(json.dumps(m,indent=2)+"\n",encoding="utf-8")
print(json.dumps({"header_count":m["header_count"],"xdk_files":sum(1 for root,dirs,files in __import__('os').walk(sdk) for _ in files)},indent=2))