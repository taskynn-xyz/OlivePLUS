# OlivePLUS
<p align="center">
  <img src="Olive/icons/oliveplus.png" alt="OlivePLUS" width="360">
</p>

<p align="center">

  
  OlivePLUS is a community fork of [Olive 0.1.2](https://github.com/olive-editor/olive), the open-source non-linear video editor.
Current version: **OlivePLUS 1.0.2 | Alpha**.

## What's different from Olive 0.1.2

- GPU acceleration level (Light / Balanced / Aggressive) that also caps preview resolution, a preview thread count, and a GPU picker for multi-adapter machines
- Hardware H.264/H.265 encoding on export for NVIDIA (NVENC), AMD (AMF) and Intel (QuickSync)
- 3D parenting for adjacent tracks, including 3D nulls that pivot their children around the null's own centre
- Layer parenting system
- Camera effect works as a 3D object and can be parented / driven by a 3D null
- Lots of effects
- Refreshed UI

See `Olive/changelog/changelog.html` for the full list.

Note: This fork is vibecoded, don't expect anything to work perfectly lol. (Currently working on improving existing/fixing non functioning features).
It is quite usable.

## Building

Standard CMake build (Qt 5, FFmpeg 4.4):

```bash
mkdir build && cd build
cmake ../Olive
cmake --build . -j
```

Prebuilt Windows, Linux and macOS artifacts are produced by the GitHub Actions workflow in `.github/workflows/build.yml`.

## Licence

Olive is licensed under the GNU GPL v3 — see `Olive/LICENSE`.
