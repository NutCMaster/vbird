# vBird

<img src="res/vbird_logo.png" alt="vBird logo" width="150">

vBird is a world editor and creator for the Vortex game platform. It is created as a community ran,
open source alternative to the official Vortex Studio with planned extensive features.

## Download

vBird ships as `vbird-win64.zip`. Unpack it anywhere and run `vbird.exe` — no installer, no Qt,
nothing else to install. Windows 10 or 11, 64-bit. Keep the folder together; the exe needs the
DLLs beside it.

## Building from source

```powershell
cmake --preset release
cmake --build --preset release
cmake --build --preset release --target deploy
.\run.ps1
```

Requires MinGW-w64 GCC, CMake 3.21+, Ninja and Qt 6.5+, with `QT_DIR` pointed at your Qt install.
See [BUILDING.md](BUILDING.md) for the full setup, and for the static build that produces a single
self-contained exe.

## Code signing

Windows builds are signed free of charge by the [SignPath Foundation](https://signpath.org/) for
qualifying open-source projects. See [SIGNING.md](SIGNING.md) for the policy, roles, and onboarding
status.

## Licence

GPL-3.0. See [LICENSE](LICENSE).
