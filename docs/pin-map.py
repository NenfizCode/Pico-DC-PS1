"""Generates docs/pico-pin-map.svg and -dark.svg.

Only pins that get a wire are drawn with a label. Connections are named by
connector pin number, never by wire colour: cable colours differ from one
cable to the next, even between the two halves of the same extension.

    python docs/pin-map.py
"""
import io

W, H = 680, 470
BODY_X, BODY_Y, BODY_W, BODY_H = 250, 40, 180, 405
LEFT_X, RIGHT_X = 263, 417
Y0, PITCH = 62, 19

# (pin, name, detail, ramp). Side is derived: 1-20 left, 21-40 right.
PINS = [
    (14, "14 · GP10", "10Ω → DC pin 1 · data A (SDCKA)", "teal"),
    (15, "15 · GP11", "10Ω → DC pin 5 · data B (SDCKB)", "teal"),
    (39, "39 · VSYS — the only 5V pin", "Regulator output · DC pin 2 (+5V)", "red"),
    (38, "38 · GND", "Common ground bus", "gray"),
    (31, "31 · GP26", "PSX pin 9 · ACK", "purple"),
    (29, "29 · GP22", "PSX pin 7 · CLK", "purple"),
    (28, "28 · GND", "PSX pin 4 · signal ground", "gray"),
    (27, "27 · GP21", "PSX pin 6 · ATT / SEL", "purple"),
    (26, "26 · GP20", "PSX pin 2 · CMD", "purple"),
    (25, "25 · GP19", "PSX pin 1 · DATA", "purple"),
]

RAMP = {
    "light": {
        "gray":   ("#F1EFE8", "#888780", "#444441", "#5F5E5A", "#D3D1C7"),
        "teal":   ("#E1F5EE", "#0F6E56", "#085041", "#0F6E56", "#5DCAA5"),
        "purple": ("#EEEDFE", "#534AB7", "#3C3489", "#534AB7", "#AFA9EC"),
        "red":    ("#FCEBEB", "#A32D2D", "#791F1F", "#A32D2D", "#F09595"),
        "bg": "#ffffff", "body": ("#F7F6F2", "#888780"),
        "hole": ("#ffffff", "#B4B2A9"), "muted": "#9a988f",
    },
    "dark": {
        "gray":   ("#444441", "#B4B2A9", "#F1EFE8", "#B4B2A9", "#6b6a65"),
        "teal":   ("#085041", "#5DCAA5", "#E1F5EE", "#9FE1CB", "#5DCAA5"),
        "purple": ("#3C3489", "#AFA9EC", "#EEEDFE", "#CECBF6", "#AFA9EC"),
        "red":    ("#791F1F", "#F09595", "#FCEBEB", "#F7C1C1", "#F09595"),
        "bg": "#1b1b1a", "body": ("#262625", "#6b6a65"),
        "hole": ("#2C2C2A", "#6b6a65"), "muted": "#6b6a65",
    },
}


def pin_xy(pin):
    if pin <= 20:
        return LEFT_X, Y0 + PITCH * (pin - 1)
    return RIGHT_X, Y0 + PITCH * (40 - pin)


def build(theme):
    t = RAMP[theme]
    used = {p[0]: p for p in PINS}
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" '
           f'viewBox="0 0 {W} {H}" role="img">',
           '<title>Pico pin map for the Dreamcast to PlayStation adapter</title>',
           '<desc>Raspberry Pi Pico drawn top-down with USB at the top. Only the '
           'pins that get a wire are labelled, each naming the connector pin it '
           'goes to. Pin 39 is the only pin that may carry 5 volts.</desc>']

    style = ['text { font-family: "Segoe UI", "Helvetica Neue", Helvetica, Arial, sans-serif; }',
             '.th { font-size: 14px; font-weight: 600; } .ts { font-size: 12px; font-weight: 400; }',
             f'.leader {{ stroke: {t["muted"]}; stroke-width: 1; stroke-dasharray: 3 3; fill: none; }}',
             f'.hole {{ fill: {t["hole"][0]}; stroke: {t["hole"][1]}; stroke-width: 1; }}']
    for r in ("gray", "teal", "purple", "red"):
        f, s, ti, su, dot = t[r]
        w = 1.5 if r == "red" else 1
        style.append(f'.{r} rect {{ fill: {f}; stroke: {s}; stroke-width: {w}; }} '
                     f'.{r} .th {{ fill: {ti}; }} .{r} .ts {{ fill: {su}; }} '
                     f'.{r} circle {{ fill: {dot}; stroke: {s}; stroke-width: 1; }}')
    out.append("<style>\n" + "\n".join(style) + "\n</style>")
    out.append(f'<rect x="0" y="0" width="{W}" height="{H}" fill="{t["bg"]}"/>')

    # board body and the USB connector
    bf, bs = t["body"]
    out.append(f'<rect x="{BODY_X}" y="{BODY_Y}" width="{BODY_W}" height="{BODY_H}" '
               f'rx="10" fill="{bf}" stroke="{bs}" stroke-width="1"/>')
    out.append(f'<g class="gray"><rect x="312" y="30" width="56" height="26" rx="3"/>'
               f'<text class="ts" x="340" y="47" text-anchor="middle">USB</text></g>')

    # every hole, so the board still reads as a Pico
    for pin in range(1, 41):
        x, y = pin_xy(pin)
        if pin not in used:
            out.append(f'<circle class="hole" cx="{x}" cy="{y}" r="2.5"/>')

    # labels, placed in pin order down each side without overlapping
    for side in ("L", "R"):
        rows = [p for p in PINS if (p[0] <= 20) == (side == "L")]
        rows.sort(key=lambda p: pin_xy(p[0])[1])
        prev = -999
        for pin, name, detail, ramp in rows:
            px, py = pin_xy(pin)
            by = max(py - 16, prev + 36)
            prev = by
            bx = 40 if side == "L" else 440
            tx = bx + 8
            out.append(f'<g class="{ramp}"><circle cx="{px}" cy="{py}" r="5"/>'
                       f'<rect x="{bx}" y="{by}" width="200" height="32" rx="4"/>'
                       f'<text class="th" x="{tx}" y="{by + 14}">{name}</text>'
                       f'<text class="ts" x="{tx}" y="{by + 27}">{detail}</text></g>')
            lx1, lx2 = (px - 5, bx + 200) if side == "L" else (px + 5, bx)
            out.append(f'<line class="leader" x1="{lx1}" y1="{py}" x2="{lx2}" y2="{by + 16}"/>')

    out.append('</svg>')
    return "\n".join(out)


if __name__ == "__main__":
    for theme in ("light", "dark"):
        name = "pico-pin-map" + ("-dark" if theme == "dark" else "")
        io.open(f"docs/{name}.svg", "w", encoding="utf-8", newline="\n").write(build(theme))
        print("wrote", name)
