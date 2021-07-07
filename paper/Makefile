paper:
	pdflatex -pdf paper.tex
	bibtex paper.aux
	pdflatex -pdf paper.tex
	pdflatex -pdf paper.tex

short_paper:
	pdflatex -pdf short_paper.tex
	bibtex short_paper.aux
	pdflatex -pdf short_paper.tex
	pdflatex -pdf short_paper.tex

clean:
	$(RM) paper.log
	$(RM) paper.fls
	$(RM) paper.aux
	$(RM) paper.bbl
	$(RM) paper.blg
	$(RM) paper.out
	$(RM) paper.blg
	$(RM) short_paper.log
	$(RM) short_paper.fls
	$(RM) short_paper.aux
	$(RM) short_paper.bbl
	$(RM) short_paper.blg
	$(RM) short_paper.out
	$(RM) short_paper.blg
	$(RM) texput.log
	$(RM) missfont.log
