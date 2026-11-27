# A pane waits for its test to start each input file. The cursor report proves
# tmux parsed the complete file before we signal completion, even off-screen.
set -e
binary=$1
socket=$2
pane=$3
directory=$4
phases=$5
stty raw -echo

phase=0
while [ "$phase" -lt "$phases" ]; do
	"$binary" -S "$socket" wait-for -S "ready-$pane-$phase"
	"$binary" -S "$socket" wait-for "start-$pane-$phase"
	cat "$directory/input-$pane-$phase"
	printf '\033[0m\033[r\033[1;1HEND %02d %02d\033[K\033[6n' "$pane" "$phase"
	reply=$(dd bs=1 count=7 2>/dev/null)
	[ "$reply" = "$(printf '\033[1;10R')" ]
	"$binary" -S "$socket" wait-for -S "done-$pane-$phase"
	phase=$((phase + 1))
done
# Keep the model intact until the attached client has finished.
exec sleep 3600
