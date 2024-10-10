#!/bin/bash

set -o pipefail

function output_help() {
	echo &&
	echo "'build': build repo for container, build products persistently located in host repo" 1>&2 &&
	echo &&
	echo "'interactive' (default): automatically calls 'build', afterwards open interactive bash to the container. From there, you can use the built tmux executable from within the container. It's automatically put in the PATH, so you can simply write 'tmux ...' to use the built executable." 1>&2
	echo &&
	echo "'cmd': run the argument that follows as a command in bash from within the container. Automatically builds tmux for the container, just like 'interactive'."
}

function assert_arg_count() {
	if [ "$1" -eq "$2" ]
	then
		return
	fi

	echo "$0: ERROR: invalid number of arguments for subcommand" 1>&2 && output_help
	exit 1
}

function do_in_new_container() {
	image_name="$(pwd | sed 's/\///' | sed 's/\//__/g')" &&
	docker build -t "$image_name" . &&
	docker run -it --rm -v .:/tmux "$image_name" "$@"
}

function do_subcommand() {
	subcommand=$1
	shift
	case "$subcommand" in

		build)
			assert_arg_count $# 0 &&

			do_in_new_container bash -c './autogen.sh && ./configure && make' ||
			exit 1
		;;

		interactive|'')
			assert_arg_count $# 0 &&

			# prerequisites
			do_subcommand build &&

			do_in_new_container bash -c 'ln -s /tmux/tmux /tmux-bin/tmux && bash' ||

			exit 1
		;;

		cmd)
			# NOTE: I want to be able to use more than 1 arg for this, but bash -c doesn't like that for some reason.
			assert_arg_count $# 1 &&

			# prerequisites
			do_subcommand build &&

			do_in_new_container bash -c "$1" ||

			exit 1
		;;

		*)
			echo "$0: ERROR: subcommand invalid" 1>&2 && output_help
			exit 1
		;;

	esac
}

do_subcommand "$@"
