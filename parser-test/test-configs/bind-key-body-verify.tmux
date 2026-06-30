set -g status off
bind-key F1 {
    display-message "parser bind body: F1"
    run-shell "echo F1-FIRED >> /tmp/claude-1000/-home-nicholas-tmux-claude/9fb594c3-5fb9-41b2-8a5a-8d3d0b39b009/scratchpad/bindresult.txt"
}
bind-key F2 {
    run-shell "echo F2-FIRST  >> /tmp/claude-1000/-home-nicholas-tmux-claude/9fb594c3-5fb9-41b2-8a5a-8d3d0b39b009/scratchpad/bindresult.txt"
    run-shell "echo F2-SECOND >> /tmp/claude-1000/-home-nicholas-tmux-claude/9fb594c3-5fb9-41b2-8a5a-8d3d0b39b009/scratchpad/bindresult.txt"
}
