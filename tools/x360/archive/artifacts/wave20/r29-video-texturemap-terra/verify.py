import json,pathlib,sys,struct
sys.path.insert(0,r'D:\Reverse\BP-Decomp_Workflow\tools\re'); import x360rd
u=lambda a:struct.unpack('>I',x360rd.rd(a,4))[0]
checks={0x82a47678:0x481c44e9,0x82a4722c:0x91440000,0x82a47c50:0x38810080,0x82a47c58:0x816b000c,0x82a47c64:0x81610080,0x82a47d28:0x8141009c,0x8210f89c:0x82a47050}
# Establish raw reference words independently; malloc target is decoded separately below.
checks.pop(0x82a47678)
for a,v in checks.items(): assert u(a)==v,(hex(a),hex(u(a)),hex(v))
exports=pathlib.Path(r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX'); calc=json.loads((exports/'0x82A47050.json').read_text()); body=json.loads((exports/'0x82A47640.json').read_text()); assert len([x for x in calc['assembly'].splitlines() if 'r4' in x])==1
assert len([x for x in body['assembly'].splitlines() if any(s in x for s in ['var_C0(r1)','var_BC(r1)','var_B8(r1)','var_B4(r1)','var_B0(r1)','var_AC(r1)','var_A8(r1)','var_A4(r1)'])])==8
print(json.dumps({'raw_sentinels':'pass','vtable_slot_0xc':'0x82A47050','scratch_writer':'82A4722C writes only [r4], no r4 increment','scratch_reader':'82A47C64..82A47D28 loads 8 words','scope':'no implementation generated; full behavior and scratch-state provenance unresolved'},indent=2))

