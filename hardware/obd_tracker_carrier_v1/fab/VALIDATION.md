# Validation

Validated on KiCad CLI 10.0.3 from `/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli`.

- `pcb upgrade --force` succeeded on `obd_tracker_carrier_v1.kicad_pcb`.
- `pcb export stats` succeeded and wrote `obd_tracker_carrier_v1_stats.rpt`.
- `pcb export pos` succeeded and wrote `obd_tracker_carrier_v1_kicad_pos.csv`.
- `pcb export dxf` succeeded and wrote the files in `mechanical/kicad_dxf/`.
- `pcb export step --board-only --no-components` succeeded and wrote `mechanical/obd_tracker_carrier_v1_kicad_board.step`.
- `sch erc` succeeded with 0 violations and wrote `obd_tracker_carrier_v1_erc.rpt`.
- Custom generated-file checks confirmed 0 wrong-layer SMD route contacts across
  83 segment-to-pad contacts.
- TPS54360 pad nets checked after correction: pin 5 = `BUCK_FB`, pin 7 = `GND`.
- ADXL375 pad nets checked after correction: pin 1/3/6/7 = `+3V3`, pin 8 =
  `ACCEL_INT1_GPIO34`, pin 12 = `GND`, pin 13 = `I2C_SDA_GPIO21`, pin 14 =
  `I2C_SCL_GPIO22`.

`pcb drc` loads the board but aborts in this local KiCad 10.0.3 CLI run before
writing a report. Treat this package as a v1 reference layout/scaffold and run a
fresh PCB DRC in the KiCad GUI after binding exact vendor footprints, tightening
the buck regulator layout, and completing final routing before fabrication.
