from pathlib import Path
p=Path(r"D:/Reverse/BP-X360-Experimental-Worktrees/scene_lanes_luna_129a/vendor/dirtysdk/src/x360/connapi_x360.cpp")
s=p.read_text(encoding="utf-8")
repls=[
("""    ProtoMangleControl(ref->pProtoMangle, 0x686f7374, 0, 0, host);""",
 """    ProtoMangleControl(ref->pProtoMangle, 0x686f7374, 0, 0,\n                       &host->UserInfo.DirtyAddr);"""),
("""        s32 address = ConnApiGetConnectAddr(ref, client, 1);\n        NetPrintfCode("connapi: establishing game connection to user %s:%s at %d\\n",\n                      client->UserInfo.strName,\n                      SocketInAddrGetText(address), NetTick());""",
 """        s32 address = ConnApiGetConnectAddr(ref, client, 1);\n        const u32 tick = NetTick();\n        const char* addressText = SocketInAddrGetText(address);\n        NetPrintfCode("connapi: establishing game connection to user %s:%s at %d\\n",\n                      client->UserInfo.strName, addressText, tick);"""),
("""        if (client->iTunnelId > 0)\n        {\n            NetPrintfCode("connapi: tunnel allocated for client %d; switching to use virtual address %s\\n",\n                          clientIndex, SocketInAddrGetText(client->iTunnelId));\n            address = client->iTunnelId;\n        }\n        NetPrintfCode("connapi: establishing game connection to user %s:%s at %d\\n",\n                      client->UserInfo.strName,\n                      SocketInAddrGetText(address), NetTick());""",
 """        if (client->iTunnelId > 0)\n        {\n            const char* tunnelAddress = SocketInAddrGetText(client->iTunnelId);\n            NetPrintfCode("connapi: tunnel allocated for client %d; switching to use virtual address %s\\n",\n                          clientIndex, tunnelAddress);\n            address = client->iTunnelId;\n        }\n        const u32 connectTick = NetTick();\n        const char* connectAddressText = SocketInAddrGetText(address);\n        NetPrintfCode("connapi: establishing game connection to user %s:%s at %d\\n",\n                      client->UserInfo.strName, connectAddressText, connectTick);"""),
("""        NetPrintfCode("connapi: destroying voip connection to user %s: %s at %d\\n",\n                      client->UserInfo.strName,\n                      "connection closed (gameonly)", NetTick());""",
 """        const u32 tick = NetTick();\n        NetPrintfCode("connapi: destroying voip connection to user %s: %s at %d\\n",\n                      client->UserInfo.strName,\n                      "connection closed (gameonly)", tick);"""),
("""        s32 address = ConnApiGetConnectAddr(ref, client, 2);\n        NetPrintfCode("connapi: establishing voip connection to user %s:%s at %d\\n",\n                      client->UserInfo.strName,\n                      SocketInAddrGetText(address), NetTick());""",
 """        s32 address = ConnApiGetConnectAddr(ref, client, 2);\n        const u32 tick = NetTick();\n        const char* addressText = SocketInAddrGetText(address);\n        NetPrintfCode("connapi: establishing voip connection to user %s:%s at %d\\n",\n                      client->UserInfo.strName, addressText, tick);"""),
("""        if (client->iTunnelId > 0)\n        {\n            NetPrintfCode("connapixenon: tunnel allocated for client %d; switching to use virtual address %s\\n",\n                          clientIndex, SocketInAddrGetText(client->iTunnelId));\n            address = client->iTunnelId;\n        }""",
 """        if (client->iTunnelId > 0)\n        {\n            const char* tunnelAddress = SocketInAddrGetText(client->iTunnelId);\n            NetPrintfCode("connapixenon: tunnel allocated for client %d; switching to use virtual address %s\\n",\n                          clientIndex, tunnelAddress);\n            address = client->iTunnelId;\n        }""")]
for old,new in repls:
    if old not in s:
        raise SystemExit("missing source fragment: " + old.splitlines()[0])
    s=s.replace(old,new,1)
p.write_text(s, encoding="utf-8", newline="\r\n")
