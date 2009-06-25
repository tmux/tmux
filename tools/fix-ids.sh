# $Id: fix-ids.sh,v 1.1 2009-06-25 16:21:32 nicm Exp $

for i in *.[ch] tmux.1; do
    (head -1 $i|grep '$OpenBSD' >/dev/null) || continue
    mv $i $i~ || exit
    sed 's/\$OpenBSD.* \$/$Id: fix-ids.sh,v 1.1 2009-06-25 16:21:32 nicm Exp $/' $i~ >$i || exit
    echo $i
done
