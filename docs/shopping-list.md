# Shopping list

What we bought to put the Jetson on the drone, what we dropped and why, and what is still open.
Wiring and mounting: [wiring-plan.html](wiring-plan.html) ([published page](https://claude.ai/artifact/RdgNP78C67CBJgxnPv8qJ5)).
Built from [hardware.md](hardware.md). Status as of 2026-11-04.

The drone already flies (frame, Pixhawk, radios, Pocket, flight pack and charger are done). This
list only covers the Jetson: power, storage and mounting.

## Ordered

### elefun.no: order 1280826, 364 kr (ordered 2026-11-04)

Letter parcel with Posten to the home address. Not trackable.

| Item | For | kr |
|---|---|---|
| [Bronto adapter, male Deans → female XT60](https://www.elefun.no/vare-69422/ladekabel-overganger-bronto-overgang-male-deans-til-female-xt60) | Charging the Jetson pack on the e450's XT60 lead | 59 |
| [Bronto T-Plug (Deans), 3× male](https://www.elefun.no/vare-55825/kontakter-bronto-t-plug-deans-3x-male) | Jetson lead, battery end. Three in the pack, so a bad solder joint isn't a problem | 29 |
| [Bronto silicone wire 18 AWG, 1 m red + 1 m black](https://www.elefun.no/vare-56910/ledning-bronto-silikonkabel-18awg) | Jetson lead | 39 |
| [Fuse heat-shrink 5 mm, 2 m](https://www.elefun.no/vare-41511/krympeplast-bunting-fuse-5mm-2-meter) | Jetson lead joints and the XM+ joints | 45 |
| [Bronto heat-shrink 2 mm, 2 m](https://www.elefun.no/vare-57841/krympeplast-bunting-bronto-2mm-2-meter) | Small joints on the TELEM2 crossover cable | 36 |
| [Panasonic FM 35 V 470 µF low-ESR (EEU-FM1V471)](https://www.elefun.no/vare-38843/) | Across + and − at the plug. Damps the plug-in spike. Stripe = − | 12 |
| [Buzzer 2s–8s LiPo alarm & voltage tester](https://www.elefun.no/vare-33067/radioutstyr-tilbehor-buzzer-2s-8s-lipo-alarm-voltage-tester) | Low-voltage warning on the Jetson pack, set to 3.5 V/cell | 95 |
| Shipping | | 49 |

### proshop.no: order 70193376, 746 kr (ordered 2026-11-04)

PostNord to the parcel locker at Rema 1000 Havstein (John Skaarvolds veg 40A). Expected to ship
2026-11-09 to 11-11; the DC plug comes from Proshop's remote warehouse and holds the order back.

| Item | For | kr |
|---|---|---|
| [Intenso Premium 250 GB, NVMe PCIe 3.0 x4, M.2 2280](https://www.proshop.no/SSD/Intenso-Premium-M2-SSD-250GB-PCIe-30-M2-2280/3072745) | Jetson storage, 2280 slot | 676 |
| [DeLOCK 2-pin terminal block → 5.5 × 2.5 mm DC plug (male)](https://www.proshop.no/Stroemkabel-intern/DeLOCK-Power-Adapter-2-pin-Terminal-Block-to-DC-jack-55-x-25-mm/3278617) | Jetson end of the lead. No current rating is published; these are normally 2–5 A and the 4 A fuse limits it anyway | 70 |

A free Norton licence came bundled with the SSD (0 kr). Ignore it.

### Kjell & Company, Trondheim City Lade: buy in the store

Replaces the Biltema blade-fuse holder, which was sold out in every Trondheim store. Both were in
stock (1 each) on 2026-11-04. Reserve with *Hent i butikk* to be sure.

| Item | Art.nr. | kr |
|---|---|---|
| [Sikringsholder 5 x 20 mm](https://www.kjell.com/no/produkter/elektro-og-verktoy/elektronikk/sikringer/sikringsholdere/sikringsholder-5-x-20-mm-p41107): inline, 2 × 12 cm 1.25 mm² leads, max 5 A | 41107 | 9 |
| [Glassrørsikring 5 x 20 mm 4 A Trege (T), 10-pk.](https://www.kjell.com/no/produkter/elektro-og-verktoy/elektronikk/sikringer/glassrorsikringer/glassrorsikring-5-x-20-mm-4-a-trege-t-10-pk.-p36927) | 36927 | 39 |

3.15 A slow (T) is just as good if they have it. Never a 5 A fuse, and avoid fast (F) fuses.

## Dropped, and why

| Item | Why we don't need it |
|---|---|
| Biltema midi fuse holder (35-648) + 3 A blade fuses (24-309) | Holder sold out in Trondheim. The Kjell glass-fuse holder does the same job |
| Kjell DC plug 5.5 × 2.5 (39980) | The DeLOCK plug from Proshop is the same part and came in the same order |
| LiPo-safe bag | Packs are stored in a metal box (not airtight) and charged supervised on a concrete floor with sprinklers |
| 2× 18650 for the Pocket | The drone already flies, so the Pocket has cells |
| M2.5 × 18 screws, M2.5 locknuts | Get locally or from the lab. The frame kit has 4 × M2.5 locknuts (assembly guide) |
| USB-A → USB-C cable for the converter | We'll find one if the converter's cable turns out to be C-to-C |
| microSD card | The SSD replaces it |

## Still open

| What | Why | Status |
|---|---|---|
| USB-A → USB-C cable | Holybro only says "Type-C USB cable" for the converter. The Jetson has USB-A ports | Check the box |
| Spare JST-GH 6-pin cable | Cut into the TELEM2 crossover. Retailers list 5 in the 6C cable set | Count the spares |
| M2.5 × 18 screws | The kit's M2.5 × 12 are too short with 10 mm printed spacers and the SSD under the carrier | Get locally |
| M3 screws for the camera mount | D435i has 2 × M3 on a 45 mm pattern; the threads are shallow, so keep them short | Size once the mount is printed |
| Strap for the Jetson pack | Zip ties + a spare red strap | Have |
| USB stick ≥ 16 GB | To install JetPack onto the SSD with NVIDIA's Jetson ISO. No Ubuntu PC needed | Have |

## Building the Jetson power lead

```
Jetson pack ═ female Deans ═ male Deans ─┬─ red ── splice ─ [5×20 glass fuse, 4 A T] ─ splice ── red ──┬── DeLOCK + (centre)
                                          │                                                            470 µF (stripe to −)
                                          └─ black ─────────────────────────────────────────────────────┴── DeLOCK − (sleeve)
```

Illustration: [hardware/blender/wiring_lead/](../hardware/blender/wiring_lead/) (render and close-up
of the plug end).

1. Solder red and black 18 AWG to the male Deans. Mind the polarity: plug the bare Deans into the
   pack first and mark + with the multimeter.
2. Cut the red wire 5–10 cm from the Deans, so the fuse sits close to the battery.
3. Slide 5 mm heat-shrink on, then solder one fuse-holder lead to each red end. Shrink.
4. Fit the 4 A slow fuse and screw the holder halves tight. Tape over the seam so vibration can't
   loosen it, and zip-tie the holder to the frame.
5. Black goes straight to − on the DeLOCK plug, with no fuse.
6. The capacitor's long leg (+) goes into the + screw with the red wire, the striped leg (−) into
   the − screw with the black wire. Reversed, an electrolytic capacitor can burst.
7. **Before connecting the Jetson:** with the fuse in, the multimeter must show +12 to +16.8 V on
   the centre pin. Without the fuse: 0 V.

There is no reverse-polarity protection between the pack and the Jetson. Step 7 is the only check.

## Charging both packs

| Pack | Plug | How it charges on the SkyRC e450 |
|---|---|---|
| Sunpadow 4S 5200 (flight) | XT60 | e450 XT60 lead directly + balance lead |
| 4S 2200 (Jetson) | Deans | e450 XT60 lead → Deans/XT60 adapter → pack, + balance lead (JST-XH) |

One pack at a time, supervised, on the concrete floor. Store packs in the metal box. Never charge
with the Jetson lead attached. The e450 tops out at 4S, so the pack can't be charged above the
Jetson's 20 V limit.

## Connection checklist

| # | Connection | Status |
|---|---|---|
| 1–8 | Flight battery → PM02 → PDB → ESCs, Pixhawk power, PWM, GPS, TELEM1/SiK, ground SiK | ✅ Flying |
| 9 | XM+ → Pixhawk RC IN | ✅ Flying |
| 10 | Pocket power | ✅ Flying |
| 11 | TELEM2 → crossover → converter → Jetson USB-A | ⚠️ Make the crossover (2↔3, 4↔5, 6–6, pin 1 cut). Read the converter's silkscreen first. USB cable: check |
| 12 | Jetson USB 3 → D435i | ✅ Camera's own cable. Use the other USB stack from the converter |
| 13 | Jetson pack → Jetson barrel jack | 🛒 elefun + Proshop + Kjell |
| 14 | Jetson pack → buzzer | 🛒 elefun |
| 15 | Charging the Jetson pack | 🛒 elefun (adapter) |
| 16 | Jetson storage | 🛒 Proshop (SSD) |
| 17 | Mounting | 🖨️ Print the three mounts below |

## 3D-printed parts

Files and print settings in [hardware/3dprint/](../hardware/3dprint/README.md). Three mounts are
planned:

- **Jetson:** 4 × 10 mm M2.5 spacers (`jetson_spacer_10mm_x4.stl`), Jetson upside down under the
  platform board, ports to the rear. The platform board has the Jetson Nano's 58 × 86 mm holes;
  measure the Orin Nano carrier before printing.
- **Camera:** Holybro's depth-camera mount isn't in the kit, so print it from Holybro's
  [3D-print zip](https://docs.holybro.com/drone-development-kit/px4-development-kit-x500v2/download).
  The D435i uses the "B" holes: 45 mm, M3 × 2.
- **Wi-Fi antennas:** The dev kit's two PCB antennas are not glued to the plastic base. Each is held
  by a small clip; slide it outward, then lift. Keep Wi-Fi on 5 GHz or off in flight so it doesn't
  interfere with the 2.4 GHz RC link.

The XM+ pole case is also in that folder.

## Setting up the SSD

NVIDIA's [quick start](https://docs.nvidia.com/jetson/orin-nano-devkit/user-guide/latest/quick_start.html):
write the Jetson ISO to a USB stick (≥ 16 GB, e.g. with Balena Etcher), fit the SSD, boot from the
stick and pick the SSD as the install target. If the UEFI firmware is older than 36.0, do the
JetPack 6.x update path first. Press **Y** when the installer offers a QSPI update.
