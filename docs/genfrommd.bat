p4 edit HsaPrmConformance.*
pandoc -o HsaPrmConformance.html --toc --toc-depth=2 -f markdown -t html --template=stylesheet.html --self-contained --normalize -smart HsaPrmConformance.md
pandoc -o HsaPrmConformance.pdf  --toc --toc-depth=2 HsaPrmConformance.md
