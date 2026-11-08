# Learn PCB design with PCBEE

A printed circuit board (PCB) holds components and connects them with copper. Good design starts with knowing what the circuit should do, then arranging parts and copper so it works electrically and can be assembled.

This guide takes you from the basic vocabulary to a small LED layout, then explains the steps needed to make a real board. No previous PCB experience is required; knowing voltage, current, resistance, and Ohm's law (`V = I × R`) will help.

See the [README](README.md#build-and-run) to launch PCBEE and the [controls](README.md#controls) for shortcuts.

## 1. Understand the pieces

| Term | Meaning |
| --- | --- |
| Schematic | A diagram of the circuit's electrical connections, independent of physical placement. |
| Symbol | A component's schematic representation, with electrical pins. |
| Footprint | The physical pattern of pads and body outline for a component. It must match the actual part. |
| Pad | Exposed copper where a component lead or terminal connects. Through-hole pads also have holes. |
| Trace | A copper path connecting pads. |
| Net | All the pins and copper intended to be electrically connected, such as ground. |
| Ratsnest | Temporary connection lines showing which pads still need routing. |
| Via | A plated hole connecting copper on different layers. |
| Copper pour | A filled copper region assigned to a net, often ground. |
| Clearance | The edge-to-edge gap between copper features that must remain separate. |
| Solder mask | The insulating coating over most copper, with openings for soldering. |
| Silkscreen | Printed labels such as references, connector names, and polarity marks. |
| ERC / DRC | Electrical rule checking of a schematic / design rule checking of a board. |
| Gerber / drill files | Manufacturing files describing board layers / hole locations and sizes. |

**A schematic symbol and a footprint are different things.** Two parts called “LED” may have different lead spacing or pin assignments. Read the exact part's datasheet before selecting a footprint.

### What you can practice in PCBEE

PCBEE currently provides five through-hole footprints, placement and rotation, and manual routing on one top-copper layer. It is useful for practicing layout decisions and reviewing connections by hand.

It does not yet have schematics, assigned nets, a ratsnest, ERC/DRC, vias, copper pours, or manufacturing export. It will allow copper to cross or touch without reporting a short. A saved `.pcb` project is an editable PCBEE layout, not a fabrication file. The bundled `examples/demo.pcb` demonstrates editing rather than a validated circuit.

## 2. Plan the circuit before placing parts

For your first exercise, design a **5 V LED indicator**:

```text
+5 V ---- resistor ---- LED anode (A)
                        LED cathode (K) ---- GND
```

The resistor limits the LED current. Assuming a red LED with a forward voltage of about 2 V and a target current of 5 mA:

```text
R = (supply voltage - LED forward voltage) / current
  = (5 V - 2 V) / 0.005 A
  = 600 ohms
```

Choose a common **680 ohm** resistor. The approximate current is then `3 / 680 = 4.4 mA`, and resistor dissipation is `3² / 680 ≈ 0.013 W`; a 0.25 W part is ample for these assumptions. Actual LED voltage varies: use the chosen LED's datasheet and check the supply range when designing a real circuit.

### Parts and connections

| Part | Exercise choice | What to check on a real part |
| --- | --- | --- |
| Supply connector | PCBEE's 1 × 4 pin header; use two pins | 2.54 mm pitch, pin numbering, lead size, mating connector |
| Resistor | 680 ohm axial, 0.25 W | Body size and leads bent to fit 7.62 mm pad spacing |
| LED | Red 3 mm through-hole LED | 2.54 mm lead spacing, forward voltage/current, anode/cathode |

Write down these three nets before routing:

| Net | Connections |
| --- | --- |
| +5 V | Header pad 1 → resistor pad 1 |
| LED drive | Resistor pad 2 → LED anode |
| GND | LED cathode → header pad 2 |

Leave header pads 3 and 4 unconnected for this exercise. Ground is the circuit's reference and return path; current must travel back to the supply for the LED to light.

**Pad convention for this exercise:** use the LED's square pad as the cathode and its round pad as the anode. PCBEE makes the first pad of every footprint square, but stores no electrical pin roles or polarity. This convention is a note for your layout, not a guarantee about a physical LED. Check its datasheet; the package's flat side and shorter lead commonly indicate the cathode, but trimmed leads are not a reliable reference.

## 3. Place the parts in PCBEE

1. Start a new board with **Ctrl/Cmd + N**. The default board is 101.6 × 76.2 mm; use a small area for this exercise.
2. Select **Pin header** in the sidebar and click near the left edge, leaving room around its pads and body. Press **Esc** to leave placement mode.
3. Place a **Resistor** to the right of the header and an **LED** farther right. Press **Esc** after placing each part so you do not add extra copies.
4. Use **Select** (`1` or `V`) to drag parts. Use **R** to rotate the selected part, or rotate the placement preview before clicking.
5. Arrange the resistor's second pad near the LED's anode. Leave an open corridor for the cathode's return trace to the header.

With these footprints unrotated, pad 1 is the square pad on the left; the remaining pads run left to right. Rotation changes their position, so track the square pad rather than relying on “left.” Labels use a shared component ID counter: placing the header, resistor, and LED in that order on a fresh board produces `J1`, `R2`, and `D3`.

### Placement habits to learn

- **Place mechanically constrained parts first.** Connectors need to face an accessible edge; switches and mounting holes must match the enclosure.
- **Group by circuit function.** Keep parts that connect closely together, without crowding component bodies or soldering access.
- **Think about return current.** Every signal or power path belongs to a loop. For this LED, keep the ground return straightforward; in faster circuits, return-path continuity becomes especially important.
- **Place decoupling capacitors beside IC supply pins.** On a later IC project, follow its datasheet and keep the supply–capacitor–ground loop short.
- **Choose placement before routing.** If many routes would cross, rearranging parts is often easier than finding a complicated path.

Checkpoint: can you point to both endpoints of each net in the table without guessing?

## 4. Route the three connections

1. Select a **0.4 mm** trace width for this low-current exercise.
2. Choose **Route** (`2` or `T`). Click header pad 1, then resistor pad 1. Click intermediate waypoints if necessary to avoid other pads or bodies.
3. Right-click or press **Enter** to end that route before starting a different net. Ending a route keeps the segments you already placed.
4. Route resistor pad 2 to the LED's round anode pad, then finish the route.
5. Route the LED's square cathode pad back to header pad 2, around the other connections, then finish.
6. Zoom in and check that every route ends at the intended pad center. PCBEE snaps to nearby pads and trace endpoints and previews 45°/90° segments.

On one copper layer, **crossing or touching copper is a connection**. A line that passes through an unrelated pad would also connect to it on a real board. Go around obstacles or move parts; there is no layer-change tool in this version.

The 0.4 mm width is a convenient practice choice, not a general current rating. In a real design, select width using current, copper thickness, acceptable temperature rise, length, and voltage drop. Choose clearance from the fabricator's capabilities and the circuit's voltage requirements. Wider traces and more space than the manufacturing minimum often make simple boards easier to build.

PCBEE's snap grid is 1.27 mm, pads are drawn 1.8 mm across with a 0.8 mm drill visualization, and available trace widths are 0.4, 0.6, and 1.0 mm. These are editor defaults rather than verified manufacturing rules for your components.

## 5. Review and save

Use the net table as a manual checklist:

- [ ] Header pad 1 connects only to resistor pad 1.
- [ ] Resistor pad 2 connects only to the LED anode.
- [ ] The LED cathode connects only to header pad 2.
- [ ] Header pads 3 and 4 remain isolated.
- [ ] No unrelated traces or pads touch or cross.
- [ ] No route stops short of a pad, and no stray segments remain.
- [ ] Component bodies have space for assembly and connector access.

Click **Save** or press **Ctrl/Cmd + S**, enter a path such as `led-practice.pcb`, and confirm. Save to a new filename for each alternative layout you want to keep. Use **Open** or **Ctrl/Cmd + O** to reopen the project.

**Moving, rotating, or deleting a component does not update its traces.** After a placement change, inspect every affected connection, delete obsolete segments, and route again. **Ctrl/Cmd + Z** undoes a mistake.

### Try these learning exercises

1. Make a second layout with shorter total routing while preserving all three nets and assembly space. Explain why you moved each part.
2. Rotate the LED after routing. Identify the broken connections and repair them using your recorded polarity convention.
3. On a practice copy, deliberately cross the +5 V and GND traces. Explain the resulting short, then undo it. Notice that PCBEE does not flag it for you.
4. Recalculate the resistor for a 3.3 V supply using the same assumed LED forward voltage and target current. You should get 260 ohms; choosing 270 ohms gives approximately 4.8 mA. The physical layout can remain the same if the replacement resistor uses the same package.

## 6. Take the design to a manufactured board

To build this circuit, recreate it in a full PCB CAD tool such as [KiCad](https://www.kicad.org/). PCBEE currently has no direct KiCad or Gerber export.

1. **Draw the schematic.** Assign component values, connector functions, and exact pin connections. Run ERC and resolve or explain each finding.
2. **Assign verified footprints.** Check pad numbering, lead spacing, body size, polarity, and hole sizes against datasheets. A 1:1 printout can help check physical fit.
3. **Set the board requirements.** Choose the outline, mounting holes, layer stack, and fabricator's trace, clearance, drill, and copper-to-edge rules before routing.
4. **Place and route.** Transfer the schematic's nets into the board editor. Reuse the placement lessons from the exercise; consider soldering access when choosing which layer carries through-hole connections. On a two-layer design, a ground plane can help provide continuous return paths, but avoid routing that unnecessarily splits it.
5. **Run DRC and inspect.** Resolve unintended shorts, clearance errors, unconnected nets, and outline issues. Check connector orientation and silkscreen polarity labels separately; DRC cannot prove the whole circuit works.
6. **Export and review fabrication files.** Generate the copper, mask, silkscreen, outline, and drill files required by the manufacturer. Open them in a Gerber viewer and inspect every layer and hole. Keep a bill of materials (BOM) with exact parts for assembly.
7. **Assemble and bring up the board.** Inspect solder joints and polarity, check for an unintended supply-to-ground short with power disconnected, then use a current-limited supply for initial power-up. Verify the supply voltage and LED operation against your calculations.

## 7. Keep learning

A useful learning order is:

1. **Circuit fundamentals:** Ohm's law, resistor power, series/parallel circuits, diode polarity, and reading datasheets.
2. **Schematic capture:** symbols, pin numbers, net labels, power connections, and ERC.
3. **Physical design:** footprints, mechanical constraints, placement, routing, and DRC.
4. **Manufacturing and assembly:** board stackups, plated holes, solder mask, fabrication outputs, and soldering.
5. **More advanced layouts:** decoupling, return paths, signal integrity, thermal design, and electromagnetic compatibility.

Resources:

- [KiCad getting started](https://docs.kicad.org/en/getting_started_in_kicad/getting_started_in_kicad.html) — a complete schematic-to-board workflow.
- [SparkFun: PCB basics](https://learn.sparkfun.com/tutorials/pcb-basics/all) — illustrated board terminology and construction.
- [SparkFun: How to read a schematic](https://learn.sparkfun.com/tutorials/how-to-read-a-schematic/all) — symbols and connections.
- Your component manufacturers' **datasheets** and board fabricator's **capabilities/design-rule documents** — the references for your actual parts and production process.

You have completed the first exercise when you can explain every connection, calculate the LED resistor, identify the LED polarity, and describe which checks remain before fabrication.
