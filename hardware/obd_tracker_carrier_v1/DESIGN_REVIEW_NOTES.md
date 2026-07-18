# Design Review Notes

## Automotive Robustness

- The regulator is intentionally a 60V-class TPS54360 design instead of a cheaper
  24-28V buck module. This gives real headroom for load-dump-like pulses when
  paired with the fuse and SMBJ33A TVS.
- A Schottky reverse-protection diode is used for v1 simplicity and low sourcing
  risk. If bench thermal testing shows too much dissipation during long cellular
  upload bursts, replace D1 with an automotive high-side ideal-diode PFET circuit.
- The v1 buck should be validated with an electronic load using at least 2A pulses
  on +5V and with cranking sag profiles. The Li-ion backup should bridge crank
  intervals where the 5V buck falls out of regulation.

## Layout Risks To Verify In KiCad

- Run KiCad DRC after binding the exact vendor footprints.
- Review the TPS54360 compensation and switching loop against the TI datasheet.
- Keep the buck hot loop small: U2 VIN/GND, D3, and input capacitors should be
  tightened during final layout pass.
- Confirm the LilyGo module's USB-C remains accessible through the enclosure
  service opening or removable lid.
- Confirm ADXL375 orientation against firmware before using axis data for anything
  beyond magnitude-based impact detection.

## Fabrication Defaults

- KiCad source is a 2-layer v1 scaffold so it opens cleanly in KiCad 10. For
  production, convert to 4 layers: F.Cu signal/power, In1 GND plane, In2 +5V/VBAT
  pours, B.Cu signal/power.
- 1 oz outer copper is acceptable for a bench v1, but 2 oz outer copper improves reverse
  diode and buck thermal margin.
- ENIG finish preferred for corrosion resistance in a sealed but vehicle-mounted enclosure.
