run-checks: $(CHECKS)
	@for test in $^; do \
	    echo "Running '$$test'..."; \
	    ./$$test || exit $?; \
	done

