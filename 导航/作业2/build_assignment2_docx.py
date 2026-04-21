from pathlib import Path
import sys

from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


BASE_DIR = Path(__file__).resolve().parent
MD_PATH = BASE_DIR / "assignment2_report.md"
DOCX_PATH = BASE_DIR / "assignment2_report.docx"
OUTPUT_DIR = BASE_DIR / "output"

FONT_SONGTI = "\u5b8b\u4f53"
FONT_HEITI = "\u9ed1\u4f53"

INLINE_IMAGES = {
    "2.2 \u5e8f\u5217\u751f\u6210\u6d41\u7a0b": [
        ("\u56fe1 X1\u3001\u53c2\u8003 X2 \u4ee5\u53ca C1 \u524d 128 \u4e2a\u7801\u7247", "first_128_chips.png"),
    ],
    "3.2 \u5b8c\u6574\u4f18\u9009\u5bf9\u9a8c\u8bc1": [
        ("\u56fe2 \u5b8c\u6574 X1/X2 \u4f18\u9009\u5bf9\u5468\u671f\u4e92\u76f8\u5173", "preferred_pair_crosscorr.png"),
    ],
    "3.3 \u4ee3\u8868\u6027 `1 s` \u7801\u81ea\u76f8\u5173": [
        ("\u56fe3 \u4ee3\u8868\u6027 1 s \u7801 C1 \u7684\u5468\u671f\u81ea\u76f8\u5173", "one_second_autocorr.png"),
    ],
    "3.4 `1 s` \u7801\u65cf\u4e92\u76f8\u5173": [
        ("\u56fe4 10 \u6761 1 s \u7801\u7684\u96f6\u5ef6\u8fdf\u4e92\u76f8\u5173\u77e9\u9635", "one_second_crosscorr_heatmap.png"),
        ("\u56fe5 \u6700\u5dee\u7801\u5bf9 C1-C2 \u7684\u5468\u671f\u4e92\u76f8\u5173\u66f2\u7ebf", "one_second_crosscorr_example.png"),
    ],
}


def set_run_font(run, size_pt: float, east_asia: str = FONT_SONGTI, bold: bool = False) -> None:
    run.font.name = "Times New Roman"
    run._element.rPr.rFonts.set(qn("w:eastAsia"), east_asia)
    run.font.size = Pt(size_pt)
    run.font.bold = bold
    run.font.color.rgb = RGBColor(0, 0, 0)


def style_paragraph(para, *, heading: bool = False, title: bool = False) -> None:
    fmt = para.paragraph_format
    fmt.line_spacing = 1.5
    fmt.first_line_indent = Pt(0) if heading or title else Pt(24)
    if title:
        para.alignment = WD_ALIGN_PARAGRAPH.CENTER
    elif heading:
        para.alignment = WD_ALIGN_PARAGRAPH.LEFT
    else:
        para.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY

    for run in para.runs:
        if title or heading:
            set_run_font(run, 14, east_asia=FONT_HEITI, bold=True)
        else:
            set_run_font(run, 12)


def add_paragraph(doc: Document, text: str, *, heading: bool = False, title: bool = False) -> None:
    para = doc.add_paragraph()
    para.add_run(text)
    style_paragraph(para, heading=heading, title=title)


def add_caption(doc: Document, text: str) -> None:
    para = doc.add_paragraph()
    para.add_run(text)
    para.paragraph_format.line_spacing = 1.5
    para.paragraph_format.first_line_indent = Pt(0)
    para.alignment = WD_ALIGN_PARAGRAPH.CENTER
    for run in para.runs:
        set_run_font(run, 12)


def flush_paragraph(doc: Document, buffer: list[str]) -> None:
    if not buffer:
        return
    text = " ".join(part.strip() for part in buffer if part.strip()).strip()
    if text:
        add_paragraph(doc, text)
    buffer.clear()


def insert_inline_images(doc: Document, heading_text: str) -> None:
    for caption, filename in INLINE_IMAGES.get(heading_text, []):
        path = OUTPUT_DIR / filename
        if path.exists():
            doc.add_picture(str(path), width=Inches(6.2))
            add_caption(doc, caption)


def build_docx(output_path: Path | None = None) -> None:
    if output_path is None:
        output_path = DOCX_PATH

    text = MD_PATH.read_text(encoding="utf-8")
    doc = Document()
    section = doc.sections[0]
    section.top_margin = Pt(72)
    section.bottom_margin = Pt(72)
    section.left_margin = Pt(85.05)
    section.right_margin = Pt(70.85)

    paragraph_buffer: list[str] = []

    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        stripped = line.strip()

        if not stripped:
            flush_paragraph(doc, paragraph_buffer)
            continue

        if stripped.startswith("# "):
            flush_paragraph(doc, paragraph_buffer)
            add_paragraph(doc, stripped[2:].strip(), title=True)
            continue

        if stripped.startswith("## "):
            flush_paragraph(doc, paragraph_buffer)
            add_paragraph(doc, stripped[3:].strip(), heading=True)
            continue

        if stripped.startswith("### "):
            flush_paragraph(doc, paragraph_buffer)
            heading_text = stripped[4:].strip()
            add_paragraph(doc, heading_text, heading=True)
            insert_inline_images(doc, heading_text)
            continue

        if stripped.startswith("- "):
            flush_paragraph(doc, paragraph_buffer)
            add_paragraph(doc, stripped[2:].strip())
            continue

        numbered = False
        for idx in range(1, 20):
            prefix = f"{idx}. "
            if stripped.startswith(prefix):
                flush_paragraph(doc, paragraph_buffer)
                add_paragraph(doc, stripped)
                numbered = True
                break
        if numbered:
            continue

        if stripped.startswith("`") and stripped.endswith("`"):
            flush_paragraph(doc, paragraph_buffer)
            add_paragraph(doc, stripped.strip("`"))
            continue

        paragraph_buffer.append(stripped)

    flush_paragraph(doc, paragraph_buffer)
    doc.save(output_path)


if __name__ == "__main__":
    target = Path(sys.argv[1]) if len(sys.argv) > 1 else DOCX_PATH
    build_docx(target)
