from __future__ import annotations

import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parent
CSV_PATH = ROOT / "5v_reg_selector_sweep.csv"
SVG_PATH = ROOT / "5v_reg_selector_sweep.svg"

WIDTH = 960
HEIGHT = 540
MARGIN_LEFT = 80
MARGIN_RIGHT = 24
MARGIN_TOP = 28
MARGIN_BOTTOM = 60


def load_points(csv_path: Path) -> tuple[list[float], list[float]]:
    xs: list[float] = []
    ys: list[float] = []

    with csv_path.open(newline="") as handle:
        reader = csv.reader(handle, delimiter=" ", skipinitialspace=True)
        for row in reader:
            values = [float(item) for item in row if item]
            if len(values) < 4:
                continue
            xs.append(values[0])
            ys.append(values[3])

    return xs, ys


def scale(value: float, source_min: float, source_max: float, target_min: float, target_max: float) -> float:
    if source_max == source_min:
        return (target_min + target_max) / 2
    return target_min + (value - source_min) * (target_max - target_min) / (source_max - source_min)


def line_points(xs: list[float], ys: list[float], x_min: float, x_max: float, y_min: float, y_max: float) -> str:
    plot_width = WIDTH - MARGIN_LEFT - MARGIN_RIGHT
    plot_height = HEIGHT - MARGIN_TOP - MARGIN_BOTTOM
    points: list[str] = []
    for x_value, y_value in zip(xs, ys):
        x = scale(x_value, x_min, x_max, MARGIN_LEFT, MARGIN_LEFT + plot_width)
        y = scale(y_value, y_min, y_max, MARGIN_TOP + plot_height, MARGIN_TOP)
        points.append(f"{x:.2f},{y:.2f}")
    return " ".join(points)


def svg_text(x: float, y: float, text: str, size: int = 14, anchor: str = "start", fill: str = "#1f2937") -> str:
    return f'<text x="{x:.1f}" y="{y:.1f}" font-family="Segoe UI, Arial, sans-serif" font-size="{size}" text-anchor="{anchor}" fill="{fill}">{text}</text>'


def main() -> None:
    xs, ys = load_points(CSV_PATH)
    if not xs:
        raise SystemExit(f"No data found in {CSV_PATH}")

    y_setpoint = ys[0]
    x_min = min(xs)
    x_max = max(xs)
    y_min = min(min(xs), min(ys), y_setpoint) - 0.1
    y_max = max(max(xs), max(ys), y_setpoint) + 0.1

    plot_width = WIDTH - MARGIN_LEFT - MARGIN_RIGHT
    plot_height = HEIGHT - MARGIN_TOP - MARGIN_BOTTOM

    axis_x = MARGIN_LEFT
    axis_y = MARGIN_TOP + plot_height

    svg_parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        svg_text(MARGIN_LEFT, 20, "5V selector crossover", size=20),
    ]

    for tick in range(5):
        x_value = x_min + (x_max - x_min) * tick / 4
        x = scale(x_value, x_min, x_max, MARGIN_LEFT, MARGIN_LEFT + plot_width)
        svg_parts.append(f'<line x1="{x:.2f}" y1="{MARGIN_TOP}" x2="{x:.2f}" y2="{MARGIN_TOP + plot_height}" stroke="#eef2f7" stroke-width="1"/>')
        svg_parts.append(svg_text(x, HEIGHT - 28, f"{x_value:.2f}", size=12, anchor="middle", fill="#475569"))

    for tick in range(5):
        y_value = y_min + (y_max - y_min) * tick / 4
        y = scale(y_value, y_min, y_max, MARGIN_TOP + plot_height, MARGIN_TOP)
        svg_parts.append(f'<line x1="{MARGIN_LEFT}" y1="{y:.2f}" x2="{MARGIN_LEFT + plot_width}" y2="{y:.2f}" stroke="#eef2f7" stroke-width="1"/>')
        svg_parts.append(svg_text(MARGIN_LEFT - 10, y + 4, f"{y_value:.2f}", size=12, anchor="end", fill="#475569"))

    # Axes
    svg_parts.append(f'<line x1="{MARGIN_LEFT}" y1="{axis_y}" x2="{MARGIN_LEFT + plot_width}" y2="{axis_y}" stroke="#111827" stroke-width="1.5"/>')
    svg_parts.append(f'<line x1="{axis_x}" y1="{MARGIN_TOP}" x2="{axis_x}" y2="{axis_y}" stroke="#111827" stroke-width="1.5"/>')

    # Reference line for y = x
    ref_points = []
    for sample in xs:
        x = scale(sample, x_min, x_max, MARGIN_LEFT, MARGIN_LEFT + plot_width)
        y = scale(sample, y_min, y_max, MARGIN_TOP + plot_height, MARGIN_TOP)
        ref_points.append(f"{x:.2f},{y:.2f}")
    svg_parts.append(f'<polyline points="{" ".join(ref_points)}" fill="none" stroke="#94a3b8" stroke-width="2" stroke-dasharray="7 5"/>')

    # 5V_reg curve
    svg_parts.append(f'<polyline points="{line_points(xs, ys, x_min, x_max, y_min, y_max)}" fill="none" stroke="#0b7285" stroke-width="3"/>')

    # labels
    svg_parts.append(svg_text(MARGIN_LEFT + plot_width - 10, MARGIN_TOP + 18, "5V_reg", size=13, anchor="end", fill="#0b7285"))
    svg_parts.append(svg_text(MARGIN_LEFT + plot_width - 10, MARGIN_TOP + 38, "VSENSE_5V+", size=13, anchor="end", fill="#64748b"))
    svg_parts.append(svg_text(WIDTH / 2, HEIGHT - 18, "VSENSE_5V+ (V)", size=14, anchor="middle", fill="#1f2937"))
    svg_parts.append(f'<g transform="translate(22,{HEIGHT / 2}) rotate(-90)">{svg_text(0, 0, "Voltage (V)", size=14, anchor="middle", fill="#1f2937")}</g>')

    # setpoint marker
    set_x = scale(y_setpoint, x_min, x_max, MARGIN_LEFT, MARGIN_LEFT + plot_width)
    set_y = scale(y_setpoint, y_min, y_max, MARGIN_TOP + plot_height, MARGIN_TOP)
    svg_parts.append(f'<line x1="{set_x:.2f}" y1="{MARGIN_TOP}" x2="{set_x:.2f}" y2="{axis_y}" stroke="#f08c00" stroke-width="1.5" stroke-dasharray="4 4"/>')
    svg_parts.append(f'<line x1="{MARGIN_LEFT}" y1="{set_y:.2f}" x2="{MARGIN_LEFT + plot_width}" y2="{set_y:.2f}" stroke="#f08c00" stroke-width="1.5" stroke-dasharray="4 4"/>')
    svg_parts.append(svg_text(set_x + 6, set_y - 6, f"Setpoint {y_setpoint:.3f} V", size=12, fill="#b45309"))

    svg_parts.append('</svg>')

    SVG_PATH.write_text("\n".join(svg_parts), encoding="utf-8")
    print(f"Saved {SVG_PATH}")


if __name__ == "__main__":
    main()
