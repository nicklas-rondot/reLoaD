"""Generate the eight PNG figures with NumPy, SciPy and Matplotlib."""
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.signal import welch

ROOT = Path(__file__).resolve().parent
SPEEDS = [0, 500, 1000, 1500, 2000]
COLORS = {0: "#0072B2", 500: "#CC79A7", 1000: "#D55E00", 1500: "#E69F00", 2000: "#009E73"}


def load_records():
    with (ROOT / "run_conditions.csv").open() as file:
        conditions = list(csv.DictReader(file))
    records = []
    for condition in conditions:
        name = condition["capture"]
        t, v, temperature = np.loadtxt(
            ROOT / f"{name}_matched.csv", delimiter=",", skiprows=1, unpack=True)
        assert len(t) == 20000 and np.isfinite([t, v, temperature]).all()
        assert np.allclose(t, np.arange(len(t)) / 2000, rtol=0, atol=1e-12)
        # Copper heater: 8.9 ohm at 19 °C; alpha at 20 °C = 0.00393/°C.
        resistance = 8.9 * (1 + .00393 * (temperature - 20)) / (1 - .00393)
        input_w = float(condition["transmitter_voltage_v"]) * float(condition["transmitter_current_midpoint_a"])
        eta = 100 * float(np.mean(v * v / resistance)) / input_w
        f, psd = welch(v, fs=2000, window="hann", nperseg=4096,
                       noverlap=2048, detrend="constant", scaling="density")
        records.append(dict(name=name, rpm=int(condition["rpm"]), t=t, v=v, f=f, psd=psd,
                            row={"voltage_mean_v": float(v.mean()),
                                 "transmitter_to_heater_efficiency_percent": eta}))
    assert len(records) == 15 and len({r["name"] for r in records}) == 15
    assert {r["rpm"] for r in records} == set(SPEEDS)
    return records


def save(fig, name):
    fig.savefig(ROOT / f"{name}.png", dpi=600, facecolor="white")
    plt.close(fig)


def pale(color):
    rgb = matplotlib.colors.to_rgb(color)
    return tuple(.68 + .32*x for x in rgb)


def main():
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 8,
        "axes.labelsize": 8, "axes.titlesize": 8, "legend.fontsize": 7,
        "xtick.labelsize": 7, "ytick.labelsize": 7, "axes.spines.top": False,
        "axes.spines.right": False, "axes.linewidth": .6})
    records = load_records()
    grouped = {rpm: [r for r in records if r["rpm"] == rpm] for rpm in SPEEDS}
    assert [len(grouped[rpm]) for rpm in SPEEDS] == [3, 3, 3, 3, 3]
    stationary_eta = float(np.mean([r["row"]["transmitter_to_heater_efficiency_percent"] for r in grouped[0]]))
    group_rows = []
    for rpm in SPEEDS:
        eta = np.array([r["row"]["transmitter_to_heater_efficiency_percent"] for r in grouped[rpm]])
        row = {}
        for field, values in [("heater_efficiency_percent", eta),
                              ("relative_efficiency_percent", 100 * eta / stationary_eta)]:
            row[field] = float(values.mean())
            row[field + "_sd_between_recordings"] = float(values.std(ddof=1))
        group_rows.append(row)

    limits = (min(r["v"].min() for r in records)-.1, max(r["v"].max() for r in records)+.1)
    for rpm in SPEEDS:
        runs = grouped[rpm]
        n = len(runs)
        fig, axes = plt.subplots(n, 2, figsize=(180/25.4, (52*n+20)/25.4), squeeze=False)
        fig.subplots_adjust(left=.10, right=.98, bottom=.085 if n == 3 else .12,
                            top=.92 if n == 3 else .89, hspace=.55, wspace=.32)
        label = "Stationary" if rpm == 0 else f"{rpm} RPM"
        fig.suptitle(f"{label} (n = {n} recordings)", fontsize=11, y=.985)
        for record, pair in zip(runs, axes):
            for ax, interval in zip(pair, [(0, 10), (5, 5.3)]):
                use = (record["t"] >= interval[0]) & (record["t"] <= interval[1])
                ax.plot(record["t"][use], record["v"][use], color=COLORS[rpm],
                        linewidth=.4 if interval[1] == 10 else .7)
                ax.axhline(record["row"]["voltage_mean_v"], color="#666666", linewidth=.5, linestyle="--")
                ax.set(xlim=interval, ylim=limits, ylabel="Rectifier voltage (V)",
                       title=f'{record["name"]} - '+("full 10 s" if interval[1] == 10 else "300 ms detail"))
                ax.grid(color="#E6E6E6", linewidth=.4)
        for ax in axes[-1]: ax.set_xlabel("Nominal time from capture start (s)")
        save(fig, f"voltage_{rpm:04d}rpm")
    fig, ax = plt.subplots(figsize=(180/25.4, 95/25.4))
    fig.subplots_adjust(left=.12, right=.98, bottom=.18, top=.80)
    for rpm in SPEEDS:
        runs = grouped[rpm]
        for record in runs:
            use = (record["f"] > 0) & (record["f"] <= 250)
            ax.semilogy(record["f"][use], record["psd"][use], color=pale(COLORS[rpm]), linewidth=.35)
        mean_psd = np.mean([r["psd"] for r in runs], axis=0)
        label = "Stationary" if rpm == 0 else f"{rpm} RPM"
        ax.semilogy(runs[0]["f"][use], mean_psd[use], color=COLORS[rpm], linewidth=.9,
                    label=label)
    ax.set(xlim=(0, 250), xlabel="Frequency (Hz)", ylabel="Voltage PSD (V²/Hz)")
    ax.legend(loc="lower center", bbox_to_anchor=(.5, 1.02), ncol=3, frameon=False)
    ax.grid(color="#E6E6E6", linewidth=.4)
    save(fig, "voltage_psd_all_speeds")
    for relative in [True, False]:
        field = "relative_efficiency_percent" if relative else "heater_efficiency_percent"
        y = np.array([r[field] for r in group_rows])
        sd = np.array([r[field+"_sd_between_recordings"] for r in group_rows])
        fig, ax = plt.subplots(figsize=(155/25.4, 100/25.4))
        fig.subplots_adjust(left=.16, right=.98, bottom=.28, top=.90)
        ax.plot(SPEEDS, y, color="#666666", linestyle="--", linewidth=.7)
        for rpm, value, spread in zip(SPEEDS, y, sd):
            ax.errorbar(rpm, value, yerr=spread, fmt="o", color=COLORS[rpm],
                        markersize=4, capsize=4, elinewidth=1, capthick=1)
            ax.annotate(f'{value:.2f}%', (rpm, value+spread),
                        xytext=(0, 7), textcoords="offset points", ha="center", fontsize=7)
        if relative: ax.axhline(100, color="#999999", linewidth=.5)
        ax.set(xlim=(-160, 2160), ylim=(float(min(y-sd)-.6), float(max(y+sd)+1.1)),
               xticks=SPEEDS, xlabel="Rotation speed (RPM)",
               ylabel="Relative heater-load efficiency (%)" if relative else "Transmitter-to-heater efficiency (%)")
        ax.grid(color="#E6E6E6", linewidth=.4)
        note = "Mean ± sample SD across recordings; copper R(T) correction\n"
        note += "Reference: mean stationary efficiency = 100%" if relative else "Input: 12 V × per-run reported current (range midpoint)"
        fig.text(.16, .045, note, fontsize=7, va="bottom")
        save(fig, "relative_heater_load_efficiency" if relative else "transmitter_to_heater_efficiency")


if __name__ == "__main__":
    main()
