# kill-server may return before the server has exited.
stop_servers()
{
	pid1=$($TMUX display-message -p '#{pid}' 2>/dev/null)
	pid2=$($TMUX2 display-message -p '#{pid}' 2>/dev/null)

	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1

	for pid in "$pid1" "$pid2"; do
		[ -n "$pid" ] || continue
		i=0
		while kill -0 "$pid" 2>/dev/null; do
			if [ "$i" -eq 50 ]; then
				echo "server $pid did not exit" >&2
				exit 1
			fi
			i=$((i + 1))
			sleep 0.1
		done
	done
}
