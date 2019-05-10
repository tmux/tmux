# Contributing to the tmux project

Bug reports, feature suggestions and especially code contributions are most
welcome. Please send by email to:

tmux-users@googlegroups.com

Alternatively, contributions through GitHub issues or pull requests are accepted.

When reporting issues:

YOU MUST INCLUDE THE TMUX VERSION

DO NOT OPEN AN ISSUE THAT DOES NOT MENTION THE TMUX VERSION

Please also include:

- your platform (Linux, OS X, or whatever);
- a brief description of the problem with steps to reproduce;
- a minimal tmux config, if you can't reproduce without a config;
- your terminal, and $TERM inside and outside of tmux;
- logs from tmux (see below);
- at most one or two screenshots, if helpful.

This should include at least the output of:

~~~bash
uname -sp && tmux -V && echo $TERM
~~~

Please do not report bugs (crashes, incorrect behaviour) without reproducing on
a tmux built from the latest code in Git.

Note that TERM inside tmux must be a variant of screen or tmux (for example:
screen or screen-256color, tmux or tmux-256color). Please ensure this is the
case before opening an issue.

To run tmux without a config and get logs, run:

~~~bash
tmux -Ltest kill-server
tmux -vv -Ltest -f/dev/null new
~~~

Then reproduce the problem, exit tmux, and attach the `tmux-server-*.log` file
from the current directory to the issue.
