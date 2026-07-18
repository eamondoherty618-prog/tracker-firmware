# OBD Tracker Carrier Board v1

This package is a v1 KiCad-oriented carrier design for the proven LilyGo
T-SIM7000G + CAN + ADXL375 vehicle tracker architecture.

## Architecture Decision

Use **Option A** for v1: a carrier board for the proven LilyGo T-SIM7000G module.
The LilyGo already integrates the ESP32-WROVER, SIM7000G, USB flashing, SIM,
GNSS/LTE RF, battery charging, and antennas. Keeping it as a replaceable module
cuts RF/layout risk and small-batch cost. A fully native ESP32/SIM7000G board is
the right v2 only after fleet field data proves the enclosure, power section,
and harness approach.

## Board Geometry

- Board: **88.0 x 40.0 mm**, 1.6 mm FR-4. The KiCad source is a 2-layer v1 scaffold; a 4-layer production finalization is recommended for better EMI and thermal margin.
- Mounting holes: M2.5 clearance, 2.7 mm drill, 5.6 mm copper keepout.
- Hole centers:
  - H1: (4.0, 4.0) mm
  - H2: (84.0, 4.0) mm
  - H3: (84.0, 36.0) mm
  - H4: (4.0, 36.0) mm
- LilyGo module envelope: 66.0 x 27.0 mm at lower-left coordinate (18.0, 6.5) mm.
- Optional bottom 18650 holder envelope: 76.0 x 21.0 mm at lower-left coordinate (8.0, 9.5) mm.

The DXF contains the exact outline, hole centers, LilyGo envelope, and 18650
alternate footprint. The STEP file is a simplified board envelope for enclosure
blocking; use the DXF for exact hole cutouts.

## Functional Nets

- OBD2 input: J1 pin 1 = OBD16 +12V, pin 2 = OBD4 GND, pin 3 = OBD6 CAN-H, pin 4 = OBD14 CAN-L.
- CAN transceiver: TJA1051T/3 or equivalent, TXD -> GPIO32, RXD -> GPIO33, VCC = 5V, VIO = 3.3V.
- ADXL375: SDA -> GPIO21, SCL -> GPIO22, INT1 -> GPIO34, CS high, SDO low for address 0x53.
- Vehicle ADC: protected VIN through 100k/20k divider and 100nF filter to GPIO35.
- Battery ADC: VBAT through 1M/1M divider and 100nF filter to GPIO36.
- Status LED: GPIO23 active high, optional.
- Reserved LilyGo pins: GPIO26/27 modem UART and GPIO2/13/14/15 SD remain unconnected.

## Power Section

The v1 power path is:

`OBD +12V -> F1 resettable fuse -> D2 SMBJ33A TVS -> D1 5A reverse Schottky -> TPS54360 60V buck -> +5V`

The +5V rail feeds the LilyGo VIN/charger path and the 5V CAN transceiver. The
carrier uses the LilyGo's onboard 3.3V regulator and charge circuit. Battery
support is either J2 JST-PH LiPo or BT1 bottom-side 18650 holder, populated per
build, routed to the LilyGo battery/charger pins.

## Build Notes

- DNP R10/JP1 for normal vehicle installs. Only populate 120 ohm termination for bench setups or isolated test harnesses.
- L2 CAN common-mode choke is optional. Populate a CAN choke or 0-ohm bypass links after EMI testing.
- Use short, twisted OBD harness conductors for CAN-H/CAN-L and keep the harness shield/return strategy consistent with the enclosure.
- Place the ADXL375 side of the board against a rigid standoff area; avoid foam tape directly under the accelerometer if impact fidelity matters.
- Verify the exact LilyGo T-SIM7000G board revision and header geometry before fab. This reference uses the user-supplied 66 x 27 mm target envelope.
- For the lowest-height build, DNP BT1 and use a flat LiPo pouch via J2.

## Files

- `obd_tracker_carrier_v1.kicad_pro` - KiCad project settings and net classes.
- `obd_tracker_carrier_v1.kicad_pcb` - PCB outline, footprints, placement, pad nets, and baseline routing scaffold.
- `obd_tracker_carrier_v1.kicad_sch` - top-level schematic notes in KiCad format.
- `obd_tracker_carrier_v1_legacy.sch` - legacy KiCad schematic notes fallback.
- `bom/obd_tracker_carrier_v1_bom.csv` - BOM with suggested MPNs and LCSC/JLCPCB fields to verify at order time.
- `bom/obd_tracker_carrier_v1_netlist.csv` - readable netlist.
- `fab/obd_tracker_carrier_v1_placement.csv` - placement/mechanical coordinate table.
- `mechanical/obd_tracker_carrier_v1_outline.dxf` - exact 2D board outline/hole/envelope DXF.
- `mechanical/obd_tracker_carrier_v1_board_reference.step` - simplified board envelope STEP.
