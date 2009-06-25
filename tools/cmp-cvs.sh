# $Id: cmp-cvs.sh,v 1.2 2009-06-25 16:54:21 nicm Exp $

rm diff.out
touch diff.out

for i in *.[ch]; do
    diff -u $i /usr/src/usr.bin/tmux/$i >diff.tmp
    set -- `wc -l diff.tmp`
    [ $1 -eq 8 ] && continue
    echo $i
    cat diff.tmp >>diff.out
done
