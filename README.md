# tmux

Welcome to tmux!

tmux is a "terminal multiplexer", it enables a number of terminals (or windows)
to be accessed and controlled from a single terminal. tmux is intended to be a
simple, modern, BSD-licensed alternative to programs such as GNU screen.

This code can be compiled to run on OpenBSD, FreeBSD, NetBSD, Linux, OS X and Solaris.


## Instalation

### On Debian / Ubuntu

    apt install tmux

### On Redhat / Fedora

    yum install tmux

### On macOS

    brew install tmux
    
### From source

To build and install tmux from a release tarball, use:

```sh
$ ./configure && make
$ sudo make install
```

tmux can use the utempter library to update utmp(5), if it is installed - run
`configure --enable-utempter` to enable this.

To get and build the latest from version control:

```sh
$ git clone https://github.com/tmux/tmux.git
$ cd tmux
$ sh autogen.sh
$ ./configure && make
```

## Dependencies

At least a working C compiler, make, autoconf,
automake, pkg-config as well as libevent and ncurses libraries and headers.

tmux depends on [libevent](http://libevent.org) 2.x and [ncurses](http://invisible-island.net/ncurses/).

For more information see http://git-scm.com. Patches should be sent by email to
the mailing list at tmux-users@googlegroups.com or submitted through GitHub at
https://github.com/tmux/tmux/issues.

## Usage

For documentation on using tmux, see the [tmux.1 manpage](http://man7.org/linux/man-pages/man1/tmux.1.html). 
It can also be viewed from the source tree with:

```sh
$ nroff -mdoc tmux.1|less
```

See a small example configuration in `example_tmux.conf`.

## Bash completion

Check bash completion at: https://github.com/imomaliev/tmux-bash-completion

## Debugging

For debugging, running tmux with `-v` or `-vv` will generate server and client log
files in the current directory.

## Mailing lists

tmux mailing lists are available. For general discussion and bug reports:

	https://groups.google.com/forum/#!forum/tmux-users

And for Git commit emails:

	https://groups.google.com/forum/#!forum/tmux-git

Subscribe by sending an email to <tmux-users+subscribe@googlegroups.com>.

Bug reports, feature suggestions and especially code contributions are most
welcome. Please send by email to:

	tmux-users@googlegroups.com

## Licence

This file and the CHANGES, FAQ, SYNCING and TODO files are licensed under the
ISC license. All other files have a license and copyright notice at their start.

-- Nicholas Marriott <nicholas.marriott@gmail.com>
