### Issue description

Please read https://github.com/tmux/tmux/blob/master/.github/CONTRIBUTING.md
before opening an issue.

Describe the problem and the steps to reproduce. Add a minimal tmux config if
necessary. Screenshots can be helpful, but no more than one or two.

Do not report bugs (crashes, incorrect behaviour) without reproducing on a tmux
built from the latest code in Git.

### Required information

Please provide the following information:

* tmux version (`tmux -V`).
* Platform (`uname -sp`).
* $TERM inside and outside of tmux (`echo $TERM`).
* Logs from tmux (`tmux kill-server; tmux -vv new`).
