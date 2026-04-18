"""Validate B1C data primary codes against ICD table 5-2."""

from __future__ import annotations

import json
from pathlib import Path

from b1c_maincode import DATA_PRIMARY_PARAMS, octal24_to_bits, primary_code

OUTPUT_DIR = Path("output")


def bits_to_octal(bits) -> str:
    digits = []
    for idx in range(0, 24, 3):
        value = (int(bits[idx]) << 2) | (int(bits[idx + 1]) << 1) | int(bits[idx + 2])
        digits.append(str(value))
    return "".join(digits)


def validate_table_5_2() -> tuple[list[dict], dict]:
    details = []
    passed = 0
    for prn, (_, _, head_ref, tail_ref) in DATA_PRIMARY_PARAMS.items():
        code = primary_code(prn, "data")
        head_bits = code[:24]
        tail_bits = code[-24:]
        head_calc = bits_to_octal(head_bits)
        tail_calc = bits_to_octal(tail_bits)
        row = {
            "prn": int(prn),
            "head_ref_octal": head_ref,
            "head_calc_octal": head_calc,
            "head_match": bool(head_calc == head_ref and (head_bits == octal24_to_bits(head_ref)).all()),
            "tail_ref_octal": tail_ref,
            "tail_calc_octal": tail_calc,
            "tail_match": bool(tail_calc == tail_ref and (tail_bits == octal24_to_bits(tail_ref)).all()),
        }
        row["passed"] = bool(row["head_match"] and row["tail_match"])
        if row["passed"]:
            passed += 1
        details.append(row)

    summary = {
        "table": "5-2",
        "description": "B1C data primary-code validation against ICD table 5-2",
        "total_prns": len(details),
        "passed_prns": passed,
        "failed_prns": len(details) - passed,
        "all_passed": passed == len(details),
    }
    return details, summary


def write_outputs(details: list[dict], summary: dict) -> None:
    OUTPUT_DIR.mkdir(exist_ok=True)

    json_path = OUTPUT_DIR / "b1c_table5_2_validation.json"
    md_path = OUTPUT_DIR / "b1c_table5_2_validation.md"

    with json_path.open("w", encoding="utf-8") as handle:
        json.dump({"summary": summary, "details": details}, handle, indent=2, ensure_ascii=False)

    sample_prns = [1, 2, 3, 10, 20, 30, 40, 50, 63]
    selected = [row for row in details if row["prn"] in sample_prns]

    lines = [
        "# B1C 主码与 ICD 表 5-2 校验结果",
        "",
        f"- 校验对象: {summary['description']}",
        f"- PRN 总数: {summary['total_prns']}",
        f"- 通过数: {summary['passed_prns']}",
        f"- 失败数: {summary['failed_prns']}",
        f"- 结论: {'全部通过' if summary['all_passed'] else '存在不一致'}",
        "",
        "## 抽样展示",
        "",
        "| PRN | 表5-2头24位 | 程序头24位 | 表5-2尾24位 | 程序尾24位 | 结果 |",
        "| --- | --- | --- | --- | --- | --- |",
    ]

    for row in selected:
        lines.append(
            f"| {row['prn']} | {row['head_ref_octal']} | {row['head_calc_octal']} | "
            f"{row['tail_ref_octal']} | {row['tail_calc_octal']} | {'通过' if row['passed'] else '失败'} |"
        )

    with md_path.open("w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


def main() -> None:
    details, summary = validate_table_5_2()
    write_outputs(details, summary)
    result = "ALL PASSED" if summary["all_passed"] else "FAILED"
    print(
        f"table 5-2 validation: {result}, "
        f"{summary['passed_prns']}/{summary['total_prns']} PRNs matched"
    )


if __name__ == "__main__":
    main()
