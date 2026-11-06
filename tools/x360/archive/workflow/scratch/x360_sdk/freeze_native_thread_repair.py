from pathlib import Path
import hashlib,json,subprocess

ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=Path(__file__).parent
meta_path=OUT/'native-thread-sync-patch.json'
old=json.loads(meta_path.read_text())
for suffix in ['patch.json','diff']:
    p=OUT/('native-thread-sync-'+suffix if suffix=='patch.json' else 'native-thread-sync.diff')
    saved=OUT/('native-thread-sync-v1-'+suffix if suffix=='patch.json' else 'native-thread-sync-v1.diff')
    assert not saved.exists(),saved
    saved.write_bytes(p.read_bytes())
base=old['base']
diff=subprocess.check_output(['git','diff','--binary',base],cwd=ROOT)
paths=subprocess.check_output(['git','diff','--name-only',base],cwd=ROOT,text=True).splitlines()
added=subprocess.check_output(['git','diff','--name-only','--diff-filter=A',base],cwd=ROOT,text=True).splitlines()
meta={'base':base,'sha256':hashlib.sha256(diff).hexdigest(),'paths':paths,'new_sources':added,
      'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths}}
meta_path.write_text(json.dumps(meta,indent=2)+'\n')
(OUT/'native-thread-sync.diff').write_bytes(diff)
(OUT/'native-thread-sync-pathspec').write_bytes(b'\0'.join(p.encode() for p in paths)+b'\0')
print(meta['sha256'],len(paths),'frozen paths')
