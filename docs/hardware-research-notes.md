# Hardware research notes

What we looked up while planning the Jetson build, where it came from, and how sure we are.
Decisions live in [hardware.md](hardware.md) and [shopping-list.md](shopping-list.md); this file
keeps the reasoning and the sources so nobody has to dig them up again. Collected 2026-10-29 to
2026-11-04.

Confidence: **High** = manufacturer document read directly. **Medium** = retailer listing, forum
post from the vendor's staff, or our own calculation. **Low** = inferred, or a search snippet we
couldn't open.

## Jetson power

### 4S straight into the barrel jack (High)

- The Orin Nano dev kit's carrier board takes **9–20 V** on the DC jack; the 19 V adapter in the
  box is just one supply in that range. Source: NVIDIA *Jetson Orin Nano Developer Kit Carrier
  Board Specification* SP-11324-001 v1.3, p. 3 and §3.8 (jack 5.5 × 2.5 mm, centre positive). The
  carrier's own regulator makes the module's 5 V, so no external converter is needed.
- A 4S LiPo is 12.0 V empty to 16.8 V full, inside the range at both ends. The SkyRC e450 charges at
  most 4S, so even a wrong chemistry setting (LiHV, 4.35 V/cell = 17.4 V) stays under 20 V.
- The same 9–20 V range is quoted on NVIDIA's forum
  ([1](https://forums.developer.nvidia.com/t/jetson-orin-nano-input-voltage/298625),
  [2](https://forums.developer.nvidia.com/t/orin-nano-dev-kit-input-voltage/251158)). Some
  third-party pages say 9–19 V or 7–20 V; we go by NVIDIA's spec.
- **Open:** the spec gives 3.5 A for the jack in §3.8, while a forum post cites 4.2 A for DCJ_IN in
  table 5-2. We size everything to the lower figure.

### Current draw (Medium, calculation)

25 W mode plus the D435i is about 28 W. That is ≈ 2.0 A at 14 V (where the buzzer alarms) and
≈ 2.3 A at 12 V. MAXN SUPER on battery would push it higher; use 25 W mode in flight.

### Fuse: 4 A slow-blow, 5×20 mm glass (Medium)

- The fuse protects the wiring and the 3.5 A jack against a short from a battery that can deliver
  far more than either can take.
- 3 A was the first plan (≈ 77 % load at 2.3 A). 4 A slow is the upper limit; 3.15 A slow is equally
  good. Never 5 A. A fast (F) fuse risks blowing on the start-up inrush.
- The Biltema midi holder (35-648) was out of stock in every Trondheim store on 2026-11-04 (nearest:
  Stjørdal), and Biltema's *Kjøp & Hent* needs a 300 kr minimum. Kjell's inline 5×20 holder
  ([41107](https://www.kjell.com/no/produkter/elektro-og-verktoy/elektronikk/sikringer/sikringsholdere/sikringsholder-5-x-20-mm-p41107),
  max 5 A, 1.25 mm² leads) with a 4 A slow fuse
  ([36927](https://www.kjell.com/no/produkter/elektro-og-verktoy/elektronikk/sikringer/glassrorsikringer/glassrorsikring-5-x-20-mm-4-a-trege-t-10-pk.-p36927))
  replaces it. The leads solder straight to 18 AWG, which the 12 AWG Biltema leads didn't.
- Kjell's MIDI blade-fuse holder (41050) and Clas Ohlson had nothing usable in Trondheim.

### Plug-in spike and the 470 µF capacitor (Medium, calculation)

Hot-plugging a LiPo into a board with only small ceramic input caps can ring well above the pack
voltage. A 470 µF / 35 V Panasonic FM (EEU-FM1V471, ESR 19 mΩ) across the plug turns that into an
over-damped charge: with ~1 µH of lead inductance and ~0.1 Ω of loop resistance (fuse, wire, ESR),
damping ratio ζ = (R/2)·√(C/L) ≈ 0.05 · √470 ≈ 1.1. The capacitor's charge energy (½CV² ≈ 0.07 J at
16.8 V) is far below what nuisance-blows a slow fuse. Its legs go into the plug's screw terminal
with the wires: long leg (+) with red, striped leg (−) with black.

### DC plug (Medium)

The Jetson end is a male 5.5 × 2.5 mm plug with a 2-pin screw terminal. Kjell sells it in **male
(39980)** and **female (39982)**; the female doesn't fit the Jetson. We bought DeLOCK's version from
Proshop (3278617). Neither Proshop nor DeLOCK publishes a current rating; this generic CCTV-style
part is normally 2–5 A, and the fuse limits it either way. A 5.5 × 2.1 mm plug fits loosely and can
reboot the Jetson in flight.

### Polarity (High)

Nothing between the pack and the Jetson protects against reversed polarity. The Deans only mates one
way, but which pin gets red is up to whoever solders it. Check with a multimeter before the first
connection: +12 to +16.8 V on the centre pin, Jetson unplugged.

### Buzzer (Medium)

The 2–8S alarm plugs onto the balance lead and is the only low-voltage warning for the Jetson pack
(PX4 can't see it). It draws current all the time, so take it off after use. The 5-pin JST-XH must
sit at the "−" end of its 9-pin header.

## Box contents

| Box | Contents | Confidence | Source |
|---|---|---|---|
| Jetson Orin Nano Super dev kit | Dev kit, 19 V supply, quick-start card. **No storage** | High | [NVIDIA quick start](https://docs.nvidia.com/jetson/orin-nano-devkit/user-guide/latest/quick_start.html) |
| X500 V2 PX4 dev kit | Pixhawk 6C, PM02 V3, M10 GPS, SiK V3, frame kit (plates, arms, landing gear, platform board with Jetson Nano holes, rails, battery mount + 2 straps, hand tools), motors, ESCs, PDB, 6 props. **Depth-camera mount sold separately** | High | [Holybro product page](https://holybro.com/products/px4-development-kit-x500-v2), [Holybro docs](https://docs.holybro.com/drone-development-kit/px4-development-kit-x500v2) |
| X500 V2 frame hardware for a companion computer | M2.5 × 12 socket screws ×4, M2.5 × 5 nylon standoffs ×4, M2.5 locknuts ×4 | Medium | Holybro *X500 v2 Assembly Guide*, p. 2, "Companion Computer Mounting Suggestion" |
| Pixhawk 6C cable set | 5 × GH 6-pin (power/TELEM), 3 × GH 4-pin, 2 × GH 10-pin, CAN/I2C splitter, 2 PWM boards, SBUS and RC DuPont cables | Medium | Retailer listings (GetFPV, Lumenier); Holybro only gives the SKU |
| Holybro UART-to-USB converter | Board (FT231X; USB-C, GH 6-pin, GH 10-pin, SH 6-pin) + "Type-C USB cable". **Not stated whether the other end is A or C** | High / open | [Holybro](https://holybro.com/products/uart-to-usb-converter) |
| Intel RealSense D435i | Camera, USB-C → USB-A cable (~1 m), mini tripod. 1/4-20 thread + 2 × M3 | Medium | Intel store listing (Intel's site was unreachable) |

### Conflict: the depth-camera mount

Holybro's dev kit page and docs say the depth-camera mount is optional and sold separately. The
frame's assembly guide still lists it (#31110) in the part list, and our earlier notes assumed it was
in the box. We print it from Holybro's
[3D-print zip](https://docs.holybro.com/drone-development-kit/px4-development-kit-x500v2/download).
The guide shows the D415/D435 hole pair "B": **45 mm, M3 × 2**. The camera's M3 threads are shallow,
so use short screws.

### Companion-computer mounting (Medium)

- The platform board has **58 × 86 mm** holes for the Jetson Nano (assembly guide p. 2). We haven't
  confirmed the Orin Nano carrier uses the same pattern: measure before printing.
- The kit's M2.5 × 12 screws and 5 mm standoffs leave no room for an M.2 SSD under the carrier once
  it hangs upside down. With our printed 10 mm spacers the stack is 2 + 10 + 1.6 mm plus a nut, so
  the screws need to be 18 mm.

### Wi-Fi antennas (Medium)

The dev kit's two PCB antennas are **not glued** to the plastic base: each is held by a small plastic
tab. Slide it outward, then lift the inside edge (NVIDIA staff,
[forum](https://forums.developer.nvidia.com/t/mounting-jetson-orin-nano-developer-kit/353681/7)).
Cable length and connector type weren't found. Keep Wi-Fi on 5 GHz or off in flight; RC is 2.4 GHz.

## Pixhawk ↔ Jetson link

- PX4's [companion-computer page](https://docs.px4.io/main/en/companion_computer/pixhawk_companion.html)
  wires a USB-serial adapter to TELEM2 **crossed**: TX↔RX, CTS↔RTS, GND↔GND, **+5 V not
  connected**. (High for a generic adapter.)
- The Holybro converter is sold for GPS and telemetry modules, which plug in straight. Its pinout is
  only published as an image (PinoutsV1.3), so we couldn't confirm whether its labels are from the
  host's side. **Read the silkscreen before soldering the crossover.** With pin 1 cut, a wrong guess
  only means no data.
- The converter is an FT231X, so it shows up as `/dev/ttyUSB0` via `ftdi_sio`; 921600 baud is well
  inside its 3 Mbaud limit.

## Storage and install

- The carrier has an M.2 Key M **2280** slot (PCIe x4) and a **2230** slot (PCIe x2). The SSD must
  be NVMe; an M.2 SATA drive fits physically but doesn't work. 2242 drives have no mounting point.
  (Medium, NVIDIA carrier spec via our earlier notes.)
- Install: write NVIDIA's Jetson ISO to a USB stick (≥ 16 GB), boot it with the SSD fitted, pick the
  SSD as the target. **No Ubuntu host PC is needed.** If the UEFI firmware is older than 36.0, go
  through the JetPack 6.x update path first; press Y when the installer offers a QSPI update (it
  times out after 30 s). JetPack 7.2+ no longer supports SD-card images. (High, NVIDIA quick start.)

## Shops and stock (2026-11-04)

| Need | Result |
|---|---|
| Biltema midi fuse holder 35-648 | Out at Tiller and Lade; nearest stock Stjørdal |
| Biltema panel holder 35-642 | Spade terminals for panel mounting, not inline. Not suitable |
| Kjell MIDI blade holder 41050 | Not in Trondheim |
| Kjell 5×20 holder 41107 | 1 at City Lade |
| Kjell 5×20 4 A T 36927 | 1 pack at City Lade; 3.15 A T not in stock there |
| Kjell DC plug 39980 (male) | In stock online; order-only at City Lade, 129 kr shipping |
| DeLOCK DC plug at Proshop | Remote warehouse; delayed the SSD order to 9–11 Nov |
| elefun DC plug | None |
| Elefun, Biltema, Proshop, Kjell search | Kjell's and Biltema's site search fail on URL queries; use Google or the category pages |

## Still unknown

- Whether the converter's USB cable is C-to-A or C-to-C.
- How many GH 6-pin cables are spare on the flying drone.
- The Orin Nano carrier's mounting-hole pattern, measured.
- The D435i's M3 thread depth (Intel datasheet 337029 has it).
- The DeLOCK plug's current rating.
