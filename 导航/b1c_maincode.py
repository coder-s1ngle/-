"""Generate and validate BDS B1C primary ranging codes."""

from __future__ import annotations

import argparse
from functools import lru_cache

import numpy as np

from b1c_parameters import DATA_PRIMARY_PARAMS, PILOT_PRIMARY_PARAMS

WEIL_LENGTH = 10243
PRIMARY_CODE_LENGTH = 10230
CHIP_RATE_HZ = 1.023e6

CHANNEL_TABLES = {
    "data": DATA_PRIMARY_PARAMS,
    "pilot": PILOT_PRIMARY_PARAMS,
}


def octal24_to_bits(octal_word: str) -> np.ndarray:
    """Convert an 8-digit octal word into 24 bits, MSB first."""
    bits = []
    for digit in octal_word:
        value = int(digit, 8)
        # 每个八进制位对应 3 个二进制位，因此 8 位八进制正好展开为 24 个码片。
        bits.extend(((value >> 2) & 1, (value >> 1) & 1, value & 1))
    return np.array(bits, dtype=np.uint8)


@lru_cache(maxsize=1)
def legendre_sequence() -> np.ndarray:
    """Generate the Legendre sequence from quadratic residues modulo 10243."""

    seq = np.zeros(WEIL_LENGTH, dtype=np.uint8)
    for x in range(1, WEIL_LENGTH):
        # 平方剩余所在的位置就是 Legendre 序列中取值为 1 的位置。
        seq[(x * x) % WEIL_LENGTH] = 1
    return seq


def weil_code(phase_diff: int) -> np.ndarray:
    """Build a length-10243 Weil code."""
    legendre = legendre_sequence()
    return np.bitwise_xor(legendre, np.roll(legendre, -phase_diff))


def primary_code(prn: int, channel: str = "data", bipolar: bool = False) -> np.ndarray:
    """Generate a B1C primary code for the selected PRN and channel."""
    try:
        phase_diff, truncation_point, _, _ = CHANNEL_TABLES[channel][prn]
    except KeyError as exc:
        raise ValueError(f"unsupported channel/prn: {channel}/{prn}") from exc

    code = weil_code(phase_diff)
    # ICD 中截取点 p 采用一基编号，而 Python 数组下标采用零基编号。
    start = truncation_point - 1
    # 这里按公式 c(n;w,p)=W((n+p-1) mod N; w) 实现循环截取。
    indices = (np.arange(PRIMARY_CODE_LENGTH) + start) % WEIL_LENGTH
    primary = code[indices]
    if bipolar:
        # 在相关分析中，使用 +/-1 双极性码比使用 0/1 码更方便。
        return 1 - 2 * primary.astype(np.int8)
    return primary


def to_bipolar(code: np.ndarray) -> np.ndarray:
    """Convert a code sequence to bipolar float form."""
    seq = np.asarray(code)
    if set(np.unique(seq)) <= {0, 1}:
        return 1.0 - 2.0 * seq.astype(np.float64)
    if set(np.unique(seq)) <= {0.0, 1.0}:
        return 1.0 - 2.0 * seq.astype(np.float64)
    return seq.astype(np.float64)


def periodic_correlation(code_a: np.ndarray, code_b: np.ndarray | None = None) -> tuple[np.ndarray, np.ndarray]:
    """Return normalized periodic correlation using FFT."""
    if code_b is None:
        code_b = code_a

    seq_a = to_bipolar(code_a)
    seq_b = to_bipolar(code_b)

    # 时域中的周期相关可以等效为 FFT(a) 与 conj(FFT(b)) 的频域乘积。
    spectrum = np.fft.fft(seq_a) * np.conj(np.fft.fft(seq_b))
    corr = np.fft.ifft(spectrum).real
    # 将零延迟移到中间位置，便于后续观察和绘图。
    corr = np.roll(corr, len(corr) // 2)
    lags = np.arange(-len(corr) // 2, len(corr) - len(corr) // 2)
    return lags, corr / len(seq_a)


def aperiodic_correlation(code_a: np.ndarray, code_b: np.ndarray | None = None) -> tuple[np.ndarray, np.ndarray]:
    """Return normalized finite-length correlation for truncated sequences."""
    if code_b is None:
        code_b = code_a

    seq_a = to_bipolar(code_a)
    seq_b = to_bipolar(code_b)

    nfft = 1 << (len(seq_a) + len(seq_b) - 2).bit_length()
    spectrum = np.fft.fft(seq_a, n=nfft) * np.fft.fft(seq_b[::-1], n=nfft)
    corr = np.fft.ifft(spectrum).real[: len(seq_a) + len(seq_b) - 1]
    lags = np.arange(-(len(seq_b) - 1), len(seq_a))
    norm = np.sqrt(np.dot(seq_a, seq_a) * np.dot(seq_b, seq_b))
    return lags, corr / norm


def validate_primary_codes() -> list[str]:
    """Check all 126 primary codes against ICD head/tail octal values."""
    failures = []
    for channel, table in CHANNEL_TABLES.items():
        for prn, (_, _, head_octal, tail_octal) in table.items():
            code = primary_code(prn, channel)
            # ICD 直接给出了每条主码的前 24 位和末尾 24 位，便于做一致性校验。
            if not np.array_equal(code[:24], octal24_to_bits(head_octal)):
                failures.append(f"{channel} PRN{prn} head mismatch")
            if not np.array_equal(code[-24:], octal24_to_bits(tail_octal)):
                failures.append(f"{channel} PRN{prn} tail mismatch")
    return failures


def format_preview(code: np.ndarray, chips: int, bipolar: bool) -> str:
    values = code[:chips]
    if bipolar:
        return " ".join(f"{int(v):+d}" for v in values)
    return "".join(str(int(v)) for v in values)


def build_argparser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--channel", choices=sorted(CHANNEL_TABLES), default="data")
    parser.add_argument("--prn", type=int, default=1)
    parser.add_argument("--chips", type=int, default=64, help="number of chips to print")
    parser.add_argument("--bipolar", action="store_true", help="print +1/-1 chips")
    parser.add_argument("--validate", action="store_true", help="validate all 126 codes")
    return parser


def main() -> None:
    args = build_argparser().parse_args()
    if args.validate:
        failures = validate_primary_codes()
        if failures:
            print("validation failed")
            for failure in failures:
                print(failure)
            raise SystemExit(1)
        print("all 126 B1C primary codes validated against the ICD tables")
        return

    code = primary_code(args.prn, args.channel, bipolar=args.bipolar)
    print(f"channel={args.channel} prn={args.prn} chips={PRIMARY_CODE_LENGTH} chip_rate={CHIP_RATE_HZ:.0f} Hz")
    print(format_preview(code, args.chips, args.bipolar))


if __name__ == "__main__":
    main()
