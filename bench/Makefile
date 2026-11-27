TESTS!= echo *.sh
LOGDIR=logs

.NOTPARALLEL:
.PHONY: all prepare $(TESTS)

all: prepare $(TESTS)
	@failed=0; \
	for test in $(TESTS); do \
		base=$${test##*/}; \
		log="$(LOGDIR)/$${base%.sh}.log"; \
		if [ -f "$$log" ]; then \
			failed=1; \
		fi; \
	done; \
	if [ "$$failed" -eq 0 ]; then \
		rmdir "$(LOGDIR)" 2>/dev/null || true; \
	fi; \
	exit $$failed

prepare:
	@test "$$(uname -s)" = Linux || \
	    { printf '%s\n' 'bench requires Linux' >&2; exit 1; }
	@command -v perf >/dev/null || \
	    { printf '%s\n' 'bench requires perf' >&2; exit 1; }
	@mkdir -p "$(LOGDIR)"
	@rm -f "$(LOGDIR)"/*.log "$(LOGDIR)"/*.counts
	@printf '%-28s %16s %16s %14s %8s\n' \
	    test instructions cycles bytes time

$(TESTS): prepare
	@base="$@"; base=$${base##*/}; \
	log="$(LOGDIR)/$${base%.sh}.log"; \
	counts="$(LOGDIR)/$${base%.sh}.counts"; \
	BENCH_TEST="./$@" BENCH_COUNTS="$(CURDIR)/$$counts" \
	    SHELL=/bin/sh LC_ALL=C timeout 60 script -q -e \
	    -c 'exec sh "$$BENCH_TEST"' /dev/null </dev/null \
	    >"$$log" 2>&1; \
	result=$$?; \
	case $$result in \
	0) \
		read -r instructions cycles elapsed <"$$counts" || exit 1; \
		bytes=$$(wc -c <"$$log"); \
		rm -f "$$log" "$$counts"; \
		printf '%-28s %16s %16s %14s %8s\n' \
		    "$${base%.sh}" "$$instructions" "$$cycles" "$$bytes" "$$elapsed" ;; \
	*) \
		rm -f "$$counts"; \
		printf '%-28s FAIL (%s)\n log: %s\n' "$@" "$$result" "$$log" ;; \
	esac
