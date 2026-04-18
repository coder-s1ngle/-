"""Generate evaluation plots and summary metrics for B1C primary codes."""

from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from b1c_maincode import (
    CHANNEL_TABLES,
    CHIP_RATE_HZ,
    PRIMARY_CODE_LENGTH,
    periodic_correlation,
    primary_code,
    to_bipolar,
    validate_primary_codes,
)

OUTPUT_DIR = Path("output_eval")
REPRESENTATIVE_PRN = 1
REPRESENTATIVE_PAIR = (1, 2)
TRUNCATION_LENGTHS = (256, 512, 1023, 2046, 4092, 8192, PRIMARY_CODE_LENGTH)
PAIR_CHUNK_SIZE = 96


def ensure_output_dir() -> None:
    OUTPUT_DIR.mkdir(exist_ok=True)


def safe_db(value: float) -> float | None:
    if value <= 0.0:
        return None
    return float(10.0 * np.log10(value))


def code_stats(channel: str, prn: int) -> dict[str, float | int | str]:
    chips = primary_code(prn, channel)
    ones = int(chips.sum())
    zeros = int(len(chips) - ones)
    bipolar = to_bipolar(chips)
    mean = float(np.mean(bipolar))
    return {
        "channel": channel,
        "prn": prn,
        "length": int(len(chips)),
        "ones": ones,
        "zeros": zeros,
        "balance_error": int(abs(ones - zeros)),
        "bipolar_mean": mean,
    }


@lru_cache(maxsize=None)
def channel_prns(channel: str) -> tuple[int, ...]:
    return tuple(sorted(CHANNEL_TABLES[channel]))


@lru_cache(maxsize=None)
def channel_bipolar_codes(channel: str) -> np.ndarray:
    return np.stack([primary_code(prn, channel, bipolar=True).astype(np.float64) for prn in channel_prns(channel)])


def pair_index_arrays(channel: str) -> tuple[np.ndarray, np.ndarray]:
    count = len(channel_prns(channel))
    return np.triu_indices(count, k=1)


def balance_summary(channel: str) -> dict[str, object]:
    stats = [code_stats(channel, prn) for prn in channel_prns(channel)]
    errors = np.array([item["balance_error"] for item in stats], dtype=np.int32)
    means = np.array([item["bipolar_mean"] for item in stats], dtype=np.float64)
    return {
        "channel": channel,
        "num_prns": len(stats),
        "perfect_balance_count": int(np.sum(errors == 0)),
        "max_balance_error": int(errors.max()),
        "mean_balance_error": float(errors.mean()),
        "max_abs_bipolar_mean": float(np.max(np.abs(means))),
        "representative_prn": next(item for item in stats if item["prn"] == REPRESENTATIVE_PRN),
    }


def autocorrelation_summary(channel: str) -> dict[str, object]:
    metrics: list[dict[str, float | int | str]] = []
    for prn, code in zip(channel_prns(channel), channel_bipolar_codes(channel), strict=True):
        _, corr = periodic_correlation(code)
        peak_index = int(np.argmax(corr))
        sidelobes = np.delete(corr, peak_index)
        metrics.append(
            {
                "prn": prn,
                "peak": float(np.max(corr)),
                "max_abs_sidelobe": float(np.max(np.abs(sidelobes))),
                "rms_sidelobe": float(np.sqrt(np.mean(sidelobes**2))),
            }
        )

    worst = max(metrics, key=lambda item: float(item["max_abs_sidelobe"]))
    representative = next(item for item in metrics if item["prn"] == REPRESENTATIVE_PRN)
    return {
        "channel": channel,
        "representative_prn": representative,
        "worst_case": worst,
        "mean_max_abs_sidelobe": float(np.mean([item["max_abs_sidelobe"] for item in metrics])),
        "mean_rms_sidelobe": float(np.mean([item["rms_sidelobe"] for item in metrics])),
    }


def periodic_crosscorrelation_summary(channel: str) -> dict[str, object]:
    prns = channel_prns(channel)
    codes = channel_bipolar_codes(channel)
    fft_codes = np.fft.fft(codes, axis=1)
    pair_a, pair_b = pair_index_arrays(channel)

    pair_metrics: list[dict[str, float | int | str]] = []
    for start in range(0, len(pair_a), PAIR_CHUNK_SIZE):
        stop = start + PAIR_CHUNK_SIZE
        chunk_a = pair_a[start:stop]
        chunk_b = pair_b[start:stop]
        products = fft_codes[chunk_a] * np.conj(fft_codes[chunk_b])
        corrs = np.fft.ifft(products, axis=1).real / PRIMARY_CODE_LENGTH
        maxima = np.max(np.abs(corrs), axis=1)
        rms_values = np.sqrt(np.mean(corrs**2, axis=1))
        means = np.mean(corrs, axis=1)
        for idx in range(len(chunk_a)):
            pair_metrics.append(
                {
                    "prn_a": int(prns[chunk_a[idx]]),
                    "prn_b": int(prns[chunk_b[idx]]),
                    "max_abs_crosscorr": float(maxima[idx]),
                    "rms_crosscorr": float(rms_values[idx]),
                    "mean_crosscorr": float(means[idx]),
                }
            )

    representative = next(
        item
        for item in pair_metrics
        if item["prn_a"] == REPRESENTATIVE_PAIR[0] and item["prn_b"] == REPRESENTATIVE_PAIR[1]
    )
    worst = max(pair_metrics, key=lambda item: float(item["max_abs_crosscorr"]))
    return {
        "channel": channel,
        "representative_pair": representative,
        "worst_case_pair": worst,
        "mean_pair_max_abs_crosscorr": float(np.mean([item["max_abs_crosscorr"] for item in pair_metrics])),
    }


def truncation_crosscorrelation_study(channel: str) -> list[dict[str, object]]:
    prns = channel_prns(channel)
    codes = channel_bipolar_codes(channel)
    pair_a, pair_b = pair_index_arrays(channel)
    prn_to_idx = {prn: idx for idx, prn in enumerate(prns)}
    rep_a = prn_to_idx[REPRESENTATIVE_PAIR[0]]
    rep_b = prn_to_idx[REPRESENTATIVE_PAIR[1]]

    results: list[dict[str, object]] = []
    for length in TRUNCATION_LENGTHS:
        truncated = codes[:, :length]
        nfft = 1 << (2 * length - 2).bit_length()
        fft_codes = np.fft.fft(truncated, n=nfft, axis=1)
        fft_reversed = np.fft.fft(truncated[:, ::-1], n=nfft, axis=1)
        corr_len = 2 * length - 1

        pair_metrics: list[dict[str, float | int | str]] = []
        for start in range(0, len(pair_a), PAIR_CHUNK_SIZE):
            stop = start + PAIR_CHUNK_SIZE
            chunk_a = pair_a[start:stop]
            chunk_b = pair_b[start:stop]
            products = fft_codes[chunk_a] * fft_reversed[chunk_b]
            corrs = np.fft.ifft(products, axis=1).real[:, :corr_len] / length
            maxima = np.max(np.abs(corrs), axis=1)
            for idx in range(len(chunk_a)):
                pair_metrics.append(
                    {
                        "prn_a": int(prns[chunk_a[idx]]),
                        "prn_b": int(prns[chunk_b[idx]]),
                        "max_abs_crosscorr": float(maxima[idx]),
                    }
                )

        representative_corr = np.fft.ifft(fft_codes[rep_a] * fft_reversed[rep_b]).real[:corr_len] / length
        representative_max = float(np.max(np.abs(representative_corr)))
        worst = max(pair_metrics, key=lambda item: float(item["max_abs_crosscorr"]))

        results.append(
            {
                "length": int(length),
                "representative_pair": {
                    "prn_a": REPRESENTATIVE_PAIR[0],
                    "prn_b": REPRESENTATIVE_PAIR[1],
                    "max_abs_crosscorr": representative_max,
                },
                "worst_case_pair": worst,
                "mean_pair_max_abs_crosscorr": float(np.mean([item["max_abs_crosscorr"] for item in pair_metrics])),
            }
        )

    return results


def power_spectrum(channel: str, nfft: int = 32768) -> tuple[np.ndarray, np.ndarray]:
    codes = channel_bipolar_codes(channel)
    spectra = np.fft.fftshift(np.fft.fft(codes, n=nfft, axis=1), axes=1)
    psd = np.mean(np.abs(spectra) ** 2 / PRIMARY_CODE_LENGTH, axis=0)
    psd /= np.max(psd)
    freq_hz = np.fft.fftshift(np.fft.fftfreq(nfft, d=1.0 / CHIP_RATE_HZ))
    return freq_hz, psd


def power_spectrum_summary(channel: str) -> dict[str, object]:
    freq_hz, psd = power_spectrum(channel)
    dc_index = len(psd) // 2
    quarter_index = int(np.argmin(np.abs(freq_hz - CHIP_RATE_HZ / 4.0)))
    return {
        "channel": channel,
        "fft_size": int(len(psd)),
        "dc_relative_power": float(psd[dc_index]),
        "dc_relative_power_db": safe_db(float(psd[dc_index])),
        "quarter_chip_rate_relative_power": float(psd[quarter_index]),
        "quarter_chip_rate_relative_power_db": safe_db(float(psd[quarter_index])),
    }


def plot_first_chips() -> None:
    x = np.arange(200)
    fig, axes = plt.subplots(2, 1, figsize=(10, 5.5), sharex=True)
    for axis, channel in zip(axes, ("data", "pilot"), strict=True):
        chips = primary_code(REPRESENTATIVE_PRN, channel)
        axis.step(x, to_bipolar(chips[:200]), where="post", linewidth=1.2)
        axis.set_title(f"B1C {channel} primary code, PRN{REPRESENTATIVE_PRN}, first 200 chips")
        axis.set_ylabel("Bipolar value")
        axis.set_ylim(-1.4, 1.4)
        axis.grid(True, alpha=0.3)
    axes[-1].set_xlabel("Chip index")
    fig.tight_layout()
    fig.savefig(OUTPUT_DIR / "b1c_data_pilot_prn1_first_200chips.png", dpi=180)
    plt.close(fig)


def plot_autocorrelation() -> None:
    fig, axes = plt.subplots(2, 2, figsize=(10, 6.2))
    for row, channel in enumerate(("data", "pilot")):
        lags, corr = periodic_correlation(primary_code(REPRESENTATIVE_PRN, channel, bipolar=True))

        axes[row, 0].plot(lags, corr, linewidth=1.0)
        axes[row, 0].set_title(f"Periodic autocorrelation, {channel}, PRN{REPRESENTATIVE_PRN}")
        axes[row, 0].set_ylabel("Normalized value")
        axes[row, 0].grid(True, alpha=0.3)

        local_mask = (lags >= -200) & (lags <= 200)
        axes[row, 1].plot(lags[local_mask], corr[local_mask], linewidth=1.0)
        axes[row, 1].set_title(f"Local sidelobes, {channel}, PRN{REPRESENTATIVE_PRN}")
        axes[row, 1].grid(True, alpha=0.3)

    axes[1, 0].set_xlabel("Lag / chips")
    axes[1, 1].set_xlabel("Lag / chips")
    fig.tight_layout()
    fig.savefig(OUTPUT_DIR / "b1c_data_pilot_prn1_autocorrelation.png", dpi=180)
    plt.close(fig)


def plot_crosscorrelation() -> None:
    fig, axes = plt.subplots(2, 1, figsize=(10, 5.5), sharex=True)
    for axis, channel in zip(axes, ("data", "pilot"), strict=True):
        code_1 = primary_code(REPRESENTATIVE_PAIR[0], channel, bipolar=True)
        code_2 = primary_code(REPRESENTATIVE_PAIR[1], channel, bipolar=True)
        lags, corr = periodic_correlation(code_1, code_2)
        mask = (lags >= -400) & (lags <= 400)
        axis.plot(lags[mask], corr[mask], linewidth=1.0)
        axis.set_title(
            f"Periodic cross-correlation, {channel}, PRN{REPRESENTATIVE_PAIR[0]} vs PRN{REPRESENTATIVE_PAIR[1]}"
        )
        axis.set_ylabel("Normalized value")
        axis.grid(True, alpha=0.3)

    axes[-1].set_xlabel("Lag / chips")
    fig.tight_layout()
    fig.savefig(OUTPUT_DIR / "b1c_data_pilot_prn1_prn2_crosscorrelation.png", dpi=180)
    plt.close(fig)


def plot_power_spectrum() -> None:
    fig, ax = plt.subplots(figsize=(10, 4.2))
    for channel, color in (("data", "tab:blue"), ("pilot", "tab:orange")):
        freq_hz, psd = power_spectrum(channel)
        ax.plot(freq_hz / 1e6, 10.0 * np.log10(np.maximum(psd, 1e-12)), label=channel, linewidth=1.1, color=color)

    ax.set_title("Average normalized power spectrum of B1C primary codes")
    ax.set_xlabel("Frequency / MHz")
    ax.set_ylabel("Relative PSD / dB")
    ax.set_xlim(-CHIP_RATE_HZ / 2e6, CHIP_RATE_HZ / 2e6)
    ax.set_ylim(-70, 2)
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(OUTPUT_DIR / "b1c_data_pilot_power_spectrum.png", dpi=180)
    plt.close(fig)


def plot_truncation_crosscorrelation(studies: dict[str, list[dict[str, object]]]) -> None:
    fig, axes = plt.subplots(2, 1, figsize=(10, 6.2), sharex=True)
    for axis, channel in zip(axes, ("data", "pilot"), strict=True):
        study = studies[channel]
        lengths = [item["length"] for item in study]
        representative = [item["representative_pair"]["max_abs_crosscorr"] for item in study]
        worst_case = [item["worst_case_pair"]["max_abs_crosscorr"] for item in study]
        mean_values = [item["mean_pair_max_abs_crosscorr"] for item in study]

        axis.plot(lengths, representative, marker="o", linewidth=1.2, label="PRN1 vs PRN2")
        axis.plot(lengths, mean_values, marker="s", linewidth=1.2, label="Mean over all PRN pairs")
        axis.plot(lengths, worst_case, marker="^", linewidth=1.2, label="Worst PRN pair")
        axis.set_xscale("log", base=2)
        axis.set_ylabel("Max |R|")
        axis.set_title(f"Aperiodic cross-correlation after truncation ({channel})")
        axis.grid(True, which="both", alpha=0.3)
        axis.legend()

    axes[-1].set_xlabel("Truncation length / chips")
    fig.tight_layout()
    fig.savefig(OUTPUT_DIR / "b1c_truncation_crosscorr.png", dpi=180)
    plt.close(fig)


def build_summary() -> dict[str, object]:
    failures = validate_primary_codes()
    truncation_studies = {channel: truncation_crosscorrelation_study(channel) for channel in CHANNEL_TABLES}

    return {
        "validation_passed": not failures,
        "validation_failures": failures,
        "primary_code_length": PRIMARY_CODE_LENGTH,
        "chip_rate_hz": CHIP_RATE_HZ,
        "available_channels": sorted(CHANNEL_TABLES),
        "balance_summary": {channel: balance_summary(channel) for channel in CHANNEL_TABLES},
        "autocorrelation_summary": {channel: autocorrelation_summary(channel) for channel in CHANNEL_TABLES},
        "periodic_crosscorrelation_summary": {
            channel: periodic_crosscorrelation_summary(channel) for channel in CHANNEL_TABLES
        },
        "truncation_crosscorrelation_summary": truncation_studies,
        "power_spectrum_summary": {channel: power_spectrum_summary(channel) for channel in CHANNEL_TABLES},
        "figure_files": [
            "b1c_data_pilot_prn1_first_200chips.png",
            "b1c_data_pilot_prn1_autocorrelation.png",
            "b1c_data_pilot_prn1_prn2_crosscorrelation.png",
            "b1c_data_pilot_power_spectrum.png",
            "b1c_truncation_crosscorr.png",
        ],
    }


def write_summary(summary: dict[str, object]) -> None:
    with (OUTPUT_DIR / "b1c_analysis_summary.json").open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2, ensure_ascii=False)


def main() -> None:
    ensure_output_dir()
    summary = build_summary()
    plot_first_chips()
    plot_autocorrelation()
    plot_crosscorrelation()
    plot_power_spectrum()
    plot_truncation_crosscorrelation(summary["truncation_crosscorrelation_summary"])
    write_summary(summary)
    print(f"analysis artifacts written to {OUTPUT_DIR.resolve()}")


if __name__ == "__main__":
    main()
