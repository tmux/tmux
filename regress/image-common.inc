# Check server image support. Exit 101 to tell the runner to skip this test.
image_check_support()
{
	support=$("$@" display-message -p '#{image_support}') || exit 1
	case "$support" in
	1) return ;;
	0) exit 101 ;;
	*) echo "unexpected image_support: $support" >&2; exit 1 ;;
	esac
}
