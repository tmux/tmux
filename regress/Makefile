TESTS!= echo *.sh

.PHONY: all $(TESTS)
.NOTPARALLEL: all $(TESTS)

all:
	@failed=0; failures=; \
	for test in $(TESTS); do \
		printf '%-40s ' "$$test"; \
		start=$$(date +%s); \
		ASAN_OPTIONS="abort_on_error=1:detect_leaks=0:$$ASAN_OPTIONS"; \
		env -i LC_CTYPE=C.UTF-8 ASAN_OPTIONS="$$ASAN_OPTIONS" \
		    sh "$$test" >/dev/null 2>&1; \
		if [ $$? -eq 0 ]; then \
			end=$$(date +%s); \
			echo "PASS ($$((end - start))s)"; \
		else \
			end=$$(date +%s); \
			echo "FAIL ($$((end - start))s)"; \
			failed=1; \
			failures="$$failures $$test"; \
		fi; \
		sleep 1; \
	done; \
	if [ "$$failed" -ne 0 ]; then \
		echo; \
		echo "failures:"; \
		for test in $$failures; do \
			echo " $$test"; \
		done; \
	fi; \
	exit $$failed

$(TESTS):
	sh $@
	sleep 1
