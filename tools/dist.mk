# $Id: dist.mk,v 1.5 2009-09-20 18:54:21 nicm Exp $

VERSION= 1.0

DISTDIR= tmux-${VERSION}
DISTFILES= *.[ch] Makefile GNUmakefile configure tmux.1 \
	NOTES TODO CHANGES FAQ \
	`find examples compat -type f -and ! -path '*CVS*'`

dist:          
		(./configure &&	make clean-all)
		grep '^#FDEBUG=' Makefile
		grep '^#FDEBUG=' GNUmakefile
		[ "`(grep '^VERSION' Makefile; grep '^VERSION' GNUmakefile)| \
		        uniq -u`" = "" ]
		chmod +x configure
		tar -zc \
		        -s '/.*/${DISTDIR}\/\0/' \
		        -f ${DISTDIR}.tar.gz ${DISTFILES}

upload-index.html: update-index.html
		scp www/index.html www/images/*.png \
		        nicm,tmux@web.sf.net:/home/groups/t/tm/tmux/htdocs
		rm -f www/index.html www/images/small-*

update-index.html:
		(cd www/images && \
		        rm -f small-* && \
		        for i in *.png; do \
		        convert "$$i" -resize 200x150 "small-$$i"; \
		        done \
		)
		sed "s/%%VERSION%%/${VERSION}/g" www/index.html.in >www/index.html
