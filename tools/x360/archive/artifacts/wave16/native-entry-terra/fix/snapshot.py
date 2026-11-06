import hashlib,json,pathlib,sys
roots=[pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\external-body-native-root\src'),pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\external-body-native-root\vendor'),pathlib.Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor'),pathlib.Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK\include\xbox')]
d={}
for root in roots:
 for p in root.rglob('*'):
  if p.is_file() and p.suffix.lower() in ('.h','.hpp','.inl','.inc','.c','.cpp'):
   try:d[str(p).lower()]=hashlib.sha256(p.read_bytes()).hexdigest()
   except OSError:pass
pathlib.Path(sys.argv[1]).write_text(json.dumps(d,sort_keys=True,indent=1),encoding='utf-8')
