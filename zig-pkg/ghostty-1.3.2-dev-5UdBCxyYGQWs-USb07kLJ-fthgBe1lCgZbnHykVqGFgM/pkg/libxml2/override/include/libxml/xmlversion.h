// This recreates parts of the generated libxml/xmlversion.h.in that we need
// to build libxml2 without actually templating the header file. We define most
// of the defines in that file using flags to the compiler in libxml2.zig

#ifndef __XML_VERSION_H__
#define __XML_VERSION_H__

#include <libxml/xmlexports.h>

// We are not GCC.
#define XML_IGNORE_FPTR_CAST_WARNINGS
#define XML_POP_WARNINGS
#define LIBXML_ATTR_FORMAT(fmt,args)
#define LIBXML_ATTR_ALLOC_SIZE(x)
#define ATTRIBUTE_UNUSED
#define XML_DEPRECATED

#endif
