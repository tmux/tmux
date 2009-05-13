# $Id: CMakeLists.txt,v 1.1 2009-05-13 23:32:21 nicm Exp $

cmake_minimum_required(VERSION 2.4)

# Project name and version.
project(TMUX)
set(VERSION 0.9)
set(DEBUG 1)

# Compiler flags. Mostly disabled unless a debug build.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DBUILD=\\\"${VERSION}\\\"")
if(DEFINED DEBUG)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -W -Wno-long-long")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wnested-externs -Wformat=2")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes -Wstrict-prototypes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-declarations -Wwrite-strings")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow -Wpointer-arith -Wcast-qual")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsign-compare -Wmissing-prototypes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-prototypes -Wmissing-declarations")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wwrite-strings -Wundef -Winline")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wbad-function-cast -Wcast-align")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG -g -ggdb")
endif(DEFINED DEBUG)

# glibc follows the principle of most surprise, so feed it some extra flags. 
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE -D_POSIX_SOURCE")
endif("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")

# Add source directory as include path.
include_directories(${TMUX_SOURCE_DIR})

# Initialise libraries with ncurses.
set(NEEDED_LIBRARIES ncurses)

# Platform in lowercase. Used later on for some filenames.
string(TOLOWER ${CMAKE_SYSTEM_NAME} LOWER_NAME)

# Build list of source files and sort out the osdep-* file choice. This is
# a bit if a hack and needs to be changed...
file(GLOB SOURCE_FILES "*.c")
string(REGEX REPLACE 
	"${TMUX_SOURCE_DIR}/osdep-[a-z]*.c;" "" SOURCE_FILES "${SOURCE_FILES}")
if(EXISTS "osdep-${LOWER_NAME}.c")
	set(SOURCE_FILES "${SOURCE_FILES};osdep-${LOWER_NAME}.c")
else(EXISTS "osdep-${LOWER_NAME}.c")
	set(SOURCE_FILES "${SOURCE_FILES};osdep-unknown.c")
endif(EXISTS "osdep-${LOWER_NAME}.c")

# This crap is needed for checks done later...
include(CheckFunctionExists)
include(CheckVariableExists)
include(CheckIncludeFiles)
include(CheckSymbolExists) 
include(CheckLibraryExists)

# Deal with some further glibc stupidity and figure out if -lcrypt is needed.
check_function_exists(crypt HAVE_CRYPT)
if(NOT HAVE_CRYPT)
	set(NEEDED_LIBRARIES "${NEEDED_LIBRARIES};crypt")
endif(NOT HAVE_CRYPT)

# See if forkpty exists and which header it is in.
check_library_exists(util forkpty "" HAVE_FORKPTY) 
if(HAVE_FORKPTY)
	set(NEEDED_LIBRARIES "${NEEDED_LIBRARIES};util")

	check_include_files(util.h HAVE_UTIL_H)
	if(NOT HAVE_UTIL_H)
		check_include_files(libutil.h HAVE_LIBUTIL_H)
		if(NOT HAVE_LIBUTIL_H)
			check_include_files(pty.h HAVE_PTY_H)
		endif(NOT HAVE_LIBUTIL_H)
	endif(NOT HAVE_UTIL_H)
else(HAVE_FORKPTY)
	# No forkpty. Try and get a compat/forkpty-* file.
	set(SOURCE_FILES "${SOURCE_FILES};compat/forkpty-${LOWER_NAME}.c")	
endif(HAVE_FORKPTY)

# Look for __progname.
check_variable_exists(__progname HAVE_PROGNAME)

# Check for paths.h.
check_include_files(paths.h HAVE_PATHS_H)

# Check for sys/{tree,queue}.h.
check_include_files(sys/tree.h HAVE_TREE_H)
check_include_files(sys/queue.h HAVE_QUEUE_H)

# queue.h without TAILQ_REPLACE is no use at all.
check_symbol_exists(TAILQ_REPLACE sys/queue.h HAVE_TAILQ_REPLACE)
if(NOT HAVE_TAILQ_REPLACE)
	set(HAVE_QUEUE_H 0)
endif(NOT HAVE_TAILQ_REPLACE)

# Check for setproctitle.
check_function_exists(setproctitle HAVE_SETPROCTITLE)

# Check for vsyslog.
check_function_exists(vsyslog HAVE_VSYSLOG)

# Check for asprintf.
check_function_exists(asprintf HAVE_ASPRINTF)
if(NOT HAVE_ASPRINTF)
       set(SOURCE_FILES "${SOURCE_FILES};compat/asprintf.c")
endif(NOT HAVE_ASPRINTF)

# Check for daemon.
check_function_exists(daemon HAVE_DAEMON)
if(NOT HAVE_DAEMON)
       set(SOURCE_FILES "${SOURCE_FILES};compat/daemon.c")
endif(NOT HAVE_DAEMON)

# Check for a getopt with optreset.
check_symbol_exists(getopt getopt.h HAVE_GETOPT)
if(HAVE_GETOPT)
	check_symbol_exists(optreset getopt.h HAVE_OPTRESET)
	if(NOT HAVE_OPTRESET)
	       # This does double duty on Linux - glibc getopt is broken (not
	       # POSIX-compliant, and no sane way to make it so at build time),
	       # but happily it also lacks BSD optreset, so this check will
	       # catch it.
	       set(HAVE_GETOPT 0)
	endif(NOT HAVE_OPTRESET)
endif(HAVE_GETOPT)
if(NOT HAVE_GETOPT)
       set(SOURCE_FILES "${SOURCE_FILES};compat/getopt.c")
endif(NOT HAVE_GETOPT)

# Check for fgetln.
check_function_exists(fgetln HAVE_FGETLN)
if(NOT HAVE_FGETLN)
       set(SOURCE_FILES "${SOURCE_FILES};compat/fgetln.c")
endif(NOT HAVE_FGETLN)

# Check for strlcat.
check_function_exists(strlcat HAVE_STRLCAT)
if(NOT HAVE_STRLCAT)
       set(SOURCE_FILES "${SOURCE_FILES};compat/strlcat.c")
endif(NOT HAVE_STRLCAT)

# Check for strlcpy.
check_function_exists(strlcpy HAVE_STRLCPY)
if(NOT HAVE_STRLCPY)
       set(SOURCE_FILES "${SOURCE_FILES};compat/strlcpy.c")
endif(NOT HAVE_STRLCPY)

# Check for strtonum.
check_function_exists(strtonum HAVE_STRTONUM)
if(NOT HAVE_STRTONUM)
       set(SOURCE_FILES "${SOURCE_FILES};compat/strtonum.c")
endif(NOT HAVE_STRTONUM)

# Point install locations.
INSTALL(PROGRAMS tmux DESTINATION bin)
INSTALL(FILES tmux.1 DESTINATION man/man1)

# Sort out the config.h file and finish everything off.
configure_file(config.h.in config.h)
add_executable(tmux ${SOURCE_FILES})
target_link_libraries(tmux ${NEEDED_LIBRARIES})
