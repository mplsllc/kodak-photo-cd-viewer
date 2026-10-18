# Kodak Photo CD Viewer

A native Linux C++/Qt6 application for browsing a mounted Kodak Photo CD. The initial player slice reads the complete `PHOTO_CD/IMAGES/IMG####.PCD` album directly, displays the disc's `STARTUP.PCD`, creates thumbnails in disc order, and opens photographs at selectable Photo CD resolutions. It does not launch or emulate CD-i software.

## Build

Debian/Ubuntu dependencies: `g++`, CMake 3.25+, Qt 6.5+ Widgets and Concurrent development packages.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run the desktop player (it detects mounted Photo CDs on launch):

```sh
./build/photo-cd-player
```

Open a mounted disc or a folder containing `PHOTO_CD/IMAGES` explicitly:

```sh
./build/photo-cd-player --open /media/$USER/your-disc-label
```

Run a real-disc inventory and decode check without opening the window:

```sh
QT_QPA_PLATFORM=offscreen ./build/photo-cd-player --self-check /media/$USER/your-disc-label
```

To export every photograph without the GUI, choose an existing output folder:

```sh
mkdir -p "$HOME/Pictures/Photo-CD-Export"
QT_QPA_PLATFORM=offscreen ./build/photo-cd-player --export-all /media/$USER/your-disc-label "$HOME/Pictures/Photo-CD-Export"
```

## Implemented in this first slice

- Native Qt6 Widgets executable; no browser or Electron runtime.
- Mounted-volume discovery plus a manual directory picker.
- Case-insensitive `PHOTO_CD` / `IMAGES` lookup and numeric `IMG####.PCD` ordering.
- In-process pcdDecode integration for sRGB output, orientation correction, 16Base/4Base/Base resolution levels, and image navigation.
- Startup artwork loading, index thumbnails, zoom, pan, rotation, favorite marks, and timed slideshows.
- **Export all photographs…** is available on the opening screen, in the File menu, and with Ctrl+Shift+E. It exports highest standard Photo CD resolution as quality-95 JPEG, reports progress, can be canceled between photographs, skips existing filenames, and refuses destinations inside the source disc.
- Disc access errors are shown when the mount disappears or a read fails.

To install the executable under your account and optionally enable desktop opening:

```sh
cmake --install build --prefix "$HOME/.local"
"$HOME/.local/share/photo-cd-player/install-user-integration.sh"
```

The integration script is opt-in and only changes user-level MIME and application settings. The interface is guide-informed and intentionally a first reconstruction, not a claim of exact pixel fidelity. The Philips v3.x guide documents the menu choices and controls; recovered CD-i assets remain local and are not bundled. The locally examined Philips application reports version 3.1 (April 14, 1993); the separate public archive used for v3.3 static analysis is not that disc. The repository contains the native viewer source and licensed decoder; it does not redistribute the original Philips program or proprietary CD-i graphics.

## Known gaps

- `OVERVIEW.PCD` is a Kodak overview pack, not a normal photo. The generic decoder reports interleaved audio when asked to decode it; this app builds the index from individual image files instead.
- No parser for all `OPTIONS.PCD`/`INFO.PCD` metadata, CD-i asset formats, saved playlist format, or multisession disc-image container is included yet.
- Custom ordered playlists, hidden-photo management, and original-language menus are not implemented yet.
- Automatic desktop insertion launch is not enabled by the build. After installing to your user prefix, run `~/.local/share/photo-cd-player/install-user-integration.sh` to register the Photo CD tree type and make this app the user-level opener. The desktop environment may still require explicit approval to launch on insertion.
- No original Philips program or graphic assets are redistributed.

Reverse-engineering evidence and confidence levels are in [`research/ledger.md`](research/ledger.md). Local extracted reference binaries are kept outside version control under `research/private/`.
