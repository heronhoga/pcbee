# PCBEE

A lightweight desktop PCB layout editor written in **C99**, using [raylib 5.5](https://www.raylib.com/) for its window, drawing, and input. Version **0.2.0** adds cross-platform builds and packaging to the working through-hole placement and copper-routing editor.

The editor uses the bundled **IBM Plex Mono** monospace font. Use the **EN / Indonesia** button at the upper right (or press **F3**) to switch the interface and interactive lesson between English and Bahasa Indonesia at any time. Switching does not change your board or lesson progress; the selected language lasts for the current session. The font and its [SIL Open Font License](assets/fonts/OFL.txt) are included in desktop packages.

## Learn PCB design

New to designing circuit boards? Start with the [PCB design guide](PCB_DESIGN_GUIDE.md): learn the terminology, calculate a simple LED circuit, practice placement and routing in PCBEE, and follow the steps toward a manufactured board. The guide includes review checklists, exercises, and further learning resources, and is bundled with desktop packages for offline reading.

### Interactive LED lab

Click **Learn PCB** in the toolbar or press **F2** to start an in-app, eight-step lesson (about 10–15 minutes):

1. Choose the current-limiting resistor for a 5 V LED circuit, with feedback on each answer.
2. Place and rotate a connector, resistor, and LED using blue footprint targets.
3. Route the supply, LED drive, and ground connections using highlighted pads and dotted connection hints.
4. Check the completed layout and save your practice board.

The learning panel stays beside the editable board. **Check step** validates your actual placement and copper connections; **Next step** unlocks after a successful check. Checks catch missing connections, touching/crossing nets, connections to unused header pads, and isolated stray traces in this fixed exercise. Earlier objectives are checked again before advancing, and edits invalidate a previous pass. Use **Back**, undo/redo, and the normal editing tools to correct mistakes.

Starting or restarting opens a fresh **63.5 × 45.72 mm** practice board and asks before discarding unsaved edits. **Exit lesson** keeps the current board for editing and saving. Saving uses `led-practice.pcb` by default; lesson progress is not stored in project files. New or successfully opening another project ends the lesson.

Lesson checks apply only to the prescribed LED layout, not arbitrary circuits or manufacturing rules. Dotted hints are not copper or guaranteed obstacle-free routes. The full [design guide](PCB_DESIGN_GUIDE.md) explains the next steps toward fabrication.

## Desktop packages

The build workflow produces these portable archives, each with the example project, README, PCB design guide, third-party licenses, and a `.sha256` checksum:

| Target | Archive | Launch after extracting |
| --- | --- | --- |
| macOS 11+ / Apple Silicon (ARM64) | `pcbee-0.2.0-macos-arm64.zip` | Open `PCBEE.app` |
| Windows 10/11 / x64 | `pcbee-0.2.0-windows-x64.zip` | Open `pcbee.exe` |
| Linux / x64 | `pcbee-0.2.0-linux-x64.tar.gz` | Run `./pcbee` |

Download workflow artifacts from **Actions → Build, test, and package** after a successful run. Version-tag builds also publish these archives to **Releases**. These packages are unsigned; macOS distribution is not Developer ID signed or notarized yet.

All targets require a graphical desktop and OpenGL 3.3-capable driver. Linux packages are built on Ubuntu 22.04 (glibc 2.35 baseline) and use X11, or XWayland in a Wayland session. On Ubuntu, runtime libraries can be installed with:

```sh
sudo apt install libgl1 libx11-6 libxrandr2 libxi6 libxcursor1 libxinerama1
```

raylib is statically linked. The Visual Studio build also statically links the C runtime, so it needs no separate raylib DLL or Visual C++ redistributable.

## Build and run

The cross-platform build uses **CMake 3.24+** and a native C compiler. CMake downloads a SHA-256-verified raylib 5.5 source archive on the first desktop build; later builds use the local copy. A separate raylib installation is not required. The model-only test build needs no network or graphics dependencies.

### macOS ARM64

Install Apple's command-line tools (`xcode-select --install`) and CMake (`brew install cmake`, or the installer from cmake.org). From the project directory:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build/cmake --parallel
open build/cmake/PCBEE.app

# Open the example from a terminal:
./build/cmake/PCBEE.app/Contents/MacOS/PCBEE examples/demo.pcb
```

The bundle's default save path is `~/board.pcb`, so launching from Finder does not try to save into an unwritable working directory.

### Windows x64

Install **Visual Studio 2022** (or Build Tools) with **Desktop development with C++**, including the Windows SDK, plus CMake 3.24+. In PowerShell from the project directory:

```powershell
cmake -S . -B build/cmake -G "Visual Studio 17 2022" -A x64
cmake --build build/cmake --config Release --parallel
ctest --test-dir build/cmake -C Release --output-on-failure
.\build\cmake\Release\pcbee.exe

# Open the example:
.\build\cmake\Release\pcbee.exe examples\demo.pcb
```

The Windows target is a GUI executable. Project paths and command-line arguments use UTF-8 internally, including filenames outside the Windows system code page. MSVC compiles the C99 source in its C11 mode.

### Linux x64 (X11)

On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --parallel
./build/cmake/pcbee examples/demo.pcb
```

Use CMake 3.24+ from cmake.org if your distribution's package is older.

### Create a distributable archive

After building on the target OS:

```sh
cpack --config build/cmake/CPackConfig.cmake -C Release -B dist
```

Archives and checksums appear in `dist/`. To reuse an existing raylib source checkout offline, add `-DFETCHCONTENT_SOURCE_DIR_RAYLIB=/absolute/path/to/raylib` to the configure command. Build in separate directories when changing toolchains or target architectures.

### Lightweight Make build (macOS / Linux)

The original Unix build remains available with only a C compiler, Make, and Git:

```sh
make -j4
./build/pcbee examples/demo.pcb
```

Its first build clones raylib 5.5 into `.deps/raylib`. Linux still needs the native development libraries listed above. Use CMake for Windows and for packaging.

### Editor diagnostics

CMake's Makefile and Ninja generators produce `build/cmake/compile_commands.json`.
The checked-in `.clangd` points clangd at this database so it can resolve raylib's
headers and types. Run the CMake configure command for your platform once, then
restart your editor's language server if diagnostics are stale. Other C/C++
extensions can use the same compilation database; for Visual Studio generators,
use the editor's CMake integration instead. If you choose another build directory,
update the compilation database path in your editor configuration.

## First layout

1. Choose **Resistor**, **Capacitor**, **LED**, **Pin header**, or **DIP-8** from the sidebar.
2. Click on the board to place it. Press **R** to rotate; click again to place another.
3. Choose **Route**, click a pad, then click another pad or a waypoint. The preview uses 45°/90° segments and snaps to nearby pads and trace endpoints.
4. Right-click or press **Enter** to finish the route. Already placed segments stay on the board.
5. Choose **Select** to select or drag a footprint. Select a trace segment or footprint and press **Delete** to remove it.
6. Click **Save**, enter a file path, and confirm. Use **Open** to load it later.

The default board is **101.6 × 76.2 mm**, with a **1.27 mm snap grid**. The visible grid becomes coarser when zoomed out; snapping remains 1.27 mm. Pad diameter is 1.8 mm with a 0.8 mm drill visualization. Trace width can be 0.4, 0.6, or 1.0 mm.

### Controls

| Action | Control |
| --- | --- |
| Select / move | `1` or `V`; left-drag a footprint |
| Route | `2` or `T`; click to commit a segment pair |
| Rotate selected footprint / placement preview | `R` |
| Delete selection | `Delete` or `Backspace` |
| Finish route | Right-click, `Enter`, or `Esc` |
| Cancel a move / exit placement | `Esc` |
| Zoom around pointer | Scroll / trackpad scroll |
| Pan | Middle-button drag or `Space` + left-drag |
| Fit board | `F` |
| New / open / save | `Ctrl/Cmd + N / O / S` |
| Undo | `Ctrl/Cmd + Z` |
| Redo | `Ctrl/Cmd + Shift + Z` or `Ctrl/Cmd + Y` |
| Help | `F1` |
| Start / exit interactive learning | `F2` or **Learn PCB** |
| Switch English / Bahasa Indonesia | `F3` or the language button in the upper right |

Save/Open use a simple UTF-8 path-entry dialog. Relative paths resolve from the directory you launched the app in. `Ctrl/Cmd + A` clears the field, and `Ctrl/Cmd + V` replaces it with a pasted path. Backspace removes one complete Unicode code point; the built-in font may display unsupported glyphs as `?`, but the path is preserved. Saving to an existing path replaces that project on all three platforms. New, Open, and closing the window prompt before discarding unsaved edits.

## Scope of this first version

- One top-copper routing layer, board outline, and footprint/pad display.
- Five built-in through-hole footprints; labels are generated from a unique component ID.
- Placement, dragging, rotation, selection, and deletion.
- Manual 45°/90° routing with pad and endpoint snapping.
- Up to **128 components**, **1,024 trace segments**, and **31 undo steps**.
- Versioned, human-readable `.pcb` projects; validated loading leaves the current board intact on failure. Saving writes a temporary sibling file before renaming it over the destination.

This is a **layout prototype**, not a manufacturing-ready CAD tool. It does not yet implement a schematic editor, nets/ratsnest, electrical or clearance checks, collision prevention, vias, copper pours, multiple copper layers, Gerber/drill export, or native file pickers. Traces are independent geometry: moving or deleting a footprint does not move or remove its traces. The example demonstrates layout editing, not a validated circuit. Board dimensions can be changed in the project file (10–1,000 mm); there is no board-size UI yet.

## Memory and architecture

The PCB model does not call `malloc`. Fixed-capacity arrays hold components and traces, and a ring buffer bounds undo history. The window/graphics library manages its own CPU/GPU allocations, so these model sizes are not the application's total memory footprint. `make test` prints the actual board and history sizes for your compiler.

```text
src/pcb.h          Board types, capacities, and model API
src/pcb.c          Geometry, routing, history, project persistence
src/learn.h        Fixed LED lesson targets and checking API
src/learn.c        Lesson placement and copper-connectivity checks
src/i18n.c         English / Indonesian interface and lesson text
src/ui.c           Monospace text rendering
src/font_data.c    Bundled font loading across platforms
src/main.c         Desktop UI, rendering, and input
src/windows_entry.c Windows GUI entry point and UTF-8 command-line conversion
tests/test_pcb.c   Model/persistence regression tests (no graphics required)
tests/test_learn.c Lesson progression, connectivity, and short-circuit regressions
tests/verify_package.py Archive, architecture, checksum, and packaged-app smoke test
examples/demo.pcb  Example layout
CMakeLists.txt     Portable builds, CTest, and CPack archives
.github/workflows/build.yml Three-target CI and tagged releases
```

The core is independent of raylib, making it a place to add net connectivity and design-rule checks. The UI uses raylib's built-in font and primitive shapes, with no external assets or UI framework.

## Checks

Run model tests from a CMake build:

```sh
ctest --test-dir build/cmake -C Release --output-on-failure
```

For offline, headless model tests with AddressSanitizer + UBSan (GCC/Clang):

```sh
cmake -S . -B build/sanitize -DPCBEE_BUILD_APP=OFF -DPCBEE_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/sanitize --parallel
ctest --test-dir build/sanitize --output-on-failure
```

Add `-DPCBEE_SMOKE_TEST=ON` when configuring a desktop build to register a three-frame window test in CTest. It requires a graphical session; on headless Linux run CTest through `xvfb-run -a` after installing `xvfb` and `xauth`. Tests stay active in Release builds and keep fixtures inside the build directory.

The Make equivalents and direct smoke-test command are:

```sh
make test                       # Geometry, bounds, routing, history, file validation
make sanitize                   # Same tests with AddressSanitizer + UBSan
./build/pcbee --smoke-test       # Open a desktop window, render 3 frames, then exit
./build/pcbee --smoke-test examples/demo.pcb
```

Run Make tests from the project root. `make clean` removes `build/` while retaining `.deps/`.

### CI and releases

Every push and pull request builds on native macOS ARM64, Windows x64, and Linux x64 runners. The workflow runs model/persistence tests (including Unicode paths, overwrite, and Windows sharing violations), creates archives, checks their binary architectures and checksums, and launches the extracted app with the bundled example from a Unicode path. Linux uses Xvfb/Mesa; Windows uses a pinned, checksum-verified Mesa software renderer solely for CI. A separate headless Linux job runs ASan/UBSan.

To publish, update the version in `CMakeLists.txt` and the fallback `PCBEE_VERSION` in `src/main.c`, update the README, then push a matching `vX.Y.Z` tag. The workflow rejects mismatched tags and creates the GitHub release only after all build and sanitizer jobs pass. No signing credentials are needed for the current unsigned archives.
