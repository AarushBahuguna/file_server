#!/bin/bash
# Assembles docs/report/*.md (+ Aarush's deadlock section) into docs/REPORT.md, REPORT.docx and REPORT.html.
# Needs pandoc for docx/html:  sudo apt install pandoc
cd "$(dirname "$0")/.." || exit 1
R=docs/report
FILES=("$R/00_front.md" "$R/01_introduction.md" "$R/02_architecture.md" "$R/03_storage.md" "$R/04_cli_ipc.md" "$R/05_server_sync_cache.md")
if [ -f docs/deadlock_scheduling_section.md ]; then FILES+=("docs/deadlock_scheduling_section.md"); else FILES+=("$R/06_deadlock_scheduling.md"); fi
FILES+=("$R/07_testing_results.md" "$R/08_conclusion.md")

missing=0
for f in "${FILES[@]}"; do [ -f "$f" ] || { echo "missing: $f"; missing=1; }; done
[ $missing -eq 0 ] || exit 1

TODOS=$(grep -n 'TODO(' "${FILES[@]}" 2>/dev/null)
if [ -n "$TODOS" ]; then
    echo "WARNING: these parts of the report are still unwritten:"
    echo "$TODOS" | sed 's/^/   /'
    echo
fi

: > docs/REPORT.md
for f in "${FILES[@]}"; do cat "$f" >> docs/REPORT.md; printf '\n\n' >> docs/REPORT.md; done
echo "wrote docs/REPORT.md ($(wc -w < docs/REPORT.md) words)"

if command -v pandoc >/dev/null 2>&1; then
    pandoc docs/REPORT.md -o docs/REPORT.docx --toc --number-sections && echo "wrote docs/REPORT.docx"
    pandoc docs/REPORT.md -o docs/REPORT.html --standalone --toc --number-sections && echo "wrote docs/REPORT.html"
else
    echo "pandoc not installed: only REPORT.md was made (sudo apt install pandoc)"
fi
