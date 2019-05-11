# What should I do before opening an issue?

Before opening an issue, please ensure that:

- TERM inside tmux is screen, screen-256color, tmux or tmux-256color. Check
  by running echo $TERM inside tmux.

- You can reproduce the problem with the latest tmux release, or a build from
  Git master.

- Your question or issue is not covered in the manual (run man tmux).

- Nobody else has opened the same issue recently.

# What should I include in an isue?

Please include the output of:

~~~bash
uname -sp && tmux -V && echo $TERM
~~~

Also include:

- Your platform (Linux, OS X, or whatever).

- A brief description of the problem with steps to reproduce.

- A minimal tmux config, if you can't reproduce without a config.

- Your terminal, and $TERM inside and outside of tmux.

- Logs from tmux (see below).

- At most one or two screenshots, if helpful.

# How do I test without a .tmux.conf?

Run a separate tmux server with -f/dev/null to skip loading .tmux.conf:

~~~bash
tmux -Ltest kill-server
tmux -Ltest -f/dev/null new
~~~

# How do I get logs from tmux?

Add -vv to tmux to create three log files in the current directory. If you can
reproduce without a configuration file:

~~~bash
tmux -Ltest kill-server
tmux -vv -Ltest -f/dev/null new
~~~

Or if you need your configuration:

~~~base
tmux kill-server
tmux -vv new
~~~

The log files are:

- tmux-server*.log: server log file.

- tmux-client*.log: client log file.

- tmux-out*.log: output log file.

To run tmux without a config and get logs, run:

~~~bash
tmux -Ltest kill-server
tmux -vv -Ltest -f/dev/null new
~~~

Then reproduce the problem, exit tmux, and attach the `tmux-server-*.log` file
from the current directory to the issue.
