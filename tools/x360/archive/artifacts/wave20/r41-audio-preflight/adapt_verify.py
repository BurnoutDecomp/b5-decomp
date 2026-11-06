import pathlib

here = pathlib.Path(__file__).resolve().parent
text = (here.parent / "r40-root-complete-preflight/verify_boundary.py").read_text()
text = text.replace('"snd_startup_x360", "sndmix_audio")',
                    '"snd_startup_x360", "sndmix_audio", "sndcontrol", "sndresample", "sndservice")')
text = text.replace('len(fresh) == 19', 'len(fresh) == 22').replace('len(targets) == 15', 'len(targets) == 18')
text = text.replace('    "MIX_create":', '''    "SNDSYSI_100hzserver": snd + "sndservice.cpp",
    "SNDSYSI_variabletimerservice": snd + "sndservice.cpp",
    "SNDSYS_service": snd + "sndservice.cpp",
    "MIX_create":''')
text = text.replace('498ac5ecbc1fe02ccce101125f8886884dada6aa',
                    'e04b2e3fb34756b02898b7b88f634d9e488896f4').replace(
                    '44e3f45ca94c915cad77e01ac56bbd9161d6c3af',
                    '0bb46c7302f8d24a762987e3f6fc3450abedee6a')
with (here / "verify_boundary.py").open("x", encoding="utf-8") as stream:
    stream.write(text)
