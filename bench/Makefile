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
	@mkdir -p "$(LOGDIR)"
	@rm -f "$(LOGDIR)"/*.log "$(LOGDIR)"/*.time

$(TESTS): prepare
	@base="$@"; base=$${base##*/}; \
	log="$(LOGDIR)/$${base%.sh}.log"; \
	timing="$(LOGDIR)/$${base%.sh}.time"; \
	BENCH_TEST="./$@" BENCH_TIME="$(CURDIR)/$$timing" SHELL=/bin/sh LC_ALL=C \
	    timeout 60 script -q -e \
	    -c 'exec sh "$$BENCH_TEST"' /dev/null </dev/null \
	    >"$$log" 2>&1; \
	result=$$?; \
	case $$result in \
	0) \
		seconds=$$(cat "$$timing"); \
		bytes=$$(wc -c <"$$log"); \
		rm -f "$$log" "$$timing"; \
		printf '%-28s %10s %14s\n' "$${base%.sh}" "$$seconds" "$$bytes" ;; \
	*) \
		rm -f "$$timing"; \
		printf '%-28s FAIL (%s)\n log: %s\n' "$@" "$$result" "$$log" ;; \
	esac
