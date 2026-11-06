from corona_fixture_final import *
import random
rng=random.Random(0x82c4f418)
old=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/corona-original-init.json').read_text())
image=bytes.fromhex(''.join(x['image'] for x in old['records']));expected=bytes.fromhex(''.join(x['after'] for x in old['records']));BASE=0x82f24310
b=bundle(['BrnCoronaManager.cpp']);select(b,'??__E?mParams@BrnCoronaTypeParams@',True)
o=b.objects[0];symbol=[(n,v,s) for n,v,s in o.symbols.values() if n.startswith('?mParams@BrnCoronaTypeParams@') and s>0][0];section=o.sections[symbol[2]];compiled_image=bytes(o.data[section[4]+symbol[1]:section[4]+symbol[1]+1200]) if section[4] else bytes(1200)
errors=[];trace_example=None
for i in range(64):
 both=[]
 for native in [False,True]:
  c=cpu(b);prefix=rng.randbytes(16) if False else bytes([i])*16;c.mem.wr(BASE-16,prefix);c.mem.wr(BASE,image if not native else compiled_image);c.mem.wr(BASE+1200,prefix);sp=ptr(c,1);c.mem.wr(sp-0x4000,bytes([i])*0x4000)
  stores=[];wr=c.mem.wr
  def track(a,data):
   if BASE<=a<BASE+1200:stores.append((a-BASE,len(data),bytes(data).hex()))
   return wr(a,data)
  c.mem.wr=track;c.run(b.entry if native else 0x82c4f418);actual=c.mem.rd(BASE,1200);both.append((actual,c.mem.rd(BASE-16,16),c.mem.rd(BASE+1200,16),stores))
  if actual!=expected or both[-1][1]!=prefix or both[-1][2]!=prefix:errors.append({'case':i,'native':native,'different_bytes':[n for n,(x,y) in enumerate(zip(actual,expected)) if x!=y][:40]})
 if i==0:trace_example={'original':both[0][-1],'native':both[1][-1]}
report={'cases':64,'final_state_errors':errors,'original_image_sha256':hashlib.sha256(image).hexdigest(),'compiled_image_sha256':hashlib.sha256(compiled_image).hexdigest(),'initialized_sha256':hashlib.sha256(expected).hexdigest(),'pre_CRT_image_matches':image==compiled_image,'table_bytes':1200,'records':25,'stride':48,'write_trace':trace_example,'write_trace_matches':trace_example['original']==trace_example['native'],'original_writes':len(trace_example['original']),'native_writes':len(trace_example['native']),'objects':{n:pin(PATHS[n]) for n in ['BrnCoronaManager.cpp']},'scope':'Actual whole native typed-table CRT versus original82C4F418, each from its real static image; all1200 final bytes and adjacent guards are checked. This diagnostic does not equate differing static-image or store-trace phases.','write_footprint_matches':sorted(trace_example['original'])==sorted(trace_example['native']),'status':'TABLE_IMAGES_AND_FINAL_STATE_MATCH' if image==compiled_image and not errors else 'TABLE_PHASE_NOT_CLOSED'}
report['raw_current_image_matches']=F.M.V.x360rd.rd(BASE,1200)==image
assert report['raw_current_image_matches'] and report['pre_CRT_image_matches'] and report['write_footprint_matches']
payload=OUT/'native-initialized-table.bin';assert not payload.exists();payload.write_bytes(both[1][0])
p=emit('final-table-crt-comparison.json',report);print(json.dumps({k:v for k,v in report.items() if k not in ['write_trace','objects']},indent=2));raise SystemExit(bool(errors))
