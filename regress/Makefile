TESTS!= echo *.sh
LOGDIR=logs

.PHONY: all prepare $(TESTS)

all: prepare $(TESTS)
	@failed=0; failures=; \
	for test in $(TESTS); do \
		base=$${test##*/}; \
		log="$(LOGDIR)/$${base%.sh}.log"; \
		if [ -f "$$log" ]; then \
			failed=1; \
			failures="$$failures $$test"; \
		fi; \
	done; \
	if [ "$$failed" -ne 0 ]; then \
		echo; \
		echo "failures:"; \
		for test in $$failures; do \
			echo " $$test"; \
		done; \
	else \
		rmdir "$(LOGDIR)" 2>/dev/null || true; \
	fi; \
	exit $$failed

prepare:
	@mkdir -p "$(LOGDIR)"
	@rm -f "$(LOGDIR)"/*.log

$(TESTS): prepare
	@base="$@"; base=$${base##*/}; \
	log="$(LOGDIR)/$${base%.sh}.log"; \
	start=$$(date +%s); \
	env -i LC_CTYPE=C.UTF-8 MallocNanoZone=0 \
	    sh -x "$@" >"$$log" 2>&1; \
	result=$$?; \
	end=$$(date +%s); \
	case $$result in \
	0) \
		rm -f "$$log"; \
		printf '%-40s PASS (%ss)\n' "$@" "$$((end - start))" ;; \
	101) \
		rm -f "$$log"; \
		printf '%-40s SKIP (%ss)\n' "$@" "$$((end - start))" ;; \
	*) \
		printf '%-40s FAIL (%ss)\n log: %s\n' \
		    "$@" "$$((end - start))" "$$log" ;; \
	esac
