EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A3 16535 11693
encoding utf-8
Sheet 1 1
Title "Vehicle GPS/CAN Tracker Carrier v1"
Date "2026-07-18"
Rev "v1 Option A LilyGo T-SIM7000G carrier"
Comp "Codex generated reference design"
$EndDescr
Text Notes 900 900 0 100 ~ 0
OBD2 pin16 +12V -> F1 PTC -> D2 SMBJ33A TVS -> D1 reverse Schottky -> VIN_PROT
Text Notes 900 1450 0 100 ~ 0
VIN_PROT -> TPS54360 60V buck -> +5V, 2A target. +5V feeds LilyGo VIN and TJA1051 VCC.
Text Notes 900 2000 0 100 ~ 0
TJA1051T/3: TXD=GPIO32, RXD=GPIO33, VIO=3V3, VCC=5V. No 120R termination unless JP1/R10 populated.
Text Notes 900 2550 0 100 ~ 0
ADXL375: SDA=GPIO21, SCL=GPIO22, INT1=GPIO34, CS tied high, SDO tied low for I2C address 0x53.
Text Notes 900 3100 0 100 ~ 0
GPIO35 senses protected vehicle input through 100k/20k RC divider. GPIO36 senses VBAT through 1M/1M RC divider.
Text Notes 900 3650 0 100 ~ 0
Battery build options: JST-PH LiPo OR bottom 18650 holder, populated per build. Both route to LilyGo BAT pins/charger.
Text Notes 900 4200 0 100 ~ 0
GPIO26/27 modem UART and GPIO2/13/14/15 SD pins remain unconnected on this carrier.
$EndSCHEMATC
