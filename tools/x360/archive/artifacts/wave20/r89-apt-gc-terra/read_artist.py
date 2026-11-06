import ida_bytes, ida_auto, ida_pro
ida_auto.auto_wait()
p='D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r89-apt-gc-terra/artist-table.bin'
open(p,'wb').write(ida_bytes.get_bytes(0x82144a18,38))
ida_pro.qexit(0)
