# START tmux completion
# This file is in the public domain
# See: http://www.debian-administration.org/articles/317 for how to write more.
# Usage: Put "source bash_completion_tmux.sh" into your .bashrc
_tmux()
{
	local cur prev words cword;
	_init_completion || return;
	if [[ $cword -eq 1 ]]; then
		COMPREPLY=($( compgen -W "$(tmux list-commands | cut -d' ' -f1)" -- "$cur" ));
		return 0
	fi
}
complete -F _tmux tmux

# END tmux completion

