
from pathlib import Path
import hashlib,json,sys
root=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r161-native-inputpads')
sys.path.insert(0,str(root))
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import rd,u32
import verify_focus as v
v.OUT=root/'assert-provenance'
stem='CgsInputPads'
symbol='?Update@InputPads@CgsInput@@'
rows=v.body(stem,symbol)
path=r'd:\p4\b5_main\burnout\main\code\gameshared\gameclasses\system\input\CgsInputPads.cpp'
data=(v.OUT/f'{stem}.obj').read_bytes()
def raw_literal(address):
 return rd(address,160).split(bytes(1))[0].decode('ascii')
assert raw_literal(0x820f92c8)==path and data.count(path.encode()+bytes(1))==1
code, rels=v.coff_body(stem,symbol)
_, calls=v.verify_coff_calls(stem,symbol,
    [0x130,0x140,0x144,0x15c,0x16c,0x170,0x1b4,0x1c4,0x1c8,0x1dc],
    ['?BeginAssert','?FireAssert','?EndAssert']*3+['?GetPadDisconnectedQueue'],rows)
literal_symbol=lambda offset: rows[offset][1].rsplit(',',1)[1].strip()
assert literal_symbol(0xc0)==literal_symbol(0xc8) and 'b5_main' in literal_symbol(0xc0)
assert code[0x12c:0x130] == bytes.fromhex('419A001C') # success skips to 0x148 after EndAssert
assert code[0x158:0x15c] == bytes.fromhex('409A001C') # success skips to 0x174 after EndAssert
assert code[0x1b0:0x1b4] == bytes.fromhex('409A001C') # success skips to 0x1cc before disconnect enqueue
data_rows=[
 (274,0x828f87e0,0x820fb76c,0x134,0x13c,0xbc,0x148,'GetPlayerID'),
 (275,0x828f880c,0x820fb7a0,0x160,0x168,0xb0,0x174,'IsConne'),
 (288,0x828f887c,0x820fb7c8,0x1b8,0x1c0,0xa0,0x1cc,'IsBound'),
]
proof=[]
for line,original,raw_message,emitted,r3,address,continuation,marker in data_rows:
 word=0x38a00000|line
 assert u32(original)==word and int.from_bytes(code[emitted:emitted+4],'big')==word
 assert code[emitted+4:emitted+8]==bytes.fromhex('7EE4BB78') # r4 <- r23, one direct path literal
 assert marker in literal_symbol(address)
 message=raw_literal(raw_message)
 assert data.count(message.encode()+bytes(1))==1
 source_register=int(rows[r3][1].rsplit('r',1)[1])
 path_register=int(rows[address][1].split('r',1)[1].split(',',1)[0])
 assert source_register==path_register and 'mr           r3,' in rows[r3][1]
 proof.append(dict(line=line,artist_instruction=f'0x{original:X}',message=message,
     emitted_instruction=f'0x{emitted:X}',continuation=f'0x{continuation:X}',literal_symbol=literal_symbol(address)))
assert not v.git('status','--porcelain=v1')
receipt=dict(commit=v.git('rev-parse','HEAD'),parent=v.git('rev-parse','HEAD^'),tree=v.git('rev-parse','HEAD^{tree}'),
             path_literal=path,assertions=proof,coff_calls=calls,
             compile_receipt_sha256=v.sha(v.OUT/'compile-receipt.json'))
(v.OUT/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Verified raw ARTIST literals, exact line operands, direct-literal COFF assertions and continuation')

