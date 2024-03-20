# Issue description | DD/MM/YY 

## Checklist Before Opening an Issue
- [ ] Read https://github.com/tmux/tmux/blob/master/.github/CONTRIBUTING.md
- [ ] Your Issue is specific and actionable.
- [ ] `$TERM` is appropriate (`echo $TERM` inside tmux).
- [ ] Issue persists in the latest tmux release or Git master build.
- [ ] Not covered in [tmux manual](https://man.openbsd.org/tmux.1) or [CHANGES file](https://raw.githubusercontent.com/tmux/tmux/master/CHANGES).
- [ ] Issue is not previously reported.
- [ ] Do not report bugs (crashes, incorrect behaviour) without reproducing on a tmux built from the latest code in Git.


## Information to Include
- tmux version (`tmux -V`).
- system version (`uname -sp`).
- $TERM inside and outside of tmux (`echo $TERM`).
- Logs from tmux (`tmux kill-server; tmux -vv new`).
- Minimal tmux config (if relevant).
- Attach tmux logs with `-vv` and up to two screenshots.

## Steps to Reproduce
1. Step 1
2. Step 2
3. Step 3

## Expected Behavior
Describe what you expected to happen.

## Actual Behavior
Describe what actually happened.

## Additional Information
Add any other context or screenshots about the issue here.
