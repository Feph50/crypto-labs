#!/usr/bin/env python3
"""Render a Markdown report (tables, images, KaTeX math, Mermaid diagrams) to a PDF with page numbers.

    python scripts/md_to_pdf.py lab1/docs/lab1-main-report.md lab1/docs/lab1-main-report.pdf [--title "Lab 1"] [--keep-html]

How it works
  1. Markdown -> HTML with markdown-it-py (pip install markdown-it-py). Math ($...$, $$...$$) and code are protected so
     Markdown does not mangle them.
  2. The HTML loads KaTeX (math) and Mermaid (diagrams) from a CDN, so an internet connection is needed once per run.
  3. Microsoft Edge or Google Chrome in headless mode prints the page to PDF (relative image paths resolve against the
     Markdown file's folder).
  4. pypdf + reportlab stamp "page x / N" in the footer (pip install pypdf reportlab).
"""
import argparse
import html
import io
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from markdown_it import MarkdownIt

BROWSERS = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
    "/usr/bin/google-chrome", "/usr/bin/chromium", "/usr/bin/chromium-browser", "/usr/bin/microsoft-edge",
]

CSS = """
@page { size: A4; margin: 18mm 16mm 20mm 16mm; }
html { font-size: 10.2pt; }
body { font-family: "Segoe UI", "Calibri", "DejaVu Sans", Arial, sans-serif; line-height: 1.45; color: #111; }
h1 { font-size: 1.7em; border-bottom: 2px solid #1f3a5f; padding-bottom: .15em; margin-top: 0; break-before: page; color: #1f3a5f; }
h2 { font-size: 1.3em; margin-top: 1.3em; color: #1f3a5f; break-after: avoid; }
h3 { font-size: 1.1em; margin-top: 1.1em; break-after: avoid; }
h4 { font-size: 1.0em; break-after: avoid; }
.pagebreak { break-before: page; }
.cover h1, .cover h2, .cover h3 { break-before: auto; border: 0; text-align: center; }
.cover { text-align: center; }
.cover ul { display: inline-block; text-align: left; }
p, li { orphans: 3; widows: 3; }
code { font-family: Consolas, "Cascadia Mono", "DejaVu Sans Mono", monospace; font-size: .88em; background: #f2f4f7; padding: 0 .2em; border-radius: 2px; word-break: break-word; }
pre { background: #f6f8fa; border: 1px solid #d8dee4; border-radius: 4px; padding: 6px 8px; font-size: 8pt; line-height: 1.3; white-space: pre-wrap; word-break: break-all; break-inside: avoid-page; }
pre code { background: none; padding: 0; font-size: inherit; }
table { border-collapse: collapse; width: 100%; margin: .6em 0; font-size: 8.6pt; break-inside: auto; }
th, td { border: 1px solid #b8c0cc; padding: 2px 4px; vertical-align: top; overflow-wrap: break-word; word-break: normal; }
th { background: #e8eef6; }
tr { break-inside: avoid; }
thead { display: table-header-group; }
table.wide { font-size: 6.2pt; }
table.wide th, table.wide td { padding: 1px 2px; }
img { max-width: 100%; height: auto; display: block; margin: .5em auto; break-inside: avoid; }
em { color: #333; }
blockquote { border-left: 3px solid #9aa7b8; margin-left: 0; padding-left: .8em; color: #333; }
hr { border: 0; border-top: 1px solid #c5ccd6; margin: 1.2em 0; }
.mermaid { text-align: center; break-inside: avoid; margin: .8em 0; }
.katex-display { overflow-x: auto; overflow-y: hidden; break-inside: avoid; }
a { color: #0b5cad; text-decoration: none; }
.fileref { font-family: Consolas, monospace; font-size: .9em; }
"""

JS = """
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.css">
<script defer src="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/katex.min.js"></script>
<script defer src="https://cdn.jsdelivr.net/npm/katex@0.16.11/dist/contrib/auto-render.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/mermaid@10.9.1/dist/mermaid.min.js"></script>
<script>
document.addEventListener('DOMContentLoaded', async function () {
  document.querySelectorAll('table').forEach(function (t) {
    var row = t.querySelector('tr'); if (row && row.children.length >= 10) t.classList.add('wide');
  });
  try { mermaid.initialize({startOnLoad: false, theme: 'default', securityLevel: 'loose', flowchart: {htmlLabels: true}});
        await mermaid.run({querySelector: '.mermaid'}); } catch (e) { console.log('mermaid error', e); }
  var tries = 0;
  (function render() {
    if (window.renderMathInElement) {
      renderMathInElement(document.body, {delimiters: [{left: '$$', right: '$$', display: true}, {left: '$', right: '$', display: false}],
                                          throwOnError: false});
      document.body.setAttribute('data-ready', '1');
    } else if (tries++ < 100) { setTimeout(render, 100); }
  })();
});
</script>
"""


def protect(text, store, prefix, pattern, flags=0):
    def repl(m):
        key = f"@@{prefix}{len(store)}@@"
        store.append(m.group(0))
        return key
    return re.sub(pattern, repl, text, flags=flags)


def restore(text, store, prefix, fn):
    for i, val in enumerate(store):
        text = text.replace(f"@@{prefix}{i}@@", fn(val))
    return text


def slug(text):
    t = re.sub(r"<[^>]+>", "", text)
    t = html.unescape(t).lower().strip()
    t = re.sub(r"[^\w\- ]", "", t)
    return t.replace(" ", "-")


def md_to_html(md_text, title):
    md_text = md_text.lstrip(chr(0xFEFF))                                                   # byte-order mark
    md_text = re.sub(chr(13) + "+" + chr(10), chr(10), md_text).replace(chr(13), chr(10))    # CRLF / stray CRs -> LF
    code, math_blocks = [], []
    md_text = protect(md_text, code, "CODE", r"```.*?```", re.S)               # fenced code first
    md_text = protect(md_text, code, "CODE", r"`[^`\n]+`")                      # inline code
    md_text = protect(md_text, math_blocks, "MATH", r"\$\$.*?\$\$", re.S)       # display math
    md_text = protect(md_text, math_blocks, "MATH", r"(?<![\\$\w])\$(?!\s)[^$\n]+?(?<!\s)\$(?![\w$])")  # inline math
    md_text = restore(md_text, code, "CODE", lambda v: v)

    md = MarkdownIt("commonmark", {"html": True, "linkify": False}).enable("table").enable("strikethrough")
    body = md.render(md_text)

    def mermaid_block(m):
        return '<pre class="mermaid">' + m.group(1) + "</pre>"
    body = re.sub(r'<pre><code class="language-mermaid">(.*?)</code></pre>', mermaid_block, body, flags=re.S)
    body = restore(body, math_blocks, "MATH", lambda v: html.escape(v, quote=False))

    # heading ids (GitHub style) so the manual table of contents links work
    used = {}

    def hid(m):
        lvl, inner = m.group(1), m.group(2)
        base = slug(inner)
        n = used.get(base, 0)
        used[base] = n + 1
        return f'<h{lvl} id="{base}{"-" + str(n) if n else ""}">{inner}</h{lvl}>'
    body = re.sub(r"<h([1-4])>(.*?)</h\1>", hid, body, flags=re.S)

    # links to local files cannot work inside a PDF: show them as plain file names
    body = re.sub(r'<a href="(?!#|https?:|mailto:)[^"]*">(.*?)</a>', r'<span class="fileref">\1</span>', body, flags=re.S)

    # cover = everything before the first "LỜI CAM KẾT" heading
    m = re.search(r'<h2 id="lời-cam-kết', body)
    if m:
        body = '<div class="cover">' + body[:m.start()] + "</div>" + body[m.start():]
        body = body.replace('<h2 id="lời-cam-kết', '<h2 style="break-before:page" id="lời-cam-kết', 1)
    return f'<!doctype html><html lang="vi"><head><meta charset="utf-8"><title>{html.escape(title)}</title><style>{CSS}</style>{JS}</head><body>{body}</body></html>'


def find_browser():
    for p in BROWSERS:
        if os.path.exists(p):
            return p
    for n in ("msedge", "google-chrome", "chromium", "chrome"):
        p = shutil.which(n)
        if p:
            return p
    sys.exit("No Edge/Chrome found: install one or add it to BROWSERS in this script")


def stamp_page_numbers(pdf_in, pdf_out, label):
    from pypdf import PdfReader, PdfWriter
    from reportlab.lib.pagesizes import A4
    from reportlab.pdfgen import canvas
    from reportlab.pdfbase import pdfmetrics
    from reportlab.pdfbase.ttfonts import TTFont
    font = "Helvetica"
    for cand in (r"C:\Windows\Fonts\segoeui.ttf", r"C:\Windows\Fontsrial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if os.path.exists(cand):
            pdfmetrics.registerFont(TTFont("FooterFont", cand))   # a TrueType font so Vietnamese letters render
            font = "FooterFont"
            break
    reader = PdfReader(pdf_in)
    writer = PdfWriter()
    n = len(reader.pages)
    for i, page in enumerate(reader.pages, start=1):
        w, h = float(page.mediabox.width), float(page.mediabox.height)
        buf = io.BytesIO()
        c = canvas.Canvas(buf, pagesize=(w, h))
        c.setFont(font, 8)
        c.setFillGray(0.35)
        c.drawString(40, 22, label)
        c.drawRightString(w - 40, 22, f"{i} / {n}")
        c.save()
        buf.seek(0)
        page.merge_page(PdfReader(buf).pages[0])
        writer.add_page(page)
    writer.add_metadata({"/Title": label})
    with open(pdf_out, "wb") as f:
        writer.write(f)
    return n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("markdown")
    ap.add_argument("pdf")
    ap.add_argument("--title", default="Report")
    ap.add_argument("--keep-html", action="store_true")
    ap.add_argument("--h1-break", choices=["page", "auto"], default="page", help="start every level-1 heading on a new page (default) or let them flow")
    ap.add_argument("--wait", type=int, default=90000, help="virtual time budget in ms for KaTeX/Mermaid")
    a = ap.parse_args()

    global CSS
    if a.h1_break == "auto":
        CSS = CSS.replace("margin-top: 0; break-before: page;", "margin-top: 1.4em; break-before: auto;")
    md_path = Path(a.markdown).resolve()
    html_text = md_to_html(md_path.read_text(encoding="utf-8-sig"), a.title)
    base = md_path.parent.as_uri() + "/"
    html_text = html_text.replace("<head>", f'<head><base href="{base}">', 1)
    tmp_dir = Path(tempfile.mkdtemp(prefix="md2pdf_"))
    html_file = tmp_dir / "report.html"
    html_file.write_text(html_text, encoding="utf-8")
    raw_pdf = tmp_dir / "raw.pdf"

    cmd = [find_browser(), "--headless=new", "--disable-gpu", "--no-sandbox", "--allow-file-access-from-files",
           "--no-pdf-header-footer", f"--virtual-time-budget={a.wait}", f"--print-to-pdf={raw_pdf}",
           f"--user-data-dir={tmp_dir / 'profile'}", html_file.as_uri()]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=600)
    # Recent Edge builds return from the launcher process BEFORE the PDF is written: wait for the file to appear
    # and for its size to stop changing.
    deadline = time.time() + 300
    last = -1
    while time.time() < deadline:
        if raw_pdf.exists():
            size = raw_pdf.stat().st_size
            if size > 0 and size == last:
                break
            last = size
        time.sleep(1.5)
    if not raw_pdf.exists():
        sys.exit("the browser produced no PDF")
    pages = stamp_page_numbers(str(raw_pdf), a.pdf, a.title)
    if a.keep_html:
        shutil.copy(html_file, Path(a.pdf).with_suffix(".html"))
    shutil.rmtree(tmp_dir, ignore_errors=True)
    print(f"{a.pdf}: {pages} pages")


if __name__ == "__main__":
    main()
