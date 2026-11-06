exec(open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r157-render-linked\inspect_pe.py').read().split('results=[]')[0])
for a,b in [(0x826f8dec,0x826f8e80),(0x826f7b20,0x826f7c50),(0x826f99d0,0x826f9ab0),(0x825cd250,0x825cd280),(0x825cd3a8,0x825cd400),(0x821d381c,0x821d3900)]:
 print('\n',hex(a));
 for i in cs.disasm(pe.get_data(a-pe.OPTIONAL_HEADER.ImageBase,b-a),a): print(f'{i.address:08x} {i.bytes.hex()} {i.mnemonic} {i.op_str}')
