#!/bin/bash
#
# By Victor Orlikowski. Public domain.
#
# This script maintains snapshots of each pane's
# history buffer, for each tmux session you are running.
# 
# It is intended to be run by cron, on whatever interval works
# for you.

# Maximum number of snapshots to keep.
max_backups=12
# Names of sessions you may wish to exclude from snapshotting,
# space separated.
ignore_sessions=""
# The directory into which you want your snapshots placed.
# The default is probably "good enough."
backup_dir=~/.tmux_backup/snapshot

########################################################################

# Rotate previous backups.
i=${max_backups}
while [[ ${i} != 0 ]] ; do
if [ -d ${backup_dir}.${i} ] ; then
  if [[ ${i} = ${max_backups} ]] ; then
    rm -r ${backup_dir}.${i}
  else
    mv ${backup_dir}.${i} ${backup_dir}.$((${i}+1))
  fi
fi
i=$((${i}-1))
done

if [ -d ${backup_dir} ] ; then
  mv ${backup_dir} ${backup_dir}.1
fi

## Dump hardcopy from all windows in all available tmux sessions.
unset TMUX
for session in $(tmux list-sessions | cut -d' ' -f1 | sed -e 's/:$//') ; do
  for ignore_session in ${ignore_sessions} ; do
    if [ ${session} = ${ignore_session} ] ; then
      continue 2
    fi
  done

  # Session name can contain the colon character (":").
  # This can screw up addressing of windows within tmux, since
  # target windows are specified as target-session:target-window.
  #
  # We use uuidgen to create a "safe" temporary session name,
  # which we then use to create a "detached" session that "links"
  # to the "real" session that we want to back up.
  tmpsession=$(uuidgen)
  tmux new-session -d -s "$tmpsession" -t "$session"
  HISTSIZE=$(tmux show-options -g -t "$tmpsession" | grep "history-limit" | awk '{print $2}')
  for win in $(tmux list-windows -t "$tmpsession" | grep -v "^\s" | cut -d' ' -f1 | sed -e 's/:$//'); do
    session_dir=$(echo "$session" | sed -e 's/ /_/g' | sed -e 's%/%|%g')
    win_spec="$tmpsession":"$win"

    if [ ! -d ${backup_dir}/${session_dir}/${win} ] ; then
      mkdir -p ${backup_dir}/${session_dir}/${win}
    fi

    for pane in $(tmux list-panes -t "$win_spec" | cut -d' ' -f1 | sed -e 's/:$//'); do
      pane_path=${backup_dir}/${session_dir}/${win}/${pane}
      pane_spec="$win_spec"."$pane"

      tmux capture-pane -t "$pane_spec" -S -${HISTSIZE}
      tmux save-buffer ${pane_path}

      if [ ! -s ${pane_path} ] ; then
        sleep 1
        rm ${pane_path}
      fi
    done
  done
  tmux kill-session -t "$tmpsession"

done
