# Paper build and results inclusion

- Edit docs/paper.tex with your method and results sections. The paper will automatically include docs/results.tex if present.
- Generate results: bash scripts/report.sh (produces docs/results.tex and docs/results.json)
- Build: bash docs/build.sh (requires pdflatex from TeX Live or similar)
- Output: docs/paper.pdf
