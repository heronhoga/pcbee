# PCBEE

A lightweight desktop PCB layout editor written in **C99**, using [raylib 5.5](https://www.raylib.com/) for its window, drawing, and input. This is a working first version focused on placing through-hole footprints and drawing copper traces.

## Build and run

### macOS

Install Apple's command-line tools if needed:

```sh
xcode-select --install
```

From this project directory:

```sh
make -j4
./build/pcbee
```

The first build downloads raylib into `.deps/raylib` and builds it locally. It needs Git and an internet connection. Subsequent builds work offline; no Homebrew packages, CMake, or separate raylib install are required.

Open the included example:

```sh
./build/pcbee examples/demo.pcb
```

### Linux (X11)

The Makefile also includes a Linux build configuration. On Debian/Ubuntu, install raylib's native build dependencies, then use the same build commands:

```sh
sudo apt install build-essential git libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev
make -j4
./build/pcbee
```

The current version has been built and smoke-tested on macOS / Apple Silicon. Linux is not yet verified. A graphical desktop and OpenGL 3.3-capable driver are required.

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

Save/Open use a simple path-entry dialog. Relative paths resolve from the directory you launched the app in. `Ctrl/Cmd + A` clears the field, and `Ctrl/Cmd + V` replaces it with a pasted path. Saving to an existing path replaces that project. New, Open, and closing the window prompt before discarding unsaved edits.

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
src/main.c         Desktop UI, rendering, and input
tests/test_pcb.c   Model/persistence regression tests (no graphics required)
examples/demo.pcb  Example layout
```

The core is independent of raylib, making it a place to add net connectivity and design-rule checks. The UI uses raylib's built-in font and primitive shapes, with no external assets or UI framework.

## Checks

```sh
make test                       # Geometry, bounds, routing, history, file validation
make sanitize                   # Same tests with AddressSanitizer + UBSan
./build/pcbee --smoke-test       # Open a desktop window, render 3 frames, then exit
```

The model tests need only a C compiler and Make; they do not download raylib. Run them from the project root. `make clean` removes build outputs while retaining the downloaded dependency.
