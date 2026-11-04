import io, sys

COLS = "ABCDEFGHIJKLMN"
NCOLS, NROWS = 14, 20
P = 20
X0, Y0 = 200, 70

def col(c): return COLS.index(c)
def parse(h):
    return col(h[0]), int(h[1:]) - 1

# Pico pins: left column C rows 1..20 = pins 1..20; right column J rows 1..20 = pins 40..21
PICO_L, PICO_R = "C", "J"

# top-side components: (kind, holes, label, ramp)
COMPONENTS = [
    ("resistor", ["A11", "A14"], "10Ω", "teal"),
    ("resistor", ["A15", "A18"], "10Ω", "teal"),
    ("regulator", ["L6", "M6", "N6"], "BA50DD0T", "coral"),
    ("cap", ["L3", "L1"], "10µF", "coral"),
    ("cap", ["L7", "L8"], "0.1", "coral"),
    ("cap", ["N7", "N8"], "0.1", "coral"),
    ("cap", ["N3", "N1"], "100µF", "coral"),
]
# external wire landings on the top side: hole -> ramp
LANDINGS = {
    "A10": "teal", "A19": "teal",
    "L4": "purple", "M4": "teal",
    "L10": "purple", "L12": "purple", "L13": "purple", "L14": "purple", "L15": "purple", "L16": "purple",
    "N17": "teal", "N18": "teal", "N19": "teal",
}
# used Pico pins -> ramp
PICO_USED = {
    "C13": "teal", "C14": "teal", "C15": "teal",
    "J2": "red", "J3": "teal", "J4": "gray", "J5": "gray",
    "J10": "purple", "J12": "purple", "J13": "purple",
    "J14": "purple", "J15": "purple", "J16": "purple",
}

# underside links: (holes path, kind) kind = bus | rail | link | ins
LINKS = [
    (["K1", "N1"], "rail"),           # ground bus, top
    (["K1", "K9"], "rail"),           # ground spur
    (["K9", "N9"], "rail"),           # ground bus, under the power section
    (["N9", "N20"], "rail"),          # ground spur down the edge
    (["J3", "K3"], "link"),           # pin 38 -> ground
    (["L3", "L6"], "bus"),            # IN link: 10uF+, 7.6V landing, leg 1
    (["N3", "N6"], "bus"),            # OUT link: 100uF+, DC +5V, VSYS, leg 3
    (["L7", "L6"], "link"), (["L8", "L9"], "link"),
    (["N7", "N6"], "link"), (["N8", "N9"], "link"),
    (["M6", "M9"], "link"),           # leg 2 -> ground
    (["M4", "N4"], "link"),           # DC +5V -> OUT
    (["J2", "N2", "N3"], "ins"),      # VSYS, insulated
    (["L10", "J10"], "link"), (["L12", "J12"], "link"), (["L13", "J13"], "link"),
    (["L14", "J14"], "link"), (["L15", "J15"], "link"), (["L16", "J16"], "link"),
    (["A10", "A11"], "link"), (["A14", "C14"], "link"), (["A15", "C15"], "link"), (["A18", "A19"], "link"),
]

TOP_LABELS = [
    ("A10", "A10 · DC data A → pin 14", "L"),
    ("A19", "A19 · DC data B → pin 15", "L"),
    ("L1", "L1, N1 · electrolytic − legs", "R"),
    ("L3", "L3, N3 · electrolytic + legs", "R"),
    ("L4", "L4 · PSX 7.6V → IN", "R"),
    ("M4", "M4 · DC +5V → OUT", "R"),
    ("L6", "L M N 6 · regulator legs 1 2 3", "R"),
    ("N7", "L7/8, N7/8 · 0.1µF · face → row 20", "R"),
    ("L10", "L10 · PSX ACK → pin 31", "R"),
    ("L11", "L11 · empty", "R"),
    ("L12", "L12 · PSX CLK → pin 29", "R"),
    ("L13", "L13 · PSX GND → pin 28", "R"),
    ("L14", "L14 · PSX ATT → pin 27", "R"),
    ("L15", "L15 · PSX CMD → pin 26", "R"),
    ("L16", "L16 · PSX DATA → pin 25", "R"),
    ("N18", "N17–19 · DC ground, sense, shield", "R"),
]
BOT_LABELS = [
    ("A10", "A10→A11, A18→A19 · DC wire ends", "L"),
    ("A14", "A14→C14, A15→C15 · to pins 14, 15", "L"),
    ("K1", "K1–N1 · ground bus (bare)", "R"),
    ("N2", "J2→N2→N3 · VSYS, insulated", "R"),
    ("J3", "J3–K3 · pin 38 → ground", "R"),
    ("L4", "L3–L6 · IN link (bare)", "R"),
    ("N5", "N3–N6 · OUT link (bare)", "R"),
    ("K6", "K1–K9 · ground spur (bare)", "R"),
    ("M8", "M6–M9 · leg 2 → ground", "R"),
    ("K9", "K9–N9 · ground bus (bare)", "R"),
    ("L13", "L→J ×6 · PSX wire ends", "R"),
    ("N15", "N9–N20 · ground spur (bare)", "R"),
]

RAMP = {
    "light": {
        "gray":   ("#F1EFE8", "#888780", "#444441", "#5F5E5A"),
        "teal":   ("#E1F5EE", "#0F6E56", "#085041", "#0F6E56"),
        "purple": ("#EEEDFE", "#534AB7", "#3C3489", "#534AB7"),
        "coral":  ("#FAECE7", "#993C1D", "#712B13", "#993C1D"),
        "red":    ("#FCEBEB", "#A32D2D", "#791F1F", "#A32D2D"),
        "board":  ("#EAF0F8", "#7F94B3"), "hole": ("#ffffff", "#B4B2A9"),
        "bg": "#ffffff", "text": "#444441", "muted": "#888780",
        "bus": "#993C1D", "rail": "#5F5E5A", "link": "#378ADD", "ins": "#D4537E",
    },
    "dark": {
        "gray":   ("#444441", "#B4B2A9", "#F1EFE8", "#B4B2A9"),
        "teal":   ("#085041", "#5DCAA5", "#E1F5EE", "#9FE1CB"),
        "purple": ("#3C3489", "#AFA9EC", "#EEEDFE", "#CECBF6"),
        "coral":  ("#712B13", "#F0997B", "#FAECE7", "#F5C4B3"),
        "red":    ("#791F1F", "#F09595", "#FCEBEB", "#F7C1C1"),
        "board":  ("#22303F", "#5A7396"), "hole": ("#2C2C2A", "#6b6a65"),
        "bg": "#1b1b1a", "text": "#F1EFE8", "muted": "#B4B2A9",
        "bus": "#F0997B", "rail": "#B4B2A9", "link": "#85B7EB", "ins": "#ED93B1",
    },
}

def build(view, theme):
    mirror = (view == "bottom")
    def xy(h):
        c, r = parse(h)
        cc = (NCOLS - 1 - c) if mirror else c
        return X0 + cc * P, Y0 + r * P
    widget = (theme == "widget")
    t = RAMP["light" if widget else theme]
    out = []
    W, H = 680, 520
    def cls(ramp): return f'class="c-{ramp}"' if widget else f'class="{ramp}"'

    title = "Universal board, component side" if view == "top" else "Universal board, solder side (mirrored)"
    if widget:
        out.append(f'<svg width="100%" viewBox="0 0 {W} {H}" role="img"><title>{title}</title><desc>Layout of the 14 by 20 hole universal board for the Dreamcast to PlayStation adapter.</desc>')
        out.append('<defs><marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse"><path d="M2 1L8 5L2 9" fill="none" stroke="context-stroke" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></marker></defs>')
    else:
        out.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img"><title>{title}</title>')
        style = ['text { font-family: "Segoe UI", "Helvetica Neue", Helvetica, Arial, sans-serif; }',
                 '.th { font-size: 14px; font-weight: 600; } .ts { font-size: 12px; font-weight: 400; } .tt { font-size: 11px; font-weight: 400; }',
                 f'.leader {{ stroke: {t["muted"]}; stroke-width: 1; stroke-dasharray: 3 3; fill: none; }}',
                 f'.axis {{ fill: {t["muted"]}; }} .t {{ fill: {t["text"]}; }}']
        for r in ("gray", "teal", "purple", "coral", "red"):
            f, s, ti, su = t[r]
            style.append(f'.{r} rect, .{r} circle {{ fill: {f}; stroke: {s}; stroke-width: 1; }} .{r} .th, .{r} .t {{ fill: {ti}; }} .{r} .ts, .{r} .tt {{ fill: {su}; }}')
        out.append("<style>\n" + "\n".join(style) + "\n</style>")
        out.append(f'<rect x="0" y="0" width="{W}" height="{H}" fill="{t["bg"]}"/>')

    # board
    bx, by = X0 - 15, Y0 - 15
    bw, bh = (NCOLS - 1) * P + 30, (NROWS - 1) * P + 30
    bf, bs = t["board"]
    if widget:
        out.append(f'<rect class="box" x="{bx}" y="{by}" width="{bw}" height="{bh}" rx="6"/>')
    else:
        out.append(f'<rect x="{bx}" y="{by}" width="{bw}" height="{bh}" rx="6" fill="{bf}" stroke="{bs}" stroke-width="1"/>')
    # corner mounting holes
    for cx, cy in ((bx + 8, by + 8), (bx + bw - 8, by + 8), (bx + 8, by + bh - 8), (bx + bw - 8, by + bh - 8)):
        out.append(f'<circle cx="{cx}" cy="{cy}" r="4" fill="{t["bg"]}" stroke="{bs}" stroke-width="1"/>')
    # axes
    axis = 'class="ts"' if widget else 'class="tt axis"'
    for c in range(NCOLS):
        cc = (NCOLS - 1 - c) if mirror else c
        out.append(f'<text {axis} x="{X0 + cc * P}" y="{Y0 - 24}" text-anchor="middle">{COLS[c]}</text>')
    for r in range(NROWS):
        out.append(f'<text {axis} x="{X0 - 26}" y="{Y0 + r * P + 4}" text-anchor="end">{r + 1}</text>')
    # holes
    hf, hs = t["hole"]
    for c in range(NCOLS):
        for r in range(NROWS):
            x, y = xy(COLS[c] + str(r + 1))
            out.append(f'<circle cx="{x}" cy="{y}" r="2.6" fill="{hf}" stroke="{hs}" stroke-width="0.8"/>')

    # Pico outline (both views; on the bottom it is what is behind the board)
    pl, pr = xy(PICO_L + "1")[0], xy(PICO_R + "1")[0]
    px0, px1 = min(pl, pr) - 12, max(pl, pr) + 12
    py0, py1 = Y0 - 12, Y0 + 19 * P + 12
    dash = ' stroke-dasharray="5 4"' if mirror else ''
    out.append(f'<rect x="{px0}" y="{py0}" width="{px1 - px0}" height="{py1 - py0}" rx="6" fill="none" stroke="{t["gray"][1]}" stroke-width="1"{dash}/>')
    usb_x = (px0 + px1) / 2
    out.append(f'<rect x="{usb_x - 18}" y="{py0 - 10}" width="36" height="16" rx="2" fill="{t["gray"][0]}" stroke="{t["gray"][1]}" stroke-width="1"/>')
    lab = 'class="ts"' if widget else 'class="tt t"'
    out.append(f'<text {lab} x="{usb_x}" y="{py0 + 42}" text-anchor="middle">Pico</text>')
    out.append(f'<text {lab} x="{usb_x}" y="{py0 + 56}" text-anchor="middle">{"seen through" if mirror else "USB at top"}</text>')

    if view == "top":
        # components
        for kind, holes, label, ramp in COMPONENTS:
            pts = [xy(h) for h in holes]
            if kind == "header":
                xs, ys = [p[0] for p in pts], [p[1] for p in pts]
                x, y = min(xs) - 8, min(ys) - 8
                out.append(f'<g {cls(ramp)}><rect x="{x}" y="{y}" width="{max(xs) - min(xs) + 16}" height="{max(ys) - min(ys) + 16}" rx="3"/></g>')
            elif kind == "resistor":
                (x1, y1), (x2, y2) = pts
                out.append(f'<g {cls(ramp)}><rect x="{x1 - 6}" y="{y1 + 10}" width="12" height="{y2 - y1 - 20}" rx="3"/></g>')
            elif kind == "regulator":
                xs = [p[0] for p in pts]; y = pts[0][1]
                cx = sum(xs) / 3
                out.append(f'<g {cls(ramp)}><rect x="{cx - 44}" y="{y - 24}" width="88" height="16" rx="3"/><text class="ts" x="{cx}" y="{y - 12}" text-anchor="middle">{label}</text></g>')
                for (x, _), n in zip(pts, ("1", "2", "3")):
                    out.append(f'<text {lab} x="{x + 7}" y="{y + 4}">{n}</text>')
            elif kind == "cap":
                (x1, y1), (x2, y2) = pts
                cy = (y1 + y2) / 2
                r = 15 if "100" in label else (11 if "10" in label else 7)
                if r > 7 and y1 > y2:
                    y1, y2 = y2, y1
                out.append(f'<g {cls(ramp)}><circle cx="{x1}" cy="{cy}" r="{r}"/></g>')
                if r > 7:
                    out.append(f'<text {lab} x="{x1}" y="{cy + r - 3}" text-anchor="middle">+</text>')
                    out.append(f'<text {lab} x="{x1}" y="{cy - 1}" text-anchor="middle">{label}</text>')
        # landings
        for h, ramp in LANDINGS.items():
            x, y = xy(h)
            out.append(f'<g {cls(ramp)}><circle cx="{x}" cy="{y}" r="5.5"/></g>')
    else:
        # underside links
        for path, kind in LINKS:
            pts = [xy(h) for h in path]
            color = t[kind if kind in ("bus", "rail", "ins") else "link"]
            width = {"bus": 4, "rail": 4, "link": 2.5, "ins": 2.5}[kind]
            extra = ' stroke-dasharray="6 4"' if kind == "ins" else ''
            d = " ".join(f"{x},{y}" for x, y in pts)
            out.append(f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"{extra}/>')
        # solder points that matter
        for h in ("M6",):
            x, y = xy(h)
            out.append(f'<g {cls("coral")}><circle cx="{x}" cy="{y}" r="5.5"/></g>')
        for h, ramp in LANDINGS.items():
            x, y = xy(h)
            out.append(f'<g {cls(ramp)}><circle cx="{x}" cy="{y}" r="5"/></g>')
        for h in ("N5", "K3", "K1", "K9", "N9"):
            x, y = xy(h)
            out.append(f'<g {cls("gray")}><circle cx="{x}" cy="{y}" r="5"/></g>')

    # used Pico pins on both views
    for h, ramp in PICO_USED.items():
        x, y = xy(h)
        out.append(f'<g {cls(ramp)}><circle cx="{x}" cy="{y}" r="5.5"/></g>')

    # callout labels
    labels = TOP_LABELS if view == "top" else BOT_LABELS
    left_y, right_y = 0, 0
    for h, text, side in labels:
        x, y = xy(h)
        if mirror:
            side = "R" if side == "L" else "L"
        if side == "L":
            ly = max(y, left_y + 16); left_y = ly
            tx = X0 - 40
            out.append(f'<line class="leader" x1="{x - 7}" y1="{y}" x2="{tx + 4}" y2="{ly}"/>')
            out.append(f'<text {"class=\"ts\"" if widget else "class=\"tt t\""} x="{tx}" y="{ly + 4}" text-anchor="end">{text}</text>')
        else:
            ly = max(y, right_y + 16); right_y = ly
            tx = X0 + (NCOLS - 1) * P + 30
            out.append(f'<line class="leader" x1="{x + 7}" y1="{y}" x2="{tx - 4}" y2="{ly}"/>')
            out.append(f'<text {"class=\"ts\"" if widget else "class=\"tt t\""} x="{tx}" y="{ly + 4}">{text}</text>')

    # legend
    ly = Y0 + (NROWS - 1) * P + 40
    if view == "top":
        items = [("teal", "Dreamcast"), ("purple", "PlayStation"), ("coral", "power parts"), ("gray", "serial, reserved"), ("red", "pin 39, 5V")]
    else:
        items = [("bus", "buses, bare wire"), ("rail", "ground, bare wire"), ("link", "short links, bare"), ("ins", "insulated wire")]
    lx = 40
    for key, name in items:
        if view == "top":
            out.append(f'<g {cls(key)}><rect x="{lx}" y="{ly - 9}" width="12" height="12" rx="2"/></g>')
        else:
            out.append(f'<line x1="{lx}" y1="{ly - 3}" x2="{lx + 14}" y2="{ly - 3}" stroke="{t[key]}" stroke-width="3"{" stroke-dasharray=\"5 3\"" if key == "ins" else ""}/>')
        out.append(f'<text {"class=\"ts\"" if widget else "class=\"tt t\""} x="{lx + 18}" y="{ly + 1}">{name}</text>')
        lx += 18 + 7 * len(name) + 22
    out.append("</svg>")
    return "\n".join(out)

if __name__ == "__main__":
    mode = sys.argv[1]
    if mode == "files":
        for view in ("top", "bottom"):
            for theme in ("light", "dark"):
                name = f"board-{view}" + ("-dark" if theme == "dark" else "")
                io.open(f"docs/{name}.svg", "w", encoding="utf-8", newline="\n").write(build(view, theme))
                print("wrote", name)
    else:
        sys.stdout.reconfigure(encoding="utf-8")
        print(build(mode, "widget"))
