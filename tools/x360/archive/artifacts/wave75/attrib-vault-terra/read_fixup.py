import ida_bytes,ida_pro
open('D:/Reverse/BP-X360-Experimental-Artifacts/wave75/attrib-vault-terra/artist-fixup.bin','wb').write(ida_bytes.get_bytes(0x828E1598,0x30))
ida_pro.qexit(0)
