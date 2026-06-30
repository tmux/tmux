# Failure scope test.
# Expected with sequence skipping: bad command fails, following command in same sequence is skipped,
# then next top-level sequence still runs.
display-message "parser failure: before bad command" ; no-such-tmux-command ; display-message "parser failure: should be skipped"
display-message "parser failure: next sequence should still run"
