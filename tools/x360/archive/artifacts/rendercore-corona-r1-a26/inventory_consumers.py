from pathlib import Path
import collections,hashlib,json,os,re,subprocess
R=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/rendercore-corona-r1-a26');O=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-corona-r1-a26')
files=subprocess.check_output(['git','ls-files'],cwd=R,env=dict(os.environ,GIT_NO_LAZY_FETCH='1')).decode().splitlines();known={os.path.normpath(str(R/p)).lower():p for p in files}
include_dirs=[R/x for x in ['src','src/SDKs/EATech','vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include','vendor/renderware/include','vendor/PPMalloc/include','vendor/coreallocator/include','vendor/dirtysdk/include','vendor/zlib/include','vendor/lua/src','src/GameShared/GameClasses/Development']]
include_dirs+=[Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK/include/xbox')]
# This conservative graph follows repository literal includes only. External roots
# are hashed independently; unresolved/macro includes do not imply absence.
reverse=collections.defaultdict(set);unresolved=[];macro=[];input_hashes={};pattern=re.compile(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',re.M)
for rel in files:
 p=R/rel
 if p.suffix.lower() not in {'.h','.hpp','.inl','.inc','.cpp','.c','.cc','.cxx'} or not p.is_file():continue
 data=p.read_bytes();input_hashes[rel]=hashlib.sha256(data).hexdigest();s=data.decode('utf-8',errors='replace')
 for name in pattern.findall(s):
  found=None
  for parent in [p.parent,*include_dirs]:
   candidate=known.get(os.path.normpath(str(parent/name)).lower())
   if candidate is not None:found=candidate;break
  if found is not None:reverse[found].add(rel)
  else:unresolved.append({'source':rel,'include':name})
 for line in s.splitlines():
  if re.match(r'^\s*#\s*include\s+[^<"\s]',line):macro.append({'source':rel,'directive':line.strip()})
sources=[p for p in files if p.startswith(('src/','vendor/')) and Path(p).suffix.lower() in {'.cpp','.c'} and not any(x.lower()=='pc' for x in Path(p).parts)]
for p in sources:reverse['src/types.hpp'].add(p)
targets=['vendor/renderware/include/rw/math/vpu/types.h','vendor/renderware/include/rw/rwcore_structs.h','src/GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h','src/GameSource/Effects/Curves.h','src/GameSource/Graphics/BrnCoronaManager.h','src/SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h']
sets={}
for target in targets:
 seen={target};q=collections.deque([target])
 while q:
  for p in reverse[q.popleft()]:
   if p not in seen:seen.add(p);q.append(p)
 sets[target]=sorted(set(sources)&seen)
report={'commit':'0b82748bcfcfd348bab01a5c0326eefaee64aa80','native_inventory_count':len(sources),'consumer_sets':sets,'conservative_union':sorted(set().union(*map(set,sets.values()))),'unresolved_literal_includes':unresolved,'macro_includes':macro,'source_header_hashes':input_hashes,'scope':'Conservative all-branch repository literal-include graph, plus actual /FItypes.hpp. CPP inventory follows build_x360.py --all filter. Source/header identities are pinned; this graph is not an assertion all consumers compile or a preprocessor-complete graph. Full combined-source inventory gate remains integration work.'}
p=O/'consumer-inventory.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'file':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'native_inputs':len(sources),'source_headers':len(input_hashes),'consumer_counts':{p:len(v) for p,v in sets.items()},'union':len(report['conservative_union']),'macro_include_count':len(macro)},indent=2))
