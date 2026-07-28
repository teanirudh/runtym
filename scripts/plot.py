from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
RES_DIR = ROOT / "results"
OUT = ROOT / "ycsb.png"

CONTENTION = [
    ("no", "YCSB no contention"),
    ("high", "YCSB high contention"),
]

SERIES = [
    ("uniform", "uniform", "#f4a020", "*"),
    ("zipfian", "zipfian", "#4c2a85", "o"),
]


def load_res(path: Path):
    rows = []
    for line in path.read_text().splitlines():
        parts = line.split()
        if len(parts) != 2:
            continue
        lat, thr = float(parts[0]), float(parts[1])
        rows.append((thr / 1e6, lat))
    rows.sort()
    if not rows:
        return np.array([]), np.array([])
    thr, lat = zip(*rows)
    return np.array(thr), np.array(lat)


def main():
    fig, axes = plt.subplots(1, 2, figsize=(9.0, 3.8))

    for ax, (cont, title) in zip(axes, CONTENTION):
        plotted = False
        for prefix, label, color, marker in SERIES:
            path = RES_DIR / f"{prefix}_{cont}_cont.res"
            if not path.exists():
                continue
            thr, lat = load_res(path)
            if len(thr) == 0:
                continue
            ax.plot(thr, lat, color=color, marker=marker, markersize=7,
                    linewidth=1.5, label=label)
            plotted = True

        if not plotted:
            ax.text(0.5, 0.5, "no data", transform=ax.transAxes,
                    ha="center", va="center", color="gray")

        ax.set_yscale("log")
        ax.set_ylim(1, 1e6)
        ax.set_title(title)
        ax.set_xlabel("Throughput (Mrps)")
        ax.set_ylabel("Tail latency (μs)")
        ax.grid(True, which="both", linestyle="--", alpha=0.5)
        ax.tick_params(axis="y", which="both", labelleft=True)
        ax.legend(loc="upper left", frameon=False)

    fig.tight_layout(w_pad=3.0)
    fig.savefig(OUT, dpi=150, bbox_inches="tight")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
