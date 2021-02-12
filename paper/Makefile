all:
	pdflatex -pdf paper.tex
	bibtex paper.aux
	pdflatex -pdf paper.tex

clean:
	$(RM) paper.log
	$(RM) paper.fls
	$(RM) paper.aux
	$(RM) paper.bbl
	$(RM) paper.blg
	$(RM) paper.out
	$(RM) texput.log
	$(RM) missfont.log
	$(RM) paper.blg
