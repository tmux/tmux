This directory contains support libraries for other Go code in this repository
(e.g. in the top level `cmd`, `lang` and `lib` directories). The difference is
that [internal](https://golang.org/cmd/go/#hdr-Internal_Directories) packages
are private implementation details that cannot be imported from outside of
those directories.
