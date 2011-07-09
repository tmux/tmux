# $Id$

for i in *.[ch] tmux.1; do
    (head -1 $i|grep '$OpenBSD' >/dev/null) || continue
    mv $i $i~ || exit
    sed 's/\$OpenBSD.* \$/$\Id$/' $i~ >$i || exit
    echo $i
done
