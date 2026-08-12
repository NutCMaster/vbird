# Building vBird

vBird is a Qt 6 Widgets application built with CMake and Ninja.

## Quick start (MSYS2)

MSYS2 is the lightest way to get a complete toolchain plus Qt. The whole set is
about 1 GB, versus 2.77 GB for the static Qt package alone.

```powershell
winget install MSYS2.MSYS2
```

Then from an MSYS2 shell:

```bash
pacman -Syu --noconfirm
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake \
                   mingw-w64-x86_64-ninja mingw-w64-x86_64-qt6-base \
                   mingw-w64-x86_64-qt6-imageformats
```

Build from the **MINGW64** shell (not the plain MSYS shell — the toolchain and
Qt only exist in the MinGW64 environment):

```bash
cd /c/Users/<you>/Downloads/vbird
export QT_DIR=/mingw64
cmake --preset release
cmake --build --preset release
cmake --build --preset release --target deploy
```

The runnable result is `build/release/bin/`. Launch it with `.\run.ps1`.

`cmake --build --preset release --target package` writes
`build/release/vbird-win64.zip`, which unpacks to a `vbird/` folder that runs on
a clean Windows 10/11 machine.

### Using a Qt installed elsewhere

Nothing is tied to MSYS2. Point `QT_DIR` at any Qt 6.5+ MinGW install
(`C:\Qt\6.9.1\mingw_64`) and the presets work unchanged.

## Presets

| Preset | Qt | Output |
| --- | --- | --- |
| `dev` | shared | Debug build, incremental |
| `release` | shared | Optimised; ship the folder |
| `dist` | static | **Single self-contained `vbird.exe`** |

Targets: `deploy` (gather the runtime), `package` (zip it), `check-deps` (verify
portability), `run` (launch).

## Everyday work

Header dependencies are tracked, so editing a header rebuilds exactly what
depends on it. `AUTOMOC` is on: add a `Q_OBJECT` class and moc runs by itself.

New `.cpp` files need adding to the `qt_add_executable` list in
[CMakeLists.txt](CMakeLists.txt) — globbing is deliberately avoided so CMake
knows to reconfigure when the file set changes.

## Verifying portability

**The exe running on your machine proves nothing.** Your Qt and MinGW DLLs are
already on `PATH`, so a broken deployment still starts here and fails everywhere
else. That is not hypothetical — it is exactly what happened during this build's
setup, twice.

```powershell
cmake --build --preset release --target check-deps
```

This walks the full transitive import graph from `vbird.exe` and hard-fails on
anything not present in the folder. A clean result:

```
-- Subsystem : GUI (no console window)
-- Self-contained folder: 26 bundled DLL(s), all present.
-- Ship: the whole folder, not vbird.exe on its own.
-- Platform plugin: platforms/qwindows.dll present.
```

Import analysis still cannot see plugins loaded by filename at runtime, so the
final check is running the extracted zip on a machine that has never had Qt
installed.

## Shipping a single .exe

The `dist` preset produces one self-contained file, but needs a static Qt.
MSYS2 has one prebuilt:

```bash
pacman -S mingw-w64-x86_64-qt6-static     # 555 MB download, 2.77 GB installed
```

```powershell
$env:QT_STATIC_DIR = 'C:\msys64\mingw64\qt6-static'
cmake --preset dist
cmake --build --preset dist
cmake --build --preset dist-check
```

[tools/build-static-qt.ps1](tools/build-static-qt.ps1) builds one from source
instead, if you want a smaller or differently configured Qt. That takes 30–60
minutes and needs Perl and a real Python.

## Design notes

**Why the exe is GUI-subsystem.** `qt_add_executable(vbird WIN32 ...)` passes
`-mwindows`. Without it Windows opens a console window behind the app.

**Why `-static` is conditional.** Folding the GCC runtime into the exe is right
for a static Qt and wrong for a shared one: distro Qt DLLs are themselves linked
against `libstdc++-6.dll`, so a statically linked exe would put two C++ runtimes
in one process with separate allocator and exception state. Those DLLs have to
ship anyway, so there is nothing to gain. [CMakeLists.txt](CMakeLists.txt) keys
this off the `TYPE` of the imported `Qt6::Core` target, so the build cannot be
configured into a state that contradicts the Qt it links.

**Why deployment has a second pass.** `windeployqt` handles Qt's own libraries
and the compiler runtime, but not the third-party libraries a distro Qt links
against. With MSYS2's Qt it left behind 28 DLLs — ICU, PCRE2, zlib, zstd,
FreeType, HarfBuzz, GLib and more. [cmake/DeployDeps.cmake](cmake/DeployDeps.cmake)
walks the import graph to closure and copies whatever is missing.

**Why `windeployqt` is called directly.** `qt_generate_deploy_app_script` runs
two deployment passes — one honouring `QT_DEPLOY_*`, one following the Qt
install's own layout — so every DLL landed twice and the folder came out at
59 MB instead of 30.

## Licensing

vBird is GPL-3.0; Qt's open-source build is LGPL-3.0. Dynamic linking is
unrestricted. Static linking is also permitted, and publishing vBird's source
alongside the build scripts satisfies the LGPL requirement that users be able to
relink against a modified Qt.

## Not yet done

- The exe carries no `VERSIONINFO` resource (no publisher, version or
  description in its Properties dialog).
- It is unsigned, so SmartScreen will warn on first run for downloaded copies.
  Fixing that needs a code-signing certificate.
