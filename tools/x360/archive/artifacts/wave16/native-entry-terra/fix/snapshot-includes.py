import hashlib,json,pathlib,re,sys
log=pathlib.Path(sys.argv[1]).read_text(encoding='utf-8',errors='replace')
old=r'C:\Users\adri1\.codex\worktrees\bp360\wave4-intake'
new=r'D:\Reverse\BP-X360-Experimental-Worktrees\external-body-native-root'
paths={str(pathlib.Path(re.sub(re.escape(old),lambda m:new,x.strip(),flags=re.I))).lower() for x in re.findall(r'Note: including file:\s*(.+)',log)}
paths.add((new+r'\src\GameSource\Main\BrnMainX360.cpp').lower())
result={}
for name in sorted(paths):
 p=pathlib.Path(name)
 result[name]=hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None
pathlib.Path(sys.argv[2]).write_text(json.dumps(result,sort_keys=True,indent=1),encoding='utf-8')
