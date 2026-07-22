#!/usr/bin/env python3
"""Generate the v1 OBD tracker carrier-board design package.

The generator keeps the mechanical dimensions, KiCad board scaffold, BOM,
placement data, and documentation in one auditable source. It intentionally
uses only the Python standard library so it can run in a fresh workspace.
"""

from __future__ import annotations

import csv
import datetime as _dt
import json
import math
import uuid
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = "obd_tracker_carrier_v1"
BOARD_W = 88.0
BOARD_H = 40.0
BOARD_T = 1.6
MOUNT_HOLES = [
    ("H1", 4.0, 4.0),
    ("H2", 84.0, 4.0),
    ("H3", 84.0, 36.0),
    ("H4", 4.0, 36.0),
]
MOUNT_DRILL = 2.7
MOUNT_COPPER = 5.6
TSIM_X = 18.0
TSIM_Y = 6.5
TSIM_W = 66.0
TSIM_H = 27.0
BAT18650_X = 8.0
BAT18650_Y = 9.5
BAT18650_W = 76.0
BAT18650_H = 21.0
NOW = _dt.datetime.now(_dt.UTC).strftime("%Y-%m-%dT%H:%M:%SZ")
NAMESPACE = uuid.UUID("3f2cfa89-842e-4e21-a614-2d527f6624c0")
PAD_COUNTER = 0


NETS = [
    "GND",
    "VIN_OBD",
    "VIN_FUSED",
    "VIN_PROT",
    "+5V",
    "+3V3",
    "VBAT",
    "CANH_OBD",
    "CANL_OBD",
    "CANH",
    "CANL",
    "CAN_TX_GPIO32",
    "CAN_RX_GPIO33",
    "I2C_SDA_GPIO21",
    "I2C_SCL_GPIO22",
    "ACCEL_INT1_GPIO34",
    "VEH_SENSE_GPIO35",
    "VBAT_SENSE_GPIO36",
    "STATUS_LED_GPIO23",
    "BOOT_DEBUG",
    "EN_DEBUG",
    "U0TX_DEBUG",
    "U0RX_DEBUG",
    "BUCK_SW",
    "BUCK_BOOT",
    "BUCK_FB",
    "BUCK_COMP",
    "BUCK_RT",
    "BUCK_EN",
    "LED_A",
    "CAN_TERM_H",
]
NET_ID = {name: i + 1 for i, name in enumerate(NETS)}


def uid(name: str) -> str:
    return str(uuid.uuid5(NAMESPACE, name))


def pad_uid() -> str:
    global PAD_COUNTER
    PAD_COUNTER += 1
    return uid(f"pad-{PAD_COUNTER}")


def net(name: str | None) -> str:
    if not name:
        return ""
    return f'(net {NET_ID[name]} "{name}")'


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def fmt(n: float) -> str:
    return f"{n:.3f}".rstrip("0").rstrip(".")


def fp_text(kind: str, text: str, x: float, y: float, layer: str, size: float = 1.0) -> str:
    return (
        f'    (fp_text {kind} "{text}" (at {fmt(x)} {fmt(y)} 0) (layer "{layer}")\n'
        f'      (effects (font (size {fmt(size)} {fmt(size)}) (thickness 0.15)))\n'
        "    )"
    )


def fp_line(x1: float, y1: float, x2: float, y2: float, layer: str, width: float = 0.12) -> str:
    return (
        f'    (fp_line (start {fmt(x1)} {fmt(y1)}) (end {fmt(x2)} {fmt(y2)})\n'
        f'      (stroke (width {fmt(width)}) (type solid)) (layer "{layer}")\n'
        f'      (uuid "{uid(f"fp-line-{x1}-{y1}-{x2}-{y2}-{layer}")}")\n'
        "    )"
    )


def footprint(ref: str, value: str, at: tuple[float, float], layer: str, body: list[str], lib: str = "OBDCarrier:Generic") -> str:
    side_silk = "B.SilkS" if layer.startswith("B.") else "F.SilkS"
    side_fab = "B.Fab" if layer.startswith("B.") else "F.Fab"
    return "\n".join(
        [
            f'  (footprint "{lib}" (layer "{layer}")',
            f'    (uuid "{uid("fp-" + ref)}")',
            f'    (at {fmt(at[0])} {fmt(at[1])})',
            fp_text("reference", ref, 0, -2.3, side_silk, 0.85),
            fp_text("value", value, 0, 2.3, side_fab, 0.75),
            *body,
            "    (embedded_fonts no)",
            "  )",
        ]
    )


def pth_pad(
    num: str,
    x: float,
    y: float,
    net_name: str | None,
    shape: str = "circle",
    size: tuple[float, float] = (1.65, 1.65),
    drill: float = 0.9,
    pinfunction: str | None = None,
) -> str:
    extra = f' {net(net_name)}' if net_name else ""
    fn = f' (pinfunction "{pinfunction}") (pintype "passive")' if pinfunction else ""
    return (
        f'    (pad "{num}" thru_hole {shape} (at {fmt(x)} {fmt(y)}) '
        f'(size {fmt(size[0])} {fmt(size[1])}) (drill {fmt(drill)}) '
        f'(layers "*.Cu" "*.Mask"){extra}{fn} (uuid "{pad_uid()}"))'
    )


def smd_pad(
    num: str,
    x: float,
    y: float,
    net_name: str | None,
    layer: str,
    shape: str = "rect",
    size: tuple[float, float] = (1.2, 1.6),
    rot: float | None = None,
    pinfunction: str | None = None,
) -> str:
    cu = "B.Cu" if layer.startswith("B.") else "F.Cu"
    paste = "B.Paste" if layer.startswith("B.") else "F.Paste"
    mask = "B.Mask" if layer.startswith("B.") else "F.Mask"
    angle = f" {fmt(rot)}" if rot is not None else ""
    extra = f' {net(net_name)}' if net_name else ""
    fn = f' (pinfunction "{pinfunction}") (pintype "passive")' if pinfunction else ""
    return (
        f'    (pad "{num}" smd {shape} (at {fmt(x)} {fmt(y)}{angle}) '
        f'(size {fmt(size[0])} {fmt(size[1])}) (layers "{cu}" "{paste}" "{mask}"){extra}{fn} (uuid "{pad_uid()}"))'
    )


def rect_body(w: float, h: float, layer: str) -> list[str]:
    return [
        fp_line(-w / 2, -h / 2, w / 2, -h / 2, layer),
        fp_line(w / 2, -h / 2, w / 2, h / 2, layer),
        fp_line(w / 2, h / 2, -w / 2, h / 2, layer),
        fp_line(-w / 2, h / 2, -w / 2, -h / 2, layer),
    ]


def make_mount(ref: str, x: float, y: float) -> str:
    body = [
        f'    (pad "" np_thru_hole circle (at 0 0) (size {fmt(MOUNT_DRILL)} {fmt(MOUNT_DRILL)}) '
        f'(drill {fmt(MOUNT_DRILL)}) (layers "*.Cu" "*.Mask") (uuid "{pad_uid()}"))',
        fp_text("user", ref, 0, -4.0, "F.SilkS", 0.8),
    ]
    return footprint(ref, "M2.5 mounting hole", (x, y), "F.Cu", body, "OBDCarrier:MountingHole_M2.5")


def make_pth_connector(ref: str, value: str, at: tuple[float, float], pins: list[tuple[str, str, str]]) -> str:
    pitch = 2.54
    start = -pitch * (len(pins) - 1) / 2
    body: list[str] = rect_body(5.5, pitch * (len(pins) - 1) + 3.2, "F.SilkS")
    for i, (num, label, net_name) in enumerate(pins):
        y = start + i * pitch
        shape = "rect" if i == 0 else "circle"
        body.append(pth_pad(num, 0, y, net_name, shape=shape, drill=1.0, pinfunction=label))
        body.append(fp_text("user", label, 3.7, y - 0.35, "F.SilkS", 0.65))
    return footprint(ref, value, at, "F.Cu", body, "OBDCarrier:Harness_4P_2.54")


def make_header(ref: str, at: tuple[float, float], pins: list[tuple[str, str | None]]) -> str:
    pitch = 2.54
    body: list[str] = rect_body(2.5, pitch * (len(pins) - 1) + 2.4, "F.SilkS")
    for i, (label, net_name) in enumerate(pins):
        y = i * pitch
        shape = "rect" if i == 0 else "circle"
        body.append(pth_pad(str(i + 1), 0, y, net_name, shape=shape, pinfunction=label))
        body.append(fp_text("user", label, 2.0, y - 0.33, "F.SilkS", 0.48))
    return footprint(ref, "T-SIM7000G low profile header rail", at, "F.Cu", body, "OBDCarrier:TSIM_Header_Rail")


def make_2pad(ref: str, value: str, at: tuple[float, float], layer: str, nets: tuple[str, str], size=(1.4, 1.8), pitch=2.4) -> str:
    silk = "B.SilkS" if layer.startswith("B.") else "F.SilkS"
    body = rect_body(pitch + 2.0, 3.0, silk)
    body.append(smd_pad("1", -pitch / 2, 0, nets[0], layer, size=size))
    body.append(smd_pad("2", pitch / 2, 0, nets[1], layer, size=size))
    return footprint(ref, value, at, layer, body, "OBDCarrier:SMD_2Pad")


def make_soic8(ref: str, value: str, at: tuple[float, float], nets_by_pin: dict[int, str | None], layer: str = "B.Cu") -> str:
    silk = "B.SilkS" if layer.startswith("B.") else "F.SilkS"
    body = rect_body(5.2, 6.2, silk)
    for i in range(4):
        y = -1.905 + i * 1.27
        body.append(smd_pad(str(i + 1), -2.6, y, nets_by_pin.get(i + 1), layer, size=(1.55, 0.6)))
        body.append(smd_pad(str(8 - i), 2.6, y, nets_by_pin.get(8 - i), layer, size=(1.55, 0.6)))
    body.append(fp_text("user", "pin1", -2.5, -3.9, silk, 0.55))
    return footprint(ref, value, at, layer, body, "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm")


def make_tps54360() -> str:
    layer = "B.Cu"
    silk = "B.SilkS"
    nets_by_pin = {
        1: "BUCK_BOOT",
        2: "VIN_PROT",
        3: "BUCK_EN",
        4: "BUCK_RT",
        5: "BUCK_FB",
        6: "BUCK_COMP",
        7: "GND",
        8: "BUCK_SW",
    }
    body = rect_body(5.3, 6.5, silk)
    for i in range(4):
        y = -1.905 + i * 1.27
        body.append(smd_pad(str(i + 1), -2.75, y, nets_by_pin[i + 1], layer, size=(1.75, 0.62)))
        body.append(smd_pad(str(8 - i), 2.75, y, nets_by_pin[8 - i], layer, size=(1.75, 0.62)))
    body.append(smd_pad("9", 0, 0, "GND", layer, size=(2.5, 3.2)))
    body.append(fp_text("user", "TPS54360 60V buck", 0, 4.0, silk, 0.55))
    return footprint("U2", "TPS54360DDAR 12V-to-5V buck", (38.5, 7.7), layer, body, "Package_SO:TI_SO-PowerPAD-8")


def make_adxl375() -> str:
    layer = "B.Cu"
    silk = "B.SilkS"
    # ADXL375 14-LGA pinout:
    # 1 VDDIO, 2 GND, 3 RESERVED, 4 GND, 5 GND, 6 VS, 7 CS,
    # 8 INT1, 9 INT2, 10 NC, 11 RESERVED, 12 SDO/ALT ADDRESS, 13 SDA, 14 SCL.
    pads = [
        ("1", -1.5, -1.5, "+3V3", "VDDIO"),
        ("2", -0.5, -1.5, "GND", "GND"),
        ("3", 0.5, -1.5, "+3V3", "RES_TO_VS"),
        ("4", 1.5, -1.5, "GND", "GND"),
        ("5", 1.5, -0.5, "GND", "GND"),
        ("6", 1.5, 0.5, "+3V3", "VS"),
        ("7", 1.5, 1.5, "+3V3", "CS_HIGH"),
        ("8", 0.5, 1.5, "ACCEL_INT1_GPIO34", "INT1"),
        ("9", -0.5, 1.5, None, "INT2_NC"),
        ("10", -1.5, 1.5, "GND", "GND"),
        ("11", -1.5, 0.5, None, "NC"),
        ("12", -1.5, -0.5, "GND", "SDO_LOW"),
        ("13", -0.5, 0, "I2C_SDA_GPIO21", "SDA"),
        ("14", 0.5, 0, "I2C_SCL_GPIO22", "SCL"),
    ]
    body = rect_body(4.2, 4.2, silk)
    for num, x, y, n, _ in pads:
        body.append(smd_pad(num, x, y, n, layer, size=(0.45, 0.45)))
    body.append(fp_text("user", "ADXL375 near H2", 0, 3.2, silk, 0.55))
    return footprint("U4", "ADXL375BCCZ-RL7", (75.5, 8.0), layer, body, "Package_LGA:ADXL375_LGA-14")


def make_jst_ph() -> str:
    body = rect_body(5.8, 4.4, "F.SilkS")
    body.append(smd_pad("1", -1.0, 0, "VBAT", "F.Cu", size=(1.0, 2.8), pinfunction="BAT+"))
    body.append(smd_pad("2", 1.0, 0, "GND", "F.Cu", size=(1.0, 2.8), pinfunction="BAT-"))
    body.append(fp_text("user", "LiPo JST-PH", 0, 3.4, "F.SilkS", 0.55))
    return footprint("J2", "JST-PH-2 LiPo, populate instead of BT1", (10.0, 32.5), "F.Cu", body, "Connector_JST:JST_PH_S2B-PH-SM4-TB")


def make_bt1() -> str:
    body = [
        fp_line(-BAT18650_W / 2, -BAT18650_H / 2, BAT18650_W / 2, -BAT18650_H / 2, "B.SilkS"),
        fp_line(BAT18650_W / 2, -BAT18650_H / 2, BAT18650_W / 2, BAT18650_H / 2, "B.SilkS"),
        fp_line(BAT18650_W / 2, BAT18650_H / 2, -BAT18650_W / 2, BAT18650_H / 2, "B.SilkS"),
        fp_line(-BAT18650_W / 2, BAT18650_H / 2, -BAT18650_W / 2, -BAT18650_H / 2, "B.SilkS"),
        pth_pad("1", -33.0, 0, "VBAT", shape="rect", size=(3.0, 3.0), drill=1.5, pinfunction="BAT+"),
        pth_pad("2", 33.0, 0, "GND", size=(3.0, 3.0), drill=1.5, pinfunction="BAT-"),
        fp_text("user", "BT1 bottom-side 18650 DNP in slim LiPo build", 0, 0, "B.SilkS", 0.75),
    ]
    return footprint("BT1", "Single 18650 holder alternate footprint", (46.0, 20.0), "B.Cu", body, "Battery:BatteryHolder_18650")


def make_can_choke() -> str:
    layer = "B.Cu"
    body = rect_body(5.0, 4.5, "B.SilkS")
    body.append(smd_pad("1", -1.7, -0.8, "CANH_OBD", layer, size=(1.2, 0.9)))
    body.append(smd_pad("2", -1.7, 0.8, "CANL_OBD", layer, size=(1.2, 0.9)))
    body.append(smd_pad("3", 1.7, -0.8, "CANH", layer, size=(1.2, 0.9)))
    body.append(smd_pad("4", 1.7, 0.8, "CANL", layer, size=(1.2, 0.9)))
    body.append(fp_text("user", "CMC or 0R links", 0, 3.1, "B.SilkS", 0.55))
    return footprint("L2", "CAN common-mode choke DNP/0R bypass", (22.5, 25.0), layer, body, "Filter:CommonModeChoke_CAN")


def make_solder_jumper_term() -> str:
    layer = "B.Cu"
    body = rect_body(3.8, 2.3, "B.SilkS")
    body.append(smd_pad("1", -0.85, 0, "CANH", layer, size=(1.1, 1.35)))
    body.append(smd_pad("2", 0.85, 0, "CAN_TERM_H", layer, size=(1.1, 1.35)))
    body.append(fp_text("user", "JP1 open: no 120R", 0, 2.2, "B.SilkS", 0.5))
    return footprint("JP1", "Optional CAN termination solder jumper, open by default", (68.5, 28.5), layer, body, "Jumper:SolderJumper_2_Open")


def make_status_led() -> list[str]:
    return [
        make_2pad("R15", "1k LED series", (74.0, 35.5), "F.Cu", ("STATUS_LED_GPIO23", "LED_A"), size=(0.8, 0.9), pitch=1.2),
        make_2pad("D5", "Green status LED", (80.0, 35.5), "F.Cu", ("LED_A", "GND"), size=(0.9, 1.0), pitch=1.4),
    ]


def make_can_esd() -> str:
    layer = "B.Cu"
    body = rect_body(2.6, 2.8, "B.SilkS")
    body.append(smd_pad("1", -0.95, 0.7, "CANH_OBD", layer, size=(0.75, 0.65), pinfunction="CANH"))
    body.append(smd_pad("2", -0.95, -0.7, "GND", layer, size=(0.75, 0.65), pinfunction="GND"))
    body.append(smd_pad("3", 0.95, 0, "CANL_OBD", layer, size=(0.75, 0.65), pinfunction="CANL"))
    body.append(fp_text("user", "CAN ESD", 0, 2.4, "B.SilkS", 0.5))
    return footprint("D4", "CAN ESD protector, SOT-23", (13.5, 25.8), layer, body, "Package_TO_SOT_SMD:SOT-23")


def make_footprints() -> list[str]:
    fps: list[str] = []
    for ref, x, y in MOUNT_HOLES:
        fps.append(make_mount(ref, x, y))

    fps.append(
        make_pth_connector(
            "J1",
            "OBD2 harness 12V/GND/CANH/CANL",
            (7.0, 20.0),
            [
                ("1", "OBD16 +12V", "VIN_OBD"),
                ("2", "OBD4 GND", "GND"),
                ("3", "OBD6 CANH", "CANH_OBD"),
                ("4", "OBD14 CANL", "CANL_OBD"),
            ],
        )
    )
    fps.append(make_jst_ph())
    fps.append(make_bt1())

    header_a = [
        ("VIN/5V", "+5V"),
        ("GND", "GND"),
        ("3V3", "+3V3"),
        ("GPIO21 SDA", "I2C_SDA_GPIO21"),
        ("GPIO22 SCL", "I2C_SCL_GPIO22"),
        ("GPIO32 CAN_TX", "CAN_TX_GPIO32"),
        ("GPIO33 CAN_RX", "CAN_RX_GPIO33"),
        ("GPIO34 INT1", "ACCEL_INT1_GPIO34"),
        ("GPIO35 VEH_ADC", "VEH_SENSE_GPIO35"),
        ("GPIO36 BAT_ADC", "VBAT_SENSE_GPIO36"),
        ("GPIO23 LED", "STATUS_LED_GPIO23"),
    ]
    header_b = [
        ("BAT+", "VBAT"),
        ("BAT-/GND", "GND"),
        ("GND", "GND"),
        ("U0TX", "U0TX_DEBUG"),
        ("U0RX", "U0RX_DEBUG"),
        ("EN", "EN_DEBUG"),
        ("BOOT", "BOOT_DEBUG"),
        ("GPIO26 NC", None),
        ("GPIO27 NC", None),
        ("GPIO2 SD NC", None),
        ("GPIO13 SD NC", None),
    ]
    fps.append(make_header("J3A", (19.2, 7.3), header_a))
    fps.append(make_header("J3B", (82.8, 7.3), header_b))

    # Input protection and buck power tree on the bottom side.
    fps.extend(
        [
            make_2pad("F1", "2A resettable fuse, 1812", (15.5, 8.2), "B.Cu", ("VIN_OBD", "VIN_FUSED"), size=(1.8, 2.8), pitch=3.6),
            make_2pad("D1", "B560C/SS56 reverse Schottky", (24.0, 8.2), "B.Cu", ("VIN_FUSED", "VIN_PROT"), size=(2.1, 2.4), pitch=4.2),
            make_2pad("D2", "SMBJ33A load-dump TVS", (15.5, 14.3), "B.Cu", ("VIN_FUSED", "GND"), size=(2.2, 2.9), pitch=4.4),
            make_2pad("C1", "10uF 50V input cap", (31.0, 5.0), "B.Cu", ("VIN_PROT", "GND"), size=(1.2, 1.4), pitch=2.0),
            make_2pad("C2", "10uF 50V input cap", (31.0, 9.0), "B.Cu", ("VIN_PROT", "GND"), size=(1.2, 1.4), pitch=2.0),
            make_tps54360(),
            make_2pad("L1", "15uH shielded inductor >=4A", (49.0, 7.7), "B.Cu", ("BUCK_SW", "+5V"), size=(2.6, 3.2), pitch=5.2),
            make_2pad("D3", "B560C buck catch diode", (45.0, 13.4), "B.Cu", ("GND", "BUCK_SW"), size=(2.1, 2.4), pitch=4.2),
            make_2pad("C3", "47uF 10V output cap", (57.0, 5.4), "B.Cu", ("+5V", "GND"), size=(1.8, 2.2), pitch=3.2),
            make_2pad("C4", "47uF 10V output cap", (57.0, 9.6), "B.Cu", ("+5V", "GND"), size=(1.8, 2.2), pitch=3.2),
            make_2pad("C5", "100nF bootstrap", (43.0, 3.4), "B.Cu", ("BUCK_BOOT", "BUCK_SW"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R5", "52.3k 1% FB top", (53.0, 13.5), "B.Cu", ("+5V", "BUCK_FB"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R6", "10.0k 1% FB bottom", (58.0, 13.5), "B.Cu", ("BUCK_FB", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R7", "200k EN top", (35.0, 13.5), "B.Cu", ("VIN_PROT", "BUCK_EN"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R8", "49.9k EN bottom", (39.5, 13.5), "B.Cu", ("BUCK_EN", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R9", "200k RT", (34.0, 3.0), "B.Cu", ("BUCK_RT", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("C6", "Compensation cap placeholder", (39.0, 3.0), "B.Cu", ("BUCK_COMP", "GND"), size=(0.75, 0.85), pitch=1.3),
        ]
    )

    fps.extend(
        [
            make_can_choke(),
            make_soic8(
                "U3",
                "TJA1051T/3 5V CAN transceiver",
                (60.5, 33.0),
                {
                    1: "CAN_TX_GPIO32",
                    2: "GND",
                    3: "+5V",
                    4: "CAN_RX_GPIO33",
                    5: "+3V3",
                    6: "CANL",
                    7: "CANH",
                    8: "GND",
                },
                "B.Cu",
            ),
            make_can_esd(),
            make_2pad("R10", "120R CAN termination DNP", (72.5, 31.5), "B.Cu", ("CAN_TERM_H", "CANL"), size=(0.8, 0.9), pitch=1.4),
            make_solder_jumper_term(),
            make_2pad("C7", "100nF CAN VCC", (64.0, 36.0), "B.Cu", ("+5V", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("C8", "100nF CAN VIO", (57.0, 36.0), "B.Cu", ("+3V3", "GND"), size=(0.75, 0.85), pitch=1.3),
        ]
    )

    fps.extend(
        [
            make_adxl375(),
            make_2pad("C9", "100nF accel decoupling", (72.0, 5.0), "B.Cu", ("+3V3", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("C10", "1uF accel decoupling", (79.0, 5.0), "B.Cu", ("+3V3", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R11", "4.7k I2C SDA pull-up", (70.0, 12.3), "B.Cu", ("I2C_SDA_GPIO21", "+3V3"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R12", "4.7k I2C SCL pull-up", (75.0, 12.3), "B.Cu", ("I2C_SCL_GPIO22", "+3V3"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R13", "100k INT1 pulldown", (80.0, 12.3), "B.Cu", ("ACCEL_INT1_GPIO34", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R1", "100k vehicle ADC top", (30.0, 34.8), "B.Cu", ("VIN_PROT", "VEH_SENSE_GPIO35"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R2", "20.0k vehicle ADC bottom", (35.0, 34.8), "B.Cu", ("VEH_SENSE_GPIO35", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("C11", "100nF vehicle ADC filter", (40.0, 34.8), "B.Cu", ("VEH_SENSE_GPIO35", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R3", "1.0M battery ADC top", (45.0, 34.8), "B.Cu", ("VBAT", "VBAT_SENSE_GPIO36"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("R4", "1.0M battery ADC bottom", (50.0, 34.8), "B.Cu", ("VBAT_SENSE_GPIO36", "GND"), size=(0.75, 0.85), pitch=1.3),
            make_2pad("C12", "100nF battery ADC filter", (55.0, 34.8), "B.Cu", ("VBAT_SENSE_GPIO36", "GND"), size=(0.75, 0.85), pitch=1.3),
        ]
    )
    fps.extend(make_status_led())
    return fps


def make_segments() -> list[str]:
    # Baseline routing scaffold. Through-hole LilyGo and OBD pads can be picked up
    # on either side, so bottom-side SMD circuitry is routed on B.Cu here.
    routes: list[tuple[str, str, float, str, list[tuple[float, float]]]] = [
        ("vin_obd", "VIN_OBD", 0.9, "B.Cu", [(7.0, 16.19), (7.0, 8.2), (13.7, 8.2)]),
        ("vin_fused_main", "VIN_FUSED", 1.2, "B.Cu", [(17.3, 8.2), (21.9, 8.2)]),
        ("vin_fused_tvs", "VIN_FUSED", 0.7, "B.Cu", [(17.3, 8.2), (17.3, 14.3), (13.3, 14.3)]),
        ("vin_prot_buck", "VIN_PROT", 1.2, "B.Cu", [(26.1, 8.2), (31.0, 8.2), (31.0, 7.065), (35.75, 7.065)]),
        ("vin_prot_c1", "VIN_PROT", 0.6, "B.Cu", [(31.0, 8.2), (30.0, 8.2), (30.0, 5.0)]),
        ("vin_prot_c2", "VIN_PROT", 0.6, "B.Cu", [(31.0, 8.2), (30.0, 8.2), (30.0, 9.0)]),
        ("vin_prot_adc", "VIN_PROT", 0.25, "B.Cu", [(31.0, 8.2), (29.35, 8.2), (29.35, 34.8)]),
        ("buck_boot", "BUCK_BOOT", 0.25, "B.Cu", [(35.75, 5.795), (42.35, 5.795), (42.35, 3.4)]),
        ("buck_en", "BUCK_EN", 0.25, "B.Cu", [(35.75, 8.335), (35.65, 8.335), (35.65, 13.5), (38.85, 13.5)]),
        ("buck_rt", "BUCK_RT", 0.25, "B.Cu", [(35.75, 9.605), (33.35, 9.605), (33.35, 3.0)]),
        ("buck_comp", "BUCK_COMP", 0.25, "B.Cu", [(41.25, 8.335), (38.35, 8.335), (38.35, 3.0)]),
        ("buck_fb", "BUCK_FB", 0.25, "B.Cu", [(41.25, 9.605), (41.25, 13.5), (53.65, 13.5), (57.35, 13.5)]),
        ("buck_sw_l1", "BUCK_SW", 1.0, "B.Cu", [(41.25, 5.795), (44.0, 5.795), (44.0, 7.7), (46.4, 7.7)]),
        ("buck_sw_catch", "BUCK_SW", 0.7, "B.Cu", [(44.0, 7.7), (47.1, 7.7), (47.1, 13.4)]),
        ("rail_5v_output", "+5V", 1.1, "B.Cu", [(51.6, 7.7), (55.4, 7.7), (55.4, 5.4)]),
        ("rail_5v_header", "+5V", 0.9, "B.Cu", [(51.6, 7.7), (30.0, 7.7), (30.0, 7.3), (19.2, 7.3)]),
        ("rail_5v_can", "+5V", 0.45, "B.Cu", [(51.6, 7.7), (58.0, 7.7), (58.0, 33.635), (57.9, 33.635)]),
        ("canh_obd_esd", "CANH_OBD", 0.25, "B.Cu", [(7.0, 21.27), (12.55, 21.27), (12.55, 26.5)]),
        ("canh_obd_choke", "CANH_OBD", 0.25, "B.Cu", [(12.55, 26.5), (20.8, 26.5), (20.8, 24.2)]),
        ("canl_obd_esd", "CANL_OBD", 0.25, "B.Cu", [(7.0, 23.81), (14.45, 23.81), (14.45, 25.8)]),
        ("canl_obd_choke", "CANL_OBD", 0.25, "B.Cu", [(14.45, 25.8), (20.8, 25.8)]),
        ("canh_bus", "CANH", 0.25, "B.Cu", [(24.2, 24.2), (40.0, 24.2), (40.0, 32.365), (63.1, 32.365)]),
        ("canl_bus", "CANL", 0.25, "B.Cu", [(24.2, 25.8), (42.0, 25.8), (42.0, 33.635), (63.1, 33.635)]),
        ("can_tx", "CAN_TX_GPIO32", 0.2, "B.Cu", [(19.2, 19.999), (52.0, 19.999), (52.0, 31.095), (57.9, 31.095)]),
        ("can_rx", "CAN_RX_GPIO33", 0.2, "B.Cu", [(19.2, 22.539), (50.0, 22.539), (50.0, 34.905), (57.9, 34.905)]),
        ("i2c_sda_pullup", "I2C_SDA_GPIO21", 0.18, "B.Cu", [(19.2, 14.919), (69.35, 14.919), (69.35, 12.3)]),
        ("i2c_sda_accel", "I2C_SDA_GPIO21", 0.18, "B.Cu", [(69.35, 12.3), (75.0, 12.3), (75.0, 8.0)]),
        ("i2c_scl_pullup", "I2C_SCL_GPIO22", 0.18, "B.Cu", [(19.2, 17.459), (74.35, 17.459), (74.35, 12.3)]),
        ("i2c_scl_accel", "I2C_SCL_GPIO22", 0.18, "B.Cu", [(74.35, 12.3), (76.0, 12.3), (76.0, 8.0)]),
        ("accel_int_pull", "ACCEL_INT1_GPIO34", 0.18, "B.Cu", [(19.2, 25.079), (79.35, 25.079), (79.35, 12.3)]),
        ("accel_int_u4", "ACCEL_INT1_GPIO34", 0.18, "B.Cu", [(79.35, 12.3), (76.0, 12.3), (76.0, 9.5)]),
        ("rail_3v3_pullups", "+3V3", 0.3, "B.Cu", [(19.2, 12.379), (70.65, 12.379), (70.65, 12.3), (75.65, 12.3)]),
        ("rail_3v3_accel_1", "+3V3", 0.25, "B.Cu", [(70.65, 12.3), (74.0, 12.3), (74.0, 6.5)]),
        ("rail_3v3_accel_3", "+3V3", 0.25, "B.Cu", [(75.65, 12.3), (76.0, 12.3), (76.0, 6.5)]),
        ("rail_3v3_accel_6_7", "+3V3", 0.25, "B.Cu", [(75.65, 12.3), (77.0, 12.3), (77.0, 8.5), (77.0, 9.5)]),
        ("rail_3v3_can_vio", "+3V3", 0.3, "B.Cu", [(19.2, 12.379), (18.0, 12.379), (18.0, 35.0), (63.1, 35.0), (63.1, 34.905)]),
        ("vehicle_adc", "VEH_SENSE_GPIO35", 0.18, "B.Cu", [(19.2, 27.619), (30.65, 27.619), (30.65, 34.8), (39.35, 34.8)]),
        ("battery_adc", "VBAT_SENSE_GPIO36", 0.18, "B.Cu", [(19.2, 30.159), (45.65, 30.159), (45.65, 34.8), (54.35, 34.8)]),
        ("vbat_jst_to_lilygo", "VBAT", 0.35, "F.Cu", [(9.0, 32.5), (9.0, 35.0), (82.8, 35.0), (82.8, 7.3)]),
        ("vbat_18650", "VBAT", 0.45, "B.Cu", [(13.0, 20.0), (13.0, 7.3), (82.8, 7.3)]),
        ("vbat_adc", "VBAT", 0.25, "B.Cu", [(82.8, 7.3), (82.8, 34.8), (44.35, 34.8)]),
        ("status_gpio", "STATUS_LED_GPIO23", 0.18, "F.Cu", [(19.2, 32.699), (74.0, 32.699), (74.0, 35.5)]),
        ("status_led", "LED_A", 0.18, "F.Cu", [(74.6, 35.5), (79.3, 35.5)]),
    ]
    out: list[str] = []
    for route_name, net_name, width, layer, points in routes:
        for i, ((x1, y1), (x2, y2)) in enumerate(zip(points, points[1:])):
            out.append(
                f'  (segment (start {fmt(x1)} {fmt(y1)}) (end {fmt(x2)} {fmt(y2)}) '
                f'(width {fmt(width)}) (layer "{layer}") (net {NET_ID[net_name]}) '
                f'(uuid "{uid(f"seg-{route_name}-{i}")}"))'
            )
    return out


def make_zones() -> str:
    pts = f"(xy 0 0) (xy {fmt(BOARD_W)} 0) (xy {fmt(BOARD_W)} {fmt(BOARD_H)}) (xy 0 {fmt(BOARD_H)})"
    return "\n".join(
        [
            f'  (zone (net {NET_ID["GND"]}) (net_name "GND") (layer "B.Cu") (uuid "{uid("gnd-zone-bcu")}") (hatch edge 0.5)',
            '    (connect_pads (clearance 0.25))',
            '    (min_thickness 0.2) (filled_areas_thickness no)',
            f'    (polygon (pts {pts}))',
            '  )',
        ]
    )


def make_kicad_pcb() -> str:
    global PAD_COUNTER
    PAD_COUNTER = 0
    nets = ["  (net 0 \"\")"] + [f'  (net {i + 1} "{n}")' for i, n in enumerate(NETS)]
    edge = [
        f'  (gr_line (start 0 0) (end {fmt(BOARD_W)} 0) (stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (uuid "{uid("edge-top")}"))',
        f'  (gr_line (start {fmt(BOARD_W)} 0) (end {fmt(BOARD_W)} {fmt(BOARD_H)}) (stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (uuid "{uid("edge-right")}"))',
        f'  (gr_line (start {fmt(BOARD_W)} {fmt(BOARD_H)}) (end 0 {fmt(BOARD_H)}) (stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (uuid "{uid("edge-bottom")}"))',
        f'  (gr_line (start 0 {fmt(BOARD_H)}) (end 0 0) (stroke (width 0.1) (type solid)) (layer "Edge.Cuts") (uuid "{uid("edge-left")}"))',
    ]
    module_box = [
        f'  (gr_rect (start {fmt(TSIM_X)} {fmt(TSIM_Y)}) (end {fmt(TSIM_X + TSIM_W)} {fmt(TSIM_Y + TSIM_H)}) '
        f'(stroke (width 0.12) (type dash)) (fill none) (layer "Dwgs.User") (uuid "{uid("tsim-envelope")}"))',
        f'  (gr_text "T-SIM7000G 66x27 envelope - verify exact rev/header before fab" '
        f'(at {fmt(TSIM_X + TSIM_W / 2)} {fmt(TSIM_Y + TSIM_H / 2)} 0) (layer "Dwgs.User") '
        f'(uuid "{uid("tsim-note")}") (effects (font (size 1.2 1.2) (thickness 0.18))))',
        f'  (gr_rect (start {fmt(BAT18650_X)} {fmt(BAT18650_Y)}) (end {fmt(BAT18650_X + BAT18650_W)} {fmt(BAT18650_Y + BAT18650_H)}) '
        f'(stroke (width 0.12) (type dash)) (fill none) (layer "B.Fab") (uuid "{uid("bat18650-envelope")}"))',
        f'  (gr_text "Bottom 18650 holder alternate; DNP for slim LiPo build" '
        f'(at {fmt(BAT18650_X + BAT18650_W / 2)} {fmt(BAT18650_Y + BAT18650_H / 2)} 0) (layer "B.Fab") '
        f'(uuid "{uid("bat18650-note")}") (effects (font (size 0.9 0.9) (thickness 0.14))))',
        f'  (gr_text "OBD tracker carrier v1 - 88.0 x 40.0 mm" '
        f'(at 44 2.6 0) (layer "F.SilkS") (uuid "{uid("title-silk")}") '
        f'(effects (font (size 1.0 1.0) (thickness 0.15))))',
    ]
    setup = """
  (setup
    (pad_to_mask_clearance 0.05)
    (allow_soldermask_bridges_in_footprints no)
    (solder_mask_min_width 0.1)
  )
""".rstrip()
    return "\n".join(
        [
            "(kicad_pcb",
            "  (version 20241229)",
            f'  (generator "{PROJECT}-generator")',
            '  (generator_version "1.0")',
            "  (general",
            "    (thickness 1.6)",
            "    (legacy_teardrops no)",
            "  )",
            '  (paper "A4")',
            "  (layers",
            '    (0 "F.Cu" signal)',
            '    (2 "B.Cu" signal)',
            '    (9 "F.Adhes" user "F.Adhesive")',
            '    (11 "B.Adhes" user "B.Adhesive")',
            '    (13 "F.Paste" user)',
            '    (15 "B.Paste" user)',
            '    (5 "F.SilkS" user "F.Silkscreen")',
            '    (7 "B.SilkS" user "B.Silkscreen")',
            '    (1 "F.Mask" user)',
            '    (3 "B.Mask" user)',
            '    (17 "Dwgs.User" user "User.Drawings")',
            '    (19 "Cmts.User" user "User.Comments")',
            '    (21 "Eco1.User" user "User.Eco1")',
            '    (23 "Eco2.User" user "User.Eco2")',
            '    (25 "Edge.Cuts" user)',
            '    (27 "Margin" user)',
            '    (31 "F.CrtYd" user "F.Courtyard")',
            '    (29 "B.CrtYd" user "B.Courtyard")',
            '    (35 "F.Fab" user)',
            '    (33 "B.Fab" user)',
            "  )",
            *nets,
            setup,
            *edge,
            *module_box,
            *make_footprints(),
            *make_segments(),
            make_zones(),
            ")",
            "",
        ]
    )


def make_kicad_pro() -> str:
    data = {
        "board": {
            "design_settings": {
                "defaults": {
                    "board_outline_line_width": 0.1,
                    "copper_line_width": 0.2,
                    "copper_text_size_h": 1.5,
                    "copper_text_size_v": 1.5,
                    "copper_text_thickness": 0.3,
                    "other_line_width": 0.15,
                    "silk_line_width": 0.15,
                    "silk_text_size_h": 1.0,
                    "silk_text_size_v": 1.0,
                    "silk_text_thickness": 0.15,
                },
                "rules": {
                    "min_clearance": 0.2,
                    "min_copper_edge_clearance": 0.25,
                    "min_hole_clearance": 0.25,
                    "min_through_hole_diameter": 0.3,
                    "min_track_width": 0.15,
                    "min_via_diameter": 0.45,
                    "min_via_drill": 0.25,
                },
                "track_widths": [0.18, 0.25, 0.8, 1.2],
                "via_dimensions": [{"diameter": 0.6, "drill": 0.3}],
            },
            "layer_presets": [],
            "viewports": [],
        },
        "boards": [],
        "cvpcb": {"equivalence_files": []},
        "erc": {"erc_exclusions": [], "meta": {"version": 0}},
        "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
        "meta": {"filename": f"{PROJECT}.kicad_pro", "version": 1},
        "net_settings": {
            "classes": [
                {
                    "bus_width": 12,
                    "clearance": 0.2,
                    "diff_pair_gap": 0.25,
                    "diff_pair_via_gap": 0.25,
                    "diff_pair_width": 0.2,
                    "line_style": 0,
                    "microvia_diameter": 0.3,
                    "microvia_drill": 0.1,
                    "name": "Default",
                    "pcb_color": "rgba(0, 0, 0, 0.000)",
                    "schematic_color": "rgba(0, 0, 0, 0.000)",
                    "track_width": 0.18,
                    "via_diameter": 0.6,
                    "via_drill": 0.3,
                    "wire_width": 6,
                },
                {
                    "bus_width": 12,
                    "clearance": 0.35,
                    "diff_pair_gap": 0.25,
                    "diff_pair_via_gap": 0.25,
                    "diff_pair_width": 0.25,
                    "line_style": 0,
                    "microvia_diameter": 0.3,
                    "microvia_drill": 0.1,
                    "name": "Power_12V_5V",
                    "pcb_color": "rgba(255, 0, 0, 0.250)",
                    "schematic_color": "rgba(255, 0, 0, 0.250)",
                    "track_width": 0.8,
                    "via_diameter": 0.8,
                    "via_drill": 0.4,
                    "wire_width": 6,
                },
                {
                    "bus_width": 12,
                    "clearance": 0.2,
                    "diff_pair_gap": 0.25,
                    "diff_pair_via_gap": 0.25,
                    "diff_pair_width": 0.25,
                    "line_style": 0,
                    "microvia_diameter": 0.3,
                    "microvia_drill": 0.1,
                    "name": "CAN",
                    "pcb_color": "rgba(0, 128, 255, 0.250)",
                    "schematic_color": "rgba(0, 128, 255, 0.250)",
                    "track_width": 0.25,
                    "via_diameter": 0.6,
                    "via_drill": 0.3,
                    "wire_width": 6,
                },
            ],
            "meta": {"version": 3},
            "net_colors": {},
            "netclass_assignments": {
                "+5V": "Power_12V_5V",
                "VIN_OBD": "Power_12V_5V",
                "VIN_FUSED": "Power_12V_5V",
                "VIN_PROT": "Power_12V_5V",
                "CANH": "CAN",
                "CANL": "CAN",
                "CANH_OBD": "CAN",
                "CANL_OBD": "CAN",
            },
        },
        "pcbnew": {"last_paths": {"gencad": "", "idf": "", "netlist": "", "specctra_dsn": "", "step": "", "vrml": ""}},
        "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
        "sheets": [],
        "text_variables": {},
    }
    return json.dumps(data, indent=2) + "\n"


def make_kicad_sch() -> str:
    # A readable top-level schematic scaffold. The CSV netlist is the detailed
    # connectivity source and the PCB carries net names on every pad.
    lines = [
        f'(kicad_sch (version 20240108) (generator "{PROJECT}-generator")',
        f'  (uuid "{uid("sch-root")}")',
        '  (paper "A3")',
        "  (title_block",
        '    (title "Vehicle GPS/CAN Tracker Carrier v1")',
        '    (company "Codex generated reference design")',
        f'    (rev "v1 Option A LilyGo T-SIM7000G carrier")',
        f'    (date "{NOW[:10]}")',
        "  )",
    ]
    notes = [
        (18, 20, "OBD2 harness: pin16 +12V, pin4 GND, pin6 CAN-H, pin14 CAN-L"),
        (18, 34, "Input protection: F1 PTC -> SMBJ33A TVS -> B560C reverse Schottky -> VIN_PROT"),
        (18, 48, "Buck: TPS54360 60V class regulator, 5V/2A target, feeds LilyGo VIN and CAN VCC"),
        (18, 62, "CAN: TJA1051T/3, TX GPIO32, RX GPIO33, no 120R termination by default"),
        (18, 76, "ADXL375: I2C GPIO21/22, addr 0x53 via CS high and SDO low, INT1 GPIO34"),
        (18, 90, "ADC: vehicle VIN_PROT divider to GPIO35, battery divider to GPIO36"),
        (18, 104, "Battery: route LilyGo charge/BAT pins to JST-PH LiPo or alternate bottom 18650 holder"),
        (18, 118, "Keep LilyGo modem UART GPIO26/27 and SD GPIO2/13/14/15 unconnected"),
        (18, 136, "Open the PCB and CSV netlist for exact pad/net assignments; verify LilyGo board revision before fab."),
    ]
    for i, (x, y, text) in enumerate(notes):
        lines.append(
            f'  (text "{text}" (at {fmt(x)} {fmt(y)} 0) '
            f'(effects (font (size 2.2 2.2)) (justify left)) (uuid "{uid(f"sch-note-{i}")}"))'
        )
    lines.append(")")
    lines.append("")
    return "\n".join(lines)


def make_legacy_sch() -> str:
    return """EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A3 16535 11693
encoding utf-8
Sheet 1 1
Title "Vehicle GPS/CAN Tracker Carrier v1"
Date "{date}"
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
""".format(date=NOW[:10])


def make_bom_rows() -> list[dict[str, str]]:
    rows = [
        ("J1", "OBD2 harness connector, 4 pos locking or screw terminal", "Molex Micro-Fit 3.0 43045-0400 preferred; KF128-2.54-4P alt", "KF128 alt C474922; Micro-Fit verify/consign", "1", "Populate one connector style. Pin order: 12V, GND, CANH, CANL."),
        ("F1", "Resettable fuse, 1812, 30V, hold 2.0A", "LUTE 1812L200/30GRT", "C49326271", "1", "Place before TVS so reverse/transient faults are current-limited."),
        ("D1", "Reverse-polarity series Schottky, 5A, 60V", "GOODWORK SS56 or equivalent", "C2848693", "1", "Schottky v1 for simplicity. PFET ideal-diode option can replace after thermal testing."),
        ("D2", "Automotive input TVS, 600W, 33V standoff", "Jingdao SMBJ33A or automotive-qualified equivalent", "C353366", "1", "Choose working voltage above jump-start/normal operation and below buck abs-max clamp margin."),
        ("U2", "60V buck regulator, 3.5A switch, SO-PowerPAD-8", "TI TPS54360DDAR", "C44377", "1", "Chosen over 28V bucks for load-dump headroom."),
        ("L1", "Shielded power inductor, 15uH, >=4A, low DCR", "Bourns SRN6045/7447719150-class", "verify footprint", "1", "Final value should be checked against TPS54360 Webench/datasheet equations."),
        ("D3", "Buck catch Schottky, 5A, 60V", "GOODWORK SS56 or equivalent", "C2848693", "1", "Required because TPS54360 is asynchronous."),
        ("C1,C2", "Input ceramic capacitor, 10uF, 50V, X7R", "0805/1206 automotive grade preferred", "JLC generic", "2", "Use two in parallel near VIN/GND of buck."),
        ("C3,C4", "Output capacitor, 47uF, 10V, X7R/polymer", "1206 ceramic or polymer low ESR", "JLC generic", "2", "Size output ripple for SIM7000G TX bursts."),
        ("C5,C6,C7,C8,C9,C10,C11,C12", "Decoupling/filter capacitors", "0603/0805 X7R", "JLC generic", "8", "Values are labelled in PCB footprints and BOM notes."),
        ("R1", "Vehicle ADC top resistor, 100k 1%", "0603", "JLC generic", "1", "With R2 scales 18V to about 3.0V."),
        ("R2", "Vehicle ADC bottom resistor, 20.0k 1%", "0603", "JLC generic", "1", "RC filter cap to ground at ADC node."),
        ("R3,R4", "Battery ADC divider, 1.0M 1%", "0603", "JLC generic", "2", "4.2V battery reads about 2.1V at GPIO36."),
        ("R5", "Buck feedback top, 52.3k 1%", "0603", "JLC generic", "1", "5.0V target with TPS54360 0.8V reference."),
        ("R6", "Buck feedback bottom, 10.0k 1%", "0603", "JLC generic", "1", "Feedback divider bottom."),
        ("R7", "Buck EN top, 200k 1%", "0603", "JLC generic", "1", "Approx 6V UVLO threshold with R8."),
        ("R8", "Buck EN bottom, 49.9k 1%", "0603", "JLC generic", "1", "Approx 6V UVLO threshold with R7."),
        ("R9", "Buck RT resistor, 200k", "0603", "JLC generic", "1", "Check final switching frequency/noise tradeoff during regulator validation."),
        ("U3", "5V high-speed CAN transceiver with 3.3V IO", "NXP TJA1051T/3/1J or MCP2562FD VIO variant", "C38695", "1", "TXD GPIO32, RXD GPIO33. S/STB tied normal mode."),
        ("D4", "CAN bus ESD protector", "Nexperia PESD1CAN,215", "C15771", "1", "Place between connector and choke/transceiver."),
        ("L2", "CAN common-mode choke, optional", "ACT45B/51MC-class", "DNP or verify", "0/1", "DNP by default or replace with 0R links for v1 testing."),
        ("R10,JP1", "Optional CAN termination", "120R 0603 plus solder jumper", "JLC generic", "DNP", "Do not populate for vehicle bus tapping."),
        ("U4", "High-g accelerometer, LGA-14", "Analog Devices ADXL375BCCZ-RL7", "C481898", "1", "Placed near H2 for good impact coupling."),
        ("R11,R12", "I2C pull-ups, 4.7k", "0603", "JLC generic", "2", "Pull to LilyGo 3.3V rail."),
        ("R13", "INT1 pulldown, 100k", "0603", "JLC generic", "1", "GPIO34 has no internal pull resistor."),
        ("D5,R15", "Status LED and 1k series resistor", "0603/0805 LED", "JLC generic", "1 set", "GPIO23 active high, optional."),
        ("J2", "LiPo battery connector, JST-PH 2.0 2-pin", "JST S2B-PH-SM4-TB(LF)(SN) or B2B-PH-K-S", "C295747 SMT or C131337 THT", "1", "Populate for slim pouch build; route to LilyGo battery/charger pins."),
        ("BT1", "Single 18650 holder footprint", "Keystone 1042/1043-class or slim 1S holder", "usually hand-assembly, verify", "DNP/1", "Bottom-side alternate. Not for the lowest-height build."),
        ("J3A,J3B", "T-SIM7000G low-profile header/solder rail", "2.54mm low-profile receptacle or direct solder pads", "JLC generic/hand assembly", "2", "Exact LilyGo board revision and header positions must be measured before fab."),
    ]
    keys = ["Reference", "Description", "Manufacturer_Part", "LCSC_JLCPCB", "Qty", "Notes"]
    return [dict(zip(keys, row)) for row in rows]


def make_netlist_rows() -> list[dict[str, str]]:
    rows = [
        ("VIN_OBD", "J1.1, F1.1", "Raw OBD pin 16 input before fuse."),
        ("VIN_FUSED", "F1.2, D2.1, D1.1", "Fused transient node."),
        ("VIN_PROT", "D1.2, U2.VIN, C1.1, C2.1, R1.1, R7.1", "Reverse-protected 12V rail feeding buck and ADC divider."),
        ("+5V", "U2 output, L1.2, C3.1, C4.1, J3A.1, U3.VCC", "5V rail for LilyGo VIN and CAN transceiver."),
        ("+3V3", "J3A.3, U3.VIO, U4.VS/VDDIO, R11.2, R12.2, C9.1, C10.1", "Use LilyGo 3.3V regulator in Option A."),
        ("GND", "J1.2, J2.2, J3A.2, J3B.2/3, U2 EP/GND, U3.GND, U4.GND, shields/returns", "Common ground."),
        ("VBAT", "J2.1, BT1.1, J3B.1, R3.1", "Battery positive routed to LilyGo charge/battery pins."),
        ("CANH_OBD", "J1.3, D4.1, L2.1", "Vehicle CAN-H before optional choke."),
        ("CANL_OBD", "J1.4, D4.2, L2.2", "Vehicle CAN-L before optional choke."),
        ("CANH", "L2.3, U3.CANH, JP1/R10", "CAN-H after optional choke."),
        ("CANL", "L2.4, U3.CANL, JP1/R10", "CAN-L after optional choke."),
        ("CAN_TX_GPIO32", "J3A.6, U3.TXD", "ESP32 TWAI TX."),
        ("CAN_RX_GPIO33", "J3A.7, U3.RXD", "ESP32 TWAI RX."),
        ("I2C_SDA_GPIO21", "J3A.4, U4.SDA, R11.1", "ADXL375 SDA."),
        ("I2C_SCL_GPIO22", "J3A.5, U4.SCL, R12.1", "ADXL375 SCL."),
        ("ACCEL_INT1_GPIO34", "J3A.8, U4.INT1, R13.1", "Wake-on-impact interrupt."),
        ("VEH_SENSE_GPIO35", "J3A.9, R1.2, R2.1, C11.1", "Vehicle voltage ADC, 100k/20k divider."),
        ("VBAT_SENSE_GPIO36", "J3A.10, R3.2, R4.1, C12.1", "Battery ADC, 1M/1M divider."),
        ("STATUS_LED_GPIO23", "J3A.11, R15.1", "Optional status LED drive."),
        ("LED_A", "R15.2, D5.1", "Status LED anode node."),
        ("CAN_TERM_H", "JP1.2, R10.1", "Optional termination node; open unless JP1 is bridged and R10 is populated."),
        ("GPIO26/GPIO27/GPIO2/GPIO13/GPIO14/GPIO15", "not connected", "Reserved for LilyGo modem UART and SD card per request."),
    ]
    keys = ["Net", "Connections", "Notes"]
    return [dict(zip(keys, row)) for row in rows]


def make_placement_rows() -> list[dict[str, str]]:
    rows = [
        ("J1", 7.0, 20.0, "Top", "OBD harness entry side"),
        ("J2", 10.0, 32.5, "Top", "JST-PH LiPo, DNP if 18650 build"),
        ("BT1", 46.0, 20.0, "Bottom", "18650 holder envelope, DNP in slim build"),
        ("J3A", 19.2, 7.3, "Top", "T-SIM left/header rail, exact rev verify"),
        ("J3B", 82.8, 7.3, "Top", "T-SIM right/header rail, exact rev verify"),
        ("U2", 38.5, 7.7, "Bottom", "Buck regulator"),
        ("U3", 60.5, 33.0, "Bottom", "CAN transceiver"),
        ("U4", 75.5, 8.0, "Bottom", "ADXL375, near H2 rigid corner"),
        ("L2", 22.5, 25.0, "Bottom", "Optional CAN common-mode choke"),
        ("D2", 15.5, 14.3, "Bottom", "Input TVS near connector"),
        ("H1", 4.0, 4.0, "Mechanical", "M2.5 2.7mm drill"),
        ("H2", 84.0, 4.0, "Mechanical", "M2.5 2.7mm drill"),
        ("H3", 84.0, 36.0, "Mechanical", "M2.5 2.7mm drill"),
        ("H4", 4.0, 36.0, "Mechanical", "M2.5 2.7mm drill"),
    ]
    return [
        {"Reference": r, "X_mm": fmt(x), "Y_mm": fmt(y), "Side": side, "Notes": notes}
        for r, x, y, side, notes in rows
    ]


def write_csv(path: Path, rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def dxf_lwpoly(points: list[tuple[float, float]], layer: str, closed: bool = True) -> str:
    out = [
        "0",
        "LWPOLYLINE",
        "100",
        "AcDbEntity",
        "8",
        layer,
        "100",
        "AcDbPolyline",
        "90",
        str(len(points)),
        "70",
        "1" if closed else "0",
    ]
    for x, y in points:
        out.extend(["10", fmt(x), "20", fmt(y)])
    return "\n".join(out)


def dxf_circle(x: float, y: float, r: float, layer: str) -> str:
    return "\n".join(["0", "CIRCLE", "100", "AcDbEntity", "8", layer, "100", "AcDbCircle", "10", fmt(x), "20", fmt(y), "30", "0", "40", fmt(r)])


def dxf_text(x: float, y: float, text: str, layer: str, height: float = 1.5) -> str:
    return "\n".join(["0", "TEXT", "100", "AcDbEntity", "8", layer, "100", "AcDbText", "10", fmt(x), "20", fmt(y), "30", "0", "40", fmt(height), "1", text])


def make_dxf() -> str:
    entities = [
        dxf_lwpoly([(0, 0), (BOARD_W, 0), (BOARD_W, BOARD_H), (0, BOARD_H)], "BOARD_OUTLINE"),
        dxf_lwpoly([(TSIM_X, TSIM_Y), (TSIM_X + TSIM_W, TSIM_Y), (TSIM_X + TSIM_W, TSIM_Y + TSIM_H), (TSIM_X, TSIM_Y + TSIM_H)], "TSIM7000G_KEEPIN"),
        dxf_lwpoly([(BAT18650_X, BAT18650_Y), (BAT18650_X + BAT18650_W, BAT18650_Y), (BAT18650_X + BAT18650_W, BAT18650_Y + BAT18650_H), (BAT18650_X, BAT18650_Y + BAT18650_H)], "BT1_18650_BOTTOM_ALT"),
        dxf_text(2, BOARD_H + 4, "OBD tracker carrier v1: 88.0 x 40.0 mm, M2.5 holes at (4,4),(84,4),(84,36),(4,36)", "NOTES", 1.8),
    ]
    for ref, x, y in MOUNT_HOLES:
        entities.append(dxf_circle(x, y, MOUNT_DRILL / 2, "MOUNT_HOLES"))
        entities.append(dxf_circle(x, y, MOUNT_COPPER / 2, "MOUNT_KEEPOUT"))
        entities.append(dxf_text(x + 2.2, y + 2.0, ref, "NOTES", 1.2))
    return "\n".join(
        [
            "0",
            "SECTION",
            "2",
            "HEADER",
            "9",
            "$INSUNITS",
            "70",
            "4",
            "0",
            "ENDSEC",
            "0",
            "SECTION",
            "2",
            "ENTITIES",
            *entities,
            "0",
            "ENDSEC",
            "0",
            "EOF",
            "",
        ]
    )


def make_step() -> str:
    # Simplified rectangular board envelope. The exact hole geometry is in the DXF.
    w, h, t = BOARD_W, BOARD_H, BOARD_T
    pts = [
        (0, 0, 0),
        (w, 0, 0),
        (w, h, 0),
        (0, h, 0),
        (0, 0, t),
        (w, 0, t),
        (w, h, t),
        (0, h, t),
    ]
    rows = [
        "ISO-10303-21;",
        "HEADER;",
        "FILE_DESCRIPTION(('Simplified PCB envelope generated by Codex'),'2;1');",
        f"FILE_NAME('{PROJECT}_board_reference.step','{NOW}',('Codex'),('Codex'),'','Codex','');",
        "FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'));",
        "ENDSEC;",
        "DATA;",
        "#1=APPLICATION_CONTEXT('configuration controlled 3d designs of mechanical parts and assemblies');",
        "#2=APPLICATION_PROTOCOL_DEFINITION('international standard','config_control_design',1994,#1);",
        "#3=PRODUCT_CONTEXT('',#1,'mechanical');",
        f"#4=PRODUCT('{PROJECT}','{PROJECT}','88 x 40 x 1.6 mm PCB envelope; holes in DXF',(#3));",
        "#5=PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('','',#4,.NOT_KNOWN.);",
        "#6=PRODUCT_DEFINITION_CONTEXT('part definition',#1,'design');",
        "#7=PRODUCT_DEFINITION('design','',#5,#6);",
        "#8=CARTESIAN_POINT('',(0.,0.,0.));",
        "#9=DIRECTION('',(0.,0.,1.));",
        "#10=DIRECTION('',(1.,0.,0.));",
        "#11=AXIS2_PLACEMENT_3D('',#8,#9,#10);",
        "#12=GEOMETRIC_REPRESENTATION_CONTEXT(3) GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#13)) GLOBAL_UNIT_ASSIGNED_CONTEXT((#14,#15,#16)) REPRESENTATION_CONTEXT('','');",
        "#13=UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-6),#14,'distance_accuracy_value','');",
        "#14=(LENGTH_UNIT() NAMED_UNIT(*) SI_UNIT(.MILLI.,.METRE.));",
        "#15=(NAMED_UNIT(*) PLANE_ANGLE_UNIT() SI_UNIT($,.RADIAN.));",
        "#16=(NAMED_UNIT(*) SI_UNIT($,.STERADIAN.) SOLID_ANGLE_UNIT());",
    ]
    for i, p in enumerate(pts, start=20):
        rows.append(f"#{i}=CARTESIAN_POINT('',({p[0]:.6f},{p[1]:.6f},{p[2]:.6f}));")
    # Minimal faceted shell using poly loops and planes. Some CAD kernels import
    # it as a surface body; use the DXF for hole cutouts if your STEP importer is strict.
    faces = [
        (20, 21, 22, 23),
        (24, 27, 26, 25),
        (20, 24, 25, 21),
        (21, 25, 26, 22),
        (22, 26, 27, 23),
        (23, 27, 24, 20),
    ]
    next_id = 40
    face_ids = []
    for face in faces:
        loop_id = next_id
        bound_id = next_id + 1
        face_id = next_id + 2
        rows.append(f"#{loop_id}=POLY_LOOP('',({','.join('#' + str(p) for p in face)}));")
        rows.append(f"#{bound_id}=FACE_OUTER_BOUND('',#{loop_id},.T.);")
        rows.append(f"#{face_id}=FACE_SURFACE('',(#{bound_id}),$,.T.);")
        face_ids.append(face_id)
        next_id += 3
    rows.append(f"#{next_id}=CLOSED_SHELL('',({','.join('#' + str(i) for i in face_ids)}));")
    rows.append(f"#{next_id + 1}=MANIFOLD_SOLID_BREP('{PROJECT}_pcb_envelope',#{next_id});")
    rows.append(f"#{next_id + 2}=SHAPE_REPRESENTATION('{PROJECT}',(#{next_id + 1}),#12);")
    rows.append(f"#{next_id + 3}=PRODUCT_DEFINITION_SHAPE('','',#7);")
    rows.append(f"#{next_id + 4}=SHAPE_DEFINITION_REPRESENTATION(#{next_id + 3},#{next_id + 2});")
    rows.append("ENDSEC;")
    rows.append("END-ISO-10303-21;")
    rows.append("")
    return "\n".join(rows)


def make_readme() -> str:
    return f"""# OBD Tracker Carrier Board v1

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

- Board: **{BOARD_W:.1f} x {BOARD_H:.1f} mm**, 1.6 mm FR-4. The KiCad source is a 2-layer v1 scaffold; a 4-layer production finalization is recommended for better EMI and thermal margin.
- Mounting holes: M2.5 clearance, {MOUNT_DRILL:.1f} mm drill, {MOUNT_COPPER:.1f} mm copper keepout.
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
- The generated routing scaffold uses bottom-layer orthogonal routes for bottom-side SMD circuitry, including CAN, TPS54360 support nets, ADXL375 I2C/INT, and ADC dividers.
- Use short, twisted OBD harness conductors for CAN-H/CAN-L and keep the harness shield/return strategy consistent with the enclosure.
- Place the ADXL375 side of the board against a rigid standoff area; avoid foam tape directly under the accelerometer if impact fidelity matters.
- Verify the exact LilyGo T-SIM7000G board revision and header geometry before fab. This reference uses the user-supplied 66 x 27 mm target envelope.
- For the lowest-height build, DNP BT1 and use a flat LiPo pouch via J2.

## Files

- `{PROJECT}.kicad_pro` - KiCad project settings and net classes.
- `{PROJECT}.kicad_pcb` - PCB outline, footprints, placement, pad nets, and baseline routing scaffold.
- `{PROJECT}.kicad_sch` - top-level schematic notes in KiCad format.
- `{PROJECT}_legacy.sch` - legacy KiCad schematic notes fallback.
- `bom/{PROJECT}_bom.csv` - BOM with suggested MPNs and LCSC/JLCPCB fields to verify at order time.
- `bom/{PROJECT}_netlist.csv` - readable netlist.
- `fab/{PROJECT}_placement.csv` - placement/mechanical coordinate table.
- `fab/{PROJECT}_stats.rpt` - KiCad-generated board statistics, created by the validation/export pass.
- `fab/{PROJECT}_erc.rpt` - KiCad schematic ERC report, created by the validation/export pass.
- `fab/VALIDATION.md` - validation commands and DRC limitation note.
- `mechanical/{PROJECT}_outline.dxf` - exact 2D board outline/hole/envelope DXF.
- `mechanical/{PROJECT}_board_reference.step` - simplified board envelope STEP.
- `mechanical/{PROJECT}_kicad_board.step` - KiCad-generated board-only STEP, created by the validation/export pass.
- `mechanical/kicad_dxf/` - KiCad-generated DXF exports for Edge.Cuts, Dwgs.User, and B.Fab.
"""


def make_design_review() -> str:
    return """# Design Review Notes

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
"""


def main() -> None:
    write(ROOT / f"{PROJECT}.kicad_pro", make_kicad_pro())
    write(ROOT / f"{PROJECT}.kicad_pcb", make_kicad_pcb())
    write(ROOT / f"{PROJECT}.kicad_sch", make_kicad_sch())
    write(ROOT / f"{PROJECT}_legacy.sch", make_legacy_sch())
    write(ROOT / "README.md", make_readme())
    write(ROOT / "DESIGN_REVIEW_NOTES.md", make_design_review())
    write_csv(ROOT / "bom" / f"{PROJECT}_bom.csv", make_bom_rows())
    write_csv(ROOT / "bom" / f"{PROJECT}_netlist.csv", make_netlist_rows())
    write_csv(ROOT / "fab" / f"{PROJECT}_placement.csv", make_placement_rows())
    write(ROOT / "mechanical" / f"{PROJECT}_outline.dxf", make_dxf())
    write(ROOT / "mechanical" / f"{PROJECT}_board_reference.step", make_step())


if __name__ == "__main__":
    main()
