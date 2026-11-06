import pathlib

here = pathlib.Path(__file__).resolve().parent
text = (here.parent / "r40-root-preflight/verify_boundary.py").read_text()
text = text.replace('("sndfilter", "sndmix", "sndpacket", "sndsys")',
                    '("sndfilter", "sndmix", "sndpacket", "sndsys", "sndvoice", "snd_panning_x360", "snd_startup_x360", "sndmix_audio")')
text = text.replace('len(fresh) == 15', 'len(fresh) == 19').replace('len(targets) == 11', 'len(targets) == 15')
text = text.replace('    "MIX_create":', '''    "MIX_audio": snd + "sndmix_audio.cpp",
    "SNDVOICEI_free": snd + "sndvoice.cpp",
    "SNDI_aztospkrvol": snd + "snd_panning_x360.cpp",
    "SNDSYS_restore": snd + "snd_startup_x360.cpp",
    "MIX_create":''')
text = text.replace('e1fb537b11395d1e21c1f0f6e0f142b7a453d8a9',
                    '498ac5ecbc1fe02ccce101125f8886884dada6aa').replace(
                    'c784b3eb58656b205fa3c09745cb10311faa618b',
                    '44e3f45ca94c915cad77e01ac56bbd9161d6c3af')
with (here / "verify_boundary.py").open("x", encoding="utf-8") as stream:
    stream.write(text)
