# Use the configured compiler and flags for the standalone encoder check.

.PHONY: sixel-buffer-probe sixel-buffer-test

sixel-buffer-probe:
	$(CC) $(CFLAGS) $(LDFLAGS) -fsanitize=address \
	    -o "$(SIXEL_TEST_DIR)/probe" "$(SIXEL_TEST_DIR)/probe.c"

sixel-buffer-test:
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	    $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -UNDEBUG \
	    -ffunction-sections -fdata-sections -fsanitize=address \
	    -fno-omit-frame-pointer $(SIXEL_TEST_LDFLAGS) $(LDFLAGS) \
	    -o "$(SIXEL_TEST_DIR)/test" "$(srcdir)/regress/image-sixel-buffer.c" \
	    "$(srcdir)/compat/reallocarray.c"
