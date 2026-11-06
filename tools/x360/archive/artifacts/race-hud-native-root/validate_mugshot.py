from pathlib import Path
import ast,json,random,struct,sys,hashlib

HERE=Path(__file__).resolve().parent
prefix=Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-bloom-root/validate_constructor.py')
nodes=[]
for n in ast.parse(prefix.read_text()).body:
    if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='report_path' for t in n.targets):break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(prefix),'exec'),globals())
obj=Path(sys.argv[1]) if len(sys.argv)>1 else Path('D:/Reverse/BP-X360-Experimental-Artifacts/roadrule-native-terra/whole1/BrnRaceMainHudState-64edb14b.obj')
product=PackedCoff(obj,'?HandleMugshotEvent@RaceMainHudState@')
bundle=PackedBundle([product])
bundle.select('?HandleMugshotEvent@RaceMainHudState@')
BASE,CACHE,EVENT,VTABLE,SETSTATE=0x51000000,0x52000000,0x53000000,0x54000000,0x54000100
mask=0xFFFFFFFF
reader=F.M.V.x360rd
assert reader.u32(0x82815570)==0x7C8A2378
assert reader.u32(0x82F264A8)==4
hi=reader.u32(0x82474B2C)&0xFFFF
lo=reader.u32(0x82474B40)&0xFFFC
lo=lo-0x10000 if lo&0x8000 else lo
original_filter=((hi<<16)+lo)&mask
methods=[('DoMugshot',0x82474B20),('DoRoadRuleShot',0x82474D10),('HandleMugshotEvent',0x82475CD0)]
original_hooks={
    0x8241C710:'run',0x8246CFF0:'text_parameter',0x8246CD00:'text_id',
    0x8246CBD8:'clear',0x824151F0:'setup',0x82415598:'show',
    0x82415620:'snap',0x82415790:'hide',SETSTATE:'state',
}
def service(name,c,trace):
    p=lambda r:c.g(r)&mask
    if name in ('run','state'):
        trace.append((name,p(3)-BASE,F.cstring(c,p(4))))
    elif name=='text_parameter':
        trace.append((name,p(3)-BASE,F.cstring(c,p(4)),p(5),p(6),F.cstring(c,p(7)),p(8)))
    elif name=='text_id':trace.append((name,p(3)-BASE,F.cstring(c,p(4)),p(5)))
    elif name=='setup':
        trace.append((name,p(3)-BASE,p(4),p(5)&255,F.cstring(c,p(6)),c.g(7)&0xFFFFFFFFFFFFFFFF,p(8)&255))
    elif name in ('show','snap'):trace.append((name,p(3)-BASE,p(4)&255))
    elif name in ('clear','hide'):trace.append((name,p(3)-BASE))
    elif name=='player':
        assert 0<=p(4)<8
        c.setg(3,p(3)+0xAC80+312*p(4));return
    else:raise AssertionError(name)
    c.setg(3,0)

def bind_symbols():
    hooks={}
    for a,n in bundle.unresolved.items():
        key=None
        if '?Run@FlaptAnimatorComponent@' in n:key='run'
        elif '?GetOnlinePlayerInfo@GuiCache@' in n:key='player'
        elif '?SetLocalisedText@TextFieldRef@' in n:key='text_parameter' if 'ZZ' in n else 'text_id'
        elif '?ClearText@TextFieldRef@' in n:key='clear'
        elif '?SetupComponent@RoadRuleShotComponent@' in n:key='setup'
        elif '?Show@RoadRuleShotComponent@' in n:key='show'
        elif '?Snap@RoadRuleShotComponent@' in n:key='snap'
        elif '?Hide@RoadRuleShotComponent@' in n:key='hide'
        if key:hooks[a]=key
    return hooks

rng=random.Random(0x82475CD0)
errors=[];totals={}
for name,original in methods:
    bundle.select('?'+name+'@RaceMainHudState@')
    symbol_hooks=bind_symbols()
    filter_symbol=next((a for a,n in bundle.unresolved.items() if '?gxMessageFilterFlags@' in n),None)
    count=108
    for index in range(count):
        image=(index%36)//6;response=index%6;aggressor=(index//36)&1
        mode=[0,15,16][index//36]
        state=rng.randbytes(0x8100);cache=bytearray(rng.randbytes(0x14000));event=bytearray(rng.randbytes(32))
        car=100+(index%8)
        if name=='DoRoadRuleShot' and index%5==0:car=999
        struct.pack_into('>I',cache,0x9E58,mode)
        for i in range(8):
            at=0xAC80+312*i;struct.pack_into('>I',cache,at+276,100+i)
            tag=('player'+str(i)).encode()+b'\0';cache[at+256:at+272]=tag.ljust(16,b'\0')
        road=rng.getrandbits(64)
        struct.pack_into('>QiiiB',event,0,road,response,image,car,aggressor)
        observed=[]
        for native in (False,True):
            c=cpu(bundle)
            c.mem.add(BASE,len(state),state);c.mem.add(CACHE,len(cache),bytes(cache));c.mem.add(EVENT,len(event),bytes(event));c.mem.add(VTABLE,0x1000)
            c.mem.add(original_filter,8,b'\0'*8)
            if filter_symbol is not None:c.mem.add(filter_symbol,8,b'\0'*8)
            F.put(c,BASE+0x140,CACHE);F.put(c,BASE+0x649C,VTABLE);F.put(c,VTABLE+12,SETSTATE)
            trace=[]
            for a,key in {**original_hooks,**symbol_hooks}.items():
                c.hooks[a]=lambda x,k=key:service(k,x,trace)
            c.setg(3,BASE);c.setg(4,EVENT)
            try:c.run(bundle.entry if native else original)
            except Exception as exc:
                print('MODEL_ERROR',name,index,native,hex(c.last_pc),repr(exc),flush=True);raise
            observed.append((trace,c.mem.rd(BASE,len(state)),c.mem.rd(CACHE,len(cache)),c.mem.rd(EVENT,len(event))))
        if observed[0]!=observed[1]:
            errors.append({'method':name,'index':index,'image':image,'response':response,'mode':mode,
                           'original_trace':observed[0][0],'product_trace':observed[1][0],
                           'bytes_equal':observed[0][1:]==observed[1][1:]})
    totals[name]=count
result={'cases':sum(totals.values()),'methods':totals,'mismatches':errors,
        'object':str(obj),'object_sha256':sha(obj),'verifier_sha256':sha(__file__),
        'original_filter':hex(original_filter),
        'limits':'Actual three whole ARTIST and compiled methods and real compiled frame/id tables. Mode/image/response/road/aggressor/player-slot inputs varied; logging filter is zero, cache nonnull. Renderer/text/animation/RoadRuleShot services controlled; player lookup returns the actual typed record stride and data. Composed HandleMugshot executes its real two child bodies, not hooks. Exact receiver addresses, consumed values, ordered service calls and full poisoned state/cache/event/guard bytes compared; no provider/render/UI runtime, whole HUD layout, allocator, link, launch or hardware claim. Existing independent overall HUD sizeof/allocation discrepancy remains a negative and is not hidden by these bounded member-call cases.'}
p=HERE/'mugshot-comparison-v1.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'cases':sum(totals.values()),'mismatches':len(errors)}),flush=True)
if errors:print(json.dumps(errors[:2],indent=2))
assert not errors
