TESTS!= echo *.sh
LOGDIR=logs

.PHONY: all
.NOTPARALLEL: all

all:
	@mkdir -p "$(LOGDIR)"; \
	rm -f "$(LOGDIR)"/*.log; \
	failed=0; failures=; \
	for test in $(TESTS); do \
		base=$${test##*/}; \
		log="$(LOGDIR)/$${base%.sh}.log"; \
		rm -f "$$log"; \
		printf '%-40s ' "$$test"; \
		start=$$(date +%s); \
		env -i LC_CTYPE=C.UTF-8 MallocNanoZone=0 \
		    sh -x "$$test" >"$$log" 2>&1; \
		if [ $$? -eq 0 ]; then \
			end=$$(date +%s); \
			rm -f "$$log"; \
			echo "PASS ($$((end - start))s)"; \
		else \
			end=$$(date +%s); \
			echo "FAIL ($$((end - start))s)"; \
			echo " log: $$log"; \
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
	else \
		rmdir "$(LOGDIR)" 2>/dev/null || true; \
	fi; \
	exit $$failed
