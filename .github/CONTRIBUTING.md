## What should I do before opening an issue?

Before opening an issue, please ensure that:

- Your problem is a specific problem or question or suggestion, not a general
  complaint.

- `$TERM` inside tmux is screen, screen-256color, tmux or tmux-256color. Check
  by running `echo $TERM` inside tmux.

- You can reproduce the problem with the latest tmux release, or a build from
  Git master.

- Your question or issue is not covered [in the
  manual](https://man.openbsd.org/tmux.1) (run `man tmux`).

- Your problem is not mentioned in [the CHANGES
  file](https://raw.githubusercontent.com/tmux/tmux/master/CHANGES).

- Nobody else has opened the same issue recently.

## What should I include in an issue?

Please include the output of:

~~~bash
uname -sp && tmux -V && echo $TERM
~~~

Also include:

- Your platform (Linux, macOS, or whatever).

- A brief description of the problem with steps to reproduce.

- A minimal tmux config, if you can't reproduce without a config.

- Your terminal, and `$TERM` inside and outside of tmux.

- Logs from tmux (see below). Please attach logs to the issue directly rather
  than using a download site or pastebin. Put in a zip file if necessary.

- At most one or two screenshots, if helpful.

## How do I test without a .tmux.conf?

Run a separate tmux server with `-f/dev/null` to skip loading `.tmux.conf`:

~~~bash
tmux -Ltest kill-server
tmux -Ltest -f/dev/null new
~~~

## How do I get logs from tmux?

Add `-vv` to tmux to create three log files in the current directory. If you can
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

- `tmux-server*.log`: server log file.

- `tmux-client*.log`: client log file.

- `tmux-out*.log`: output log file.

Please attach the log files to your issue.
