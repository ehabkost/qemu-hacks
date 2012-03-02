# Makefile for running the target-specific tests
# Run inside the target directory

# main Makefile from target
include Makefile
-include $(TARGET_PATH)/tests/Makefile

CHECKS = $(TARGET_CHECKS)

include ../tests/test-rules.mak

check-target: run-checks
