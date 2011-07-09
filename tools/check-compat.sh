# $Id$

grep "#include" compat.h|while read line; do
     grep "$line" *.[ch] compat/*.[ch]
done
