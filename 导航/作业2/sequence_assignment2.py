from __future__ import annotations

import json
from dataclasses import dataclass
from itertools import combinations
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


ORDER = 13
FULL_PERIOD = 2**ORDER - 1
CHIP_RATE_HZ = 8.184e6
X1_TRUNCATED_LENGTH = 8184
X2_TRUNCATED_LENGTH = 8185
SHORT_P_PERIOD_CHIPS = X1_TRUNCATED_LENGTH * X2_TRUNCATED_LENGTH
SHORT_P_PERIOD_SECONDS = SHORT_P_PERIOD_CHIPS / CHIP_RATE_HZ
ONE_SECOND_LENGTH = int(CHIP_RATE_HZ)
OUTPUT_DIR = Path("output")

# The task document stores formula (4-9) as embedded Equation.3 objects.
# The polynomial pair below is reconstructed from that formula and matches
# one 13th-order preferred pair.
G1_DEGREES = (13, 4, 3, 1, 0)
G2_DEGREES = (13, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
REGISTER1_INIT = (1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0)
REGISTER2_INITIAL_STATES = (
    (0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0),
    (1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0),
    (0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0),
    (0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
    (1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
    (1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0),
    (1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0),
    (1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0),
    (1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0),
    (1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0),
)
REPRESENTATIVE_CODE_INDEX = 1
REPRESENTATIVE_PAIR = ("C1", "C2")


@dataclass(frozen=True)
class SequenceStats:
    name: str
    length: int
    ones: int
    zeros: int


def lfsr_m_sequence(
    order: int,
    polynomial_degrees: tuple[int, ...],
    initial_state: tuple[int, ...],
    length: int | None = None,
) -> np.ndarray:
    """Generates an m-sequence from the characteristic recurrence."""
    if length is None:
        length = 2**order - 1
    if len(initial_state) != order:
        raise ValueError(f"initial_state must have length {order}")
    seq = np.empty(length, dtype=np.uint8)
    seq[:order] = np.array(initial_state, dtype=np.uint8)
    taps = [degree for degree in polynomial_degrees if degree != order]
    for k in range(length - order):
        feedback = 0
        for degree in taps:
            feedback ^= int(seq[k + degree])
        seq[k + order] = feedback
    return seq


def to_bipolar_float(seq: np.ndarray) -> np.ndarray:
    return 1.0 - 2.0 * seq.astype(np.float32)


def periodic_correlation(a: np.ndarray, b: np.ndarray) -> np.ndarray:
    aa = to_bipolar_float(a)
    bb = to_bipolar_float(b)
    spectrum = np.fft.rfft(aa) * np.conj(np.fft.rfft(bb))
    corr = np.fft.irfft(spectrum, n=len(a)).astype(np.float32)
    return corr


def truncate_periodic(seq: np.ndarray, length: int) -> np.ndarray:
    return seq[:length].copy()


def repeated_view(seq: np.ndarray, total_length: int, shift: int = 0) -> np.ndarray:
    idx = (np.arange(total_length, dtype=np.int64) + shift) % len(seq)
    return seq[idx]


def short_p_code_binary(x1_trunc: np.ndarray, x2_trunc: np.ndarray, delay: int, length: int) -> np.ndarray:
    x1 = repeated_view(x1_trunc, length, shift=0)
    x2 = repeated_view(x2_trunc, length, shift=delay)
    return np.bitwise_xor(x1, x2)


def describe_sequence(name: str, seq: np.ndarray) -> SequenceStats:
    ones = int(seq.sum())
    return SequenceStats(name=name, length=len(seq), ones=ones, zeros=len(seq) - ones)


def centered_correlation(corr: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    centered = np.roll(corr, len(corr) // 2)
    lags = np.arange(-len(corr) // 2, len(corr) - len(corr) // 2, dtype=np.int64)
    return lags, centered


def build_code_family(
    x1_trunc: np.ndarray,
) -> tuple[dict[str, np.ndarray], dict[str, dict[str, list[int] | int | float]]]:
    codes: dict[str, np.ndarray] = {}
    stats: dict[str, dict[str, list[int] | int | float]] = {}
    for idx, register2_state in enumerate(REGISTER2_INITIAL_STATES, start=1):
        label = f"C{idx}"
        x2_full = lfsr_m_sequence(ORDER, G2_DEGREES, register2_state)
        x2_trunc = truncate_periodic(x2_full, X2_TRUNCATED_LENGTH)
        code = short_p_code_binary(x1_trunc, x2_trunc, delay=0, length=ONE_SECOND_LENGTH)
        codes[label] = code
        stats[label] = {
            "registers2": list(register2_state),
            "x2_full": describe_sequence(f"{label}_x2_full", x2_full).__dict__,
            "x2_truncated": describe_sequence(f"{label}_x2_truncated", x2_trunc).__dict__,
            "one_second_code": describe_sequence(f"{label}_one_second", code).__dict__,
            "bipolar_mean": float(np.mean(to_bipolar_float(code))),
        }
    return codes, stats


def autocorrelation_metrics(code: np.ndarray) -> tuple[dict[str, float | str], np.ndarray]:
    corr = periodic_correlation(code, code) / len(code)
    sidelobes = corr[1:]
    metrics = {
        "code": f"C{REPRESENTATIVE_CODE_INDEX}",
        "peak": float(corr[0]),
        "max_abs_sidelobe": float(np.max(np.abs(sidelobes))),
        "rms_sidelobe": float(np.sqrt(np.mean(np.square(sidelobes)))),
    }
    return metrics, corr


def crosscorrelation_metrics(
    codes: dict[str, np.ndarray],
) -> tuple[dict[str, object], dict[tuple[str, str], np.ndarray], np.ndarray, np.ndarray, list[str]]:
    labels = list(codes.keys())
    pair_curves: dict[tuple[str, str], np.ndarray] = {}
    search_matrix = np.eye(len(labels), dtype=np.float32)
    zero_lag_matrix = np.eye(len(labels), dtype=np.float32)
    pair_stats: list[dict[str, object]] = []

    for i, j in combinations(range(len(labels)), 2):
        label_a = labels[i]
        label_b = labels[j]
        corr = periodic_correlation(codes[label_a], codes[label_b]) / len(codes[label_a])
        pair_curves[(label_a, label_b)] = corr
        max_abs = float(np.max(np.abs(corr)))
        rms = float(np.sqrt(np.mean(np.square(corr))))
        zero_lag = float(corr[0])
        search_matrix[i, j] = search_matrix[j, i] = max_abs
        zero_lag_matrix[i, j] = zero_lag_matrix[j, i] = abs(zero_lag)
        pair_stats.append(
            {
                "pair": [label_a, label_b],
                "max_abs": max_abs,
                "zero_lag": zero_lag,
                "rms": rms,
            }
        )

    representative_item = next(
        item for item in pair_stats if tuple(item["pair"]) == REPRESENTATIVE_PAIR
    )
    worst_search_pair = max(pair_stats, key=lambda item: float(item["max_abs"]))
    worst_zero_lag_pair = max(pair_stats, key=lambda item: abs(float(item["zero_lag"])))

    summary = {
        "representative_pair": representative_item,
        "worst_search_pair": worst_search_pair,
        "worst_zero_lag_pair": worst_zero_lag_pair,
        "mean_pair_max_abs": float(np.mean([item["max_abs"] for item in pair_stats])),
        "mean_pair_abs_zero_lag": float(np.mean([abs(item["zero_lag"]) for item in pair_stats])),
        "mean_pair_rms": float(np.mean([item["rms"] for item in pair_stats])),
        "pair_count": len(pair_stats),
        "all_pairs": sorted(pair_stats, key=lambda item: float(item["max_abs"]), reverse=True),
    }
    return summary, pair_curves, search_matrix, zero_lag_matrix, labels


def plot_first_chips(x1: np.ndarray, x2: np.ndarray, code: np.ndarray) -> Path:
    fig, axes = plt.subplots(3, 1, figsize=(10, 6), sharex=True)
    show = 128
    axes[0].step(np.arange(show), x1[:show], where="post")
    axes[0].set_title("X1 first 128 chips")
    axes[1].step(np.arange(show), x2[:show], where="post")
    axes[1].set_title("X2 first 128 chips under C1 registers2")
    axes[2].step(np.arange(show), code[:show], where="post")
    axes[2].set_title("One-second short P code C1 first 128 chips")
    axes[2].set_xlabel("Chip index")
    for ax in axes:
        ax.set_ylim(-0.2, 1.2)
        ax.grid(True, alpha=0.3)
    fig.tight_layout()
    out = OUTPUT_DIR / "first_128_chips.png"
    fig.savefig(out, dpi=160)
    plt.close(fig)
    return out


def plot_preferred_pair_crosscorr(corr: np.ndarray) -> Path:
    lags = np.arange(len(corr))
    fig, ax = plt.subplots(figsize=(10, 3.8))
    ax.plot(lags, corr / FULL_PERIOD, linewidth=0.8)
    ax.set_title("Normalized periodic cross-correlation of X1 and X2")
    ax.set_xlabel("Lag")
    ax.set_ylabel("Correlation")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    out = OUTPUT_DIR / "preferred_pair_crosscorr.png"
    fig.savefig(out, dpi=160)
    plt.close(fig)
    return out


def plot_one_second_autocorr(corr: np.ndarray, label: str) -> Path:
    lags, centered = centered_correlation(corr)
    local_mask = np.abs(lags) <= 2000
    step = max(1, len(centered) // 16000)

    fig, axes = plt.subplots(1, 2, figsize=(12, 4.5))
    axes[0].plot(lags[::step], centered[::step], linewidth=0.8)
    axes[0].set_title(f"Periodic autocorrelation of 1 s short P code {label}")
    axes[0].set_xlabel("Lag / chips")
    axes[0].set_ylabel("Normalized value")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(lags[local_mask], centered[local_mask], linewidth=0.8)
    axes[1].set_title(f"Local sidelobes of {label}")
    axes[1].set_xlabel("Lag / chips")
    axes[1].set_ylabel("Normalized value")
    axes[1].grid(True, alpha=0.3)

    fig.tight_layout()
    out = OUTPUT_DIR / "one_second_autocorr.png"
    fig.savefig(out, dpi=160)
    plt.close(fig)
    return out


def plot_one_second_crosscorr_heatmap(matrix: np.ndarray, labels: list[str]) -> Path:
    fig, ax = plt.subplots(figsize=(6.5, 5.5))
    im = ax.imshow(matrix, cmap="viridis", vmin=0.0, vmax=float(np.max(matrix)))
    ax.set_xticks(np.arange(len(labels)), labels)
    ax.set_yticks(np.arange(len(labels)), labels)
    ax.set_title("Absolute zero-lag cross-correlation of 1 s short P codes")
    for i in range(len(labels)):
        for j in range(len(labels)):
            ax.text(j, i, f"{matrix[i, j]:.3f}", ha="center", va="center", color="white", fontsize=7)
    fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04, label="Max |R|")
    fig.tight_layout()
    out = OUTPUT_DIR / "one_second_crosscorr_heatmap.png"
    fig.savefig(out, dpi=160)
    plt.close(fig)
    return out


def plot_one_second_crosscorr_example(corr: np.ndarray, label_a: str, label_b: str) -> Path:
    lags, centered = centered_correlation(corr)
    step = max(1, len(centered) // 16000)

    fig, ax = plt.subplots(figsize=(10, 4))
    ax.plot(lags[::step], centered[::step], linewidth=0.8)
    ax.set_title(f"Periodic cross-correlation of 1 s short P codes {label_a} and {label_b}")
    ax.set_xlabel("Lag / chips")
    ax.set_ylabel("Normalized value")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    out = OUTPUT_DIR / "one_second_crosscorr_example.png"
    fig.savefig(out, dpi=160)
    plt.close(fig)
    return out


def build_summary() -> dict:
    x1_full = lfsr_m_sequence(ORDER, G1_DEGREES, REGISTER1_INIT)
    x2_reference_full = lfsr_m_sequence(ORDER, G2_DEGREES, REGISTER2_INITIAL_STATES[0])
    x1_trunc = truncate_periodic(x1_full, X1_TRUNCATED_LENGTH)

    preferred_pair_corr = periodic_correlation(
        x1_full,
        lfsr_m_sequence(ORDER, G2_DEGREES, REGISTER2_INITIAL_STATES[1]),
    )
    preferred_levels = sorted(int(v) for v in np.unique(np.rint(preferred_pair_corr).astype(np.int64)))

    codes, code_stats = build_code_family(x1_trunc)
    representative_label = f"C{REPRESENTATIVE_CODE_INDEX}"
    representative_autocorr_metrics, representative_autocorr = autocorrelation_metrics(
        codes[representative_label]
    )
    crosscorr_summary, pair_curves, search_matrix, zero_lag_matrix, code_labels = crosscorrelation_metrics(codes)

    first_chip_plot = plot_first_chips(x1_full, x2_reference_full, codes["C1"])
    preferred_pair_plot = plot_preferred_pair_crosscorr(preferred_pair_corr)
    autocorr_plot = plot_one_second_autocorr(representative_autocorr, representative_label)
    heatmap_plot = plot_one_second_crosscorr_heatmap(zero_lag_matrix, code_labels)
    worst_pair = tuple(crosscorr_summary["worst_search_pair"]["pair"])
    worst_pair_plot = plot_one_second_crosscorr_example(pair_curves[worst_pair], worst_pair[0], worst_pair[1])

    return {
        "assumed_formulas": {
            "G1": "1 + x + x^3 + x^4 + x^13",
            "G2": "1 + x + x^2 + x^3 + x^4 + x^5 + x^6 + x^7 + x^8 + x^9 + x^10 + x^11 + x^13",
            "P_i_pm1": "P_i(t) = X_1(t) X_2(t - i T_c)",
            "P_i_binary": "P_i(t) = X_1(t) xor X_2(t - i T_c)",
        },
        "initial_register_states": {
            "registers1": list(REGISTER1_INIT),
            "registers2_family": {
                f"C{idx}": list(state) for idx, state in enumerate(REGISTER2_INITIAL_STATES, start=1)
            },
        },
        "sequence_stats": {
            "x1_full": describe_sequence("x1_full", x1_full).__dict__,
            "x1_truncated": describe_sequence("x1_truncated", x1_trunc).__dict__,
            "code_family": code_stats,
        },
        "periods": {
            "full_m_sequence_chips": FULL_PERIOD,
            "x1_truncated_chips": X1_TRUNCATED_LENGTH,
            "x2_truncated_chips": X2_TRUNCATED_LENGTH,
            "short_p_period_chips": SHORT_P_PERIOD_CHIPS,
            "short_p_period_seconds": SHORT_P_PERIOD_SECONDS,
            "one_second_short_p_chips": ONE_SECOND_LENGTH,
        },
        "preferred_pair_crosscorr": {
            "unnormalized_levels": preferred_levels,
            "normalized_levels": [v / FULL_PERIOD for v in preferred_levels],
            "counts": {
                str(level): int(np.sum(np.rint(preferred_pair_corr).astype(np.int64) == level))
                for level in preferred_levels
            },
            "expected_levels": [-129 / FULL_PERIOD, -1 / FULL_PERIOD, 127 / FULL_PERIOD],
        },
        "one_second_correlation": {
            "autocorrelation": representative_autocorr_metrics,
            "crosscorrelation": crosscorr_summary,
            "search_max_matrix": search_matrix.tolist(),
            "zero_lag_abs_matrix": zero_lag_matrix.tolist(),
        },
        "artifacts": {
            "first_chip_plot": str(first_chip_plot),
            "preferred_pair_plot": str(preferred_pair_plot),
            "one_second_autocorr_plot": str(autocorr_plot),
            "one_second_crosscorr_heatmap": str(heatmap_plot),
            "one_second_crosscorr_example": str(worst_pair_plot),
        },
    }


def main() -> None:
    OUTPUT_DIR.mkdir(exist_ok=True)
    summary = build_summary()
    out = OUTPUT_DIR / "summary.json"
    out.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
