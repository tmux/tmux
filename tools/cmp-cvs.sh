# $Id: cmp-cvs.sh,v 1.1 2009-06-25 16:49:22 nicm Exp $

for i in *.[ch]; do
    diff -u $i /usr/src/usr.bin/tmux/$i
done
