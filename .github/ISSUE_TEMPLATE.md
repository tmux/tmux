# Issue description

Please describe the problem and the steps to reproduce.
Add a minimal tmux config if necessary. Screenshots can be very helpful too.

Please do not report bugs (crashes, incorrect behaviour) without reproducing on
a tmux built from the latest code in Git.

To run tmux without a config and get logs, run:

    tmux -Ltest kill-server
    tmux -vv -Ltest -f/dev/null new

Then reproduce the problem, exit tmux, and attach the `tmux-server-*.log` file
from the current directory to the issue.

# Information

Please fill the following information. It is very important that at least the
tmux version is mentioned.

* TMUX VERSION (`tmux -V`):
* Platform (`uname -sp`):
* Your terminal, and $TERM inside and outside of tmux (`echo $TERM`):
* Logs from tmux
To run tmux without a config and get logs, run:

~~~bash
tmux -Ltest kill-server
tmux -vv -Ltest -f/dev/null new
~~~

You can then add the logs from the current directory to the issue.
