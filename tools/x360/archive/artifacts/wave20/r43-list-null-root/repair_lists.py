import pathlib

for name in ("VehicleList", "WheelList", "ChallengeList"):
    path = pathlib.Path(f"src/SharedClasses/DataLists/{name}.cpp")
    raw = path.read_bytes()
    crlf = b"\r\n" in raw
    text = raw.decode("utf-8").replace("\r\n", "\n")
    start = text.index("    // X360: &dword_82FFB25C")
    end_marker = "    const CgsResource::ResourceHandle skInvalidHandle = {};\n"
    stop = text.index(end_marker, start) + len(end_marker)
    text = text[:start] + text[stop:]
    text = text.replace("namespace\n{\n}\n\n", "", 1)
    text = text.replace("namespace\n{\n\n    // The serialised", "namespace\n{\n    // The serialised", 1)
    text = text.replace("maStaticDataLists[i] = skInvalidHandle", "maStaticDataLists[i] = CgsResource::NULLResourcePtr")
    assert text.count("= skInvalidHandle;") == 2
    text = text.replace("= skInvalidHandle;", "= CgsResource::NULLResourcePtr;")
    assert "skInvalidHandle" not in text
    path.write_bytes((text.replace("\n", "\r\n") if crlf else text).encode("utf-8"))
