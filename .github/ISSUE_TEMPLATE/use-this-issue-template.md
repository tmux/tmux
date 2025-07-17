---
name: Use this issue template
about: Please read https://github.com/tmux/tmux/blob/master/.github/CONTRIBUTING.md
title: ''
labels: ''
assignees: ''

---

### Issue description

Please read https://github.com/tmux/tmux/blob/master/.github/CONTRIBUTING.md
before opening an issue.

If you have upgraded, make sure your issue is not covered in the CHANGES file
for your version: https://raw.githubusercontent.com/tmux/tmux/master/CHANGES

Describe the problem and the steps to reproduce. Add a minimal tmux config if
necessary. Screenshots can be helpful, but no more than one or two.

Do not report bugs (crashes, incorrect behaviour) without reproducing on a tmux
built from the latest code in Git.

### Required information

Please provide the following information. These are **required**. Note that bug reports without logs may be ignored or closed without comment.

* tmux version (`tmux -V`).
* Platform (`uname -sp`).
* Terminal in use (xterm, rxvt, etc).
* $TERM *inside* tmux (`echo $TERM`).
* $TERM *outside* tmux (`echo $TERM`).
* Logs from tmux (`tmux kill-server; tmux -vv new`).
