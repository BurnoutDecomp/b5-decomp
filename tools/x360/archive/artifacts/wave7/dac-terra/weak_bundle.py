import sys,struct
from pathlib import Path
root=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(root/'scratch/x360_sdk'));import native_atomic_fixture as F
class WeakBundle(F.Bundle):
 def __init__(self,objects):
  super().__init__(objects);self.weak_resolved=[]
  for oi,obj in enumerate(objects):
   _,_,_,symoff,nsyms,_,_=struct.unpack_from('<HHIIIHH',obj.data);i=0
   while i<nsyms:
    raw,value,sec,kind,storage,aux=struct.unpack_from('<8sIhHBB',obj.data,symoff+i*18)
    if storage==105 and aux:
     tag,characteristics=struct.unpack_from('<II',obj.data,symoff+(i+1)*18);name=obj.symbols[i][0];fallback=obj.symbols[tag][0]
     if name not in self.definitions and characteristics==2 and fallback in self.definitions:
      self.definitions[name]=self.definitions[fallback];self.weak_resolved.append((name,fallback,characteristics))
    i+=1+aux
