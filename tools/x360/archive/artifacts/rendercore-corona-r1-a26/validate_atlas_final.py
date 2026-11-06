from corona_fixture_final import *
BASE=0x82fafc10;SIZE=768
b=bundle(['BrnCoronaManager.cpp']);select(b,'??__E?s_atlasUVs@BrnCoronaManager@')
for (oi,sec),base in b.bases.items():
 o=b.objects[oi]
 for at,ix,kind in o.relocations(sec):
  if o.symbols[ix][0].startswith('?s_atlasUVs@BrnCoronaManager@'):
   raw=struct.unpack_from('>I',o.data,o.sections[sec][4]+at)[0]
   assert kind in [0x10,0x11]
   value=(raw&0xffff0000)|(((BASE+0x8000)>>16 if kind==0x10 else BASE)&0xffff)
   struct.pack_into('>I',b.images[base],at,value)
original_image=F.M.V.x360rd.rd(BASE,SIZE);assert original_image==bytes(SIZE)
o=b.objects[0];symbol=next((n,v,s) for n,v,s in o.symbols.values() if n.startswith('?s_atlasUVs@') and s>0);section=o.sections[symbol[2]];native_image=o.data[section[4]+symbol[1]:section[4]+symbol[1]+SIZE] if section[4] else bytes(SIZE);assert native_image==original_image
errors=[];traces=None;expected=None
for i in range(32):
 both=[]
 for native in [False,True]:
  c=cpu(b);c.mem.add(0x82faf000,0x1000,F.M.V.x360rd.rd(0x82faf000,0x1000));c.mem.wr(BASE,native_image if native else original_image);guard=bytes([i])*16;c.mem.wr(BASE-16,guard);c.mem.wr(BASE+SIZE,guard);sp=ptr(c,1);c.mem.wr(sp-0x4000,bytes([i])*0x4000);writes=[];wr=c.mem.wr
  def track(a,data):
   if BASE<=a<BASE+SIZE:writes.append((a-BASE,len(data),bytes(data).hex()))
   return wr(a,data)
  c.mem.wr=track;c.run(b.entry if native else 0x82c4edd8);both.append((c.mem.rd(BASE,SIZE),c.mem.rd(BASE-16,16),c.mem.rd(BASE+SIZE,16),writes))
 if both[0][:3]!=both[1][:3] or both[0][1]!=guard or both[0][2]!=guard:errors.append(i)
 if i==0:traces=[both[0][-1],both[1][-1]];expected=both[0][0]
report={'cases':32,'errors':errors,'image_sha256':hashlib.sha256(original_image).hexdigest(),'initialized_sha256':hashlib.sha256(expected).hexdigest(),'original_writes':len(traces[0]),'native_writes':len(traces[1]),'write_footprint_matches':sorted(traces[0])==sorted(traces[1]),'trace_order_matches':traces[0]==traces[1],'object':pin(PATHS['BrnCoronaManager.cpp']),'scope':'Whole native atlas CRT against original82C4EDD8 from exact zero image, all48 native vector rows, guards and original/native store footprints. Source constructors remain genuine inline SDK bodies.','limits':'Pure local initializer semantics only; global CRT ordering/GPU/game launch not claimed.'}
assert not errors and report['write_footprint_matches'];emit('final-atlas-crt-comparison.json',report);print(json.dumps(report,indent=2))
