# World geometry lifetime regression

Run from the workflow checkout:

```powershell
python b5-decomp/tests/run_world_vertex_lifetime.py
powershell -ExecutionPolicy Bypass -File tools/tests/run_case.ps1 -Case b5-decomp/tests/WorldGeometryStreaming.ps1
```

The 16 focused checks execute the production declaration-retirement methods with
counted COM references. They cover allocation boundaries, address reuse, failed
declarations, duplicate frees, diagnostic pointer lifetime and complete teardown.

The live case streams between Park Vale, Waterfront and River City, revisits the
same locations, and exercises a real crash-camera transition and return to driving.
Its placement sweep uses the existing harness; a separate forced player crash
makes the camera transition deterministic. It requires all five placements,
declaration retirement, zero stale formats and no new assertions or exceptions.
Camera state records, rather than input presses, establish that the cut occurred.
Frame captures remain necessary for checking visual defects beyond this cache.

The original address-only declaration cache survived resource-pool frees. A
baseline sweep observed five stale entries, including cached 28-byte formats used
for 24-byte source vertices and cached 24-byte formats used for 28-byte vertices.
Those mismatches can reinterpret non-position bytes as positions and stretch meshes.

`Pool::FreeMemoryForResource` already notifies `WorldGeometry_OnResourceMemoryFreed`
before returning each allocation to the heap. That notification now also retires
the descriptor cache. The cache owns one D3D declaration reference; the D3D device
retains its own reference while a declaration is bound. Numeric address ordering
allows range retirement without scanning every live declaration on every free.

`BRN_WORLD_GEOMETRY_DIAG=1` compares live descriptor bytes with the cached source
snapshot and reports `[world-vd] STALE` and `[world-vd] retired=`. It is opt-in;
normal rendering does not copy or compare descriptor contents on cache hits.
