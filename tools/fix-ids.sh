# $Id: fix-ids.sh,v 1.3 2009-07-01 19:03:34 nicm Exp $

for i in *.[ch] tmux.1; do
    (head -1 $i|grep '$OpenBSD' >/dev/null) || continue
    mv $i $i~ || exit
    sed 's/\$OpenBSD.* \$/$\Id$/' $i~ >$i || exit
    echo $i
done
