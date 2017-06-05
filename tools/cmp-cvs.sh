#!/bin/ksh

rm diff.out
touch diff.out

for i in *.[ch]; do
    diff -u -I'\$OpenBSD' $i ../../OpenBSD/tmux/$i >diff.tmp
    set -- `wc -l diff.tmp`
    [ $1 -eq 8 ] && continue
    echo $i
    cat diff.tmp >>diff.out
done
