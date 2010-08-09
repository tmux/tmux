# START tmux completion
# This file is in the public domain
# See: http://www.debian-administration.org/articles/317 for how to write more.
# Usage: Put "source bash_completion_tmux.sh" into your .bashrc
_tmux() 
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    
    opts=" \
    attach-session \
    bind-key \
    break-pane \
    capture-pane \
    choose-client \
    choose-session \
    choose-window \
    clear-history \
    clock-mode \
    command-prompt \
    confirm-before \
    copy-buffer \
    copy-mode \
    delete-buffer \
    detach-client \
    display-message \
    display-panes \
    down-pane \
    find-window \
    has-session \
    if-shell \
    join-pane \
    kill-pane \
    kill-server \
    kill-session \
    kill-window \
    last-window \
    link-window \
    list-buffers \
    list-clients \
    list-commands \
    list-keys \
    list-panes \
    list-sessions \
    list-windows \
    load-buffer \
    lock-client \
    lock-server \
    lock-session \
    move-window \
    new-session \
    new-window \
    next-layout \
    next-window \
    paste-buffer \
    pipe-pane \
    previous-layout \
    previous-window \
    refresh-client \
    rename-session \
    rename-window \
    resize-pane \
    respawn-window \
    rotate-window \
    run-shell \
    save-buffer \
    select-layout \
    select-pane \
    select-prompt \
    select-window \
    send-keys \
    send-prefix \
    server-info \
    set-buffer \
    set-environment \
    set-option \
    set-window-option \
    show-buffer \
    show-environment \
    show-messages \
    show-options \
    show-window-options \
    source-file \
    split-window \
    start-server \
    suspend-client \
    swap-pane \
    swap-window \
    switch-client \
    unbind-key \
    unlink-window \
    up-pane"

    COMPREPLY=($(compgen -W "${opts}" -- ${cur}))  
    return 0

}
complete -F _tmux tmux

# END tmux completion


 	  	 
