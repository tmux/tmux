# $Id: check-compat.sh,v 1.1 2010-10-24 00:42:04 tcunha Exp $

grep "#include" compat.h|while read line; do
     grep "$line" *.[ch] compat/*.[ch]
done
