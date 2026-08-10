# ============================================================================
# Makefile — VM Example Suite
# ============================================================================
#
# Build all examples:
#     make
#
# Run all examples:
#     make run
#
# Build + run a single example (e.g.):
#     make basic
#     ./basic
#
# Clean:
#     make clean
#
# Enable VM instruction tracing (prints each opcode to stderr):
#     make VM_DEBUG=1
# ============================================================================

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -Wno-unused-function \
          -I.. -I.
LDFLAGS = -lm

VM_SRC  = ../vm.c

ifdef VM_DEBUG
CFLAGS += -DVM_DEBUG
endif

# -----------------------------------------------------------------------
# List of example programs (add new ones here)
# -----------------------------------------------------------------------
EXAMPLES = \
    basic          \
    arithmetic     \
    floating_point \
    comparison     \
    branching      \
    bitwise        \
    shifts         \
    conversions    \
    memory         \
    pointers       \
    native_functions \
    error_handling \
    loops          \
    fibonacci      \
    factorial      \
    complex        \
    file_execution \
    assembler_disassembler_demo \
    unsigned_ops   \
    single_step    \
    coroutines     \
    benchmark      \
    debugger_profiler

# Tools (separate from examples — vm_run is the standalone bytecode runner)
TOOLS = vm_run

# On Windows (MinGW/MSYS2), executables have .exe suffix.
# On Linux/macOS, no suffix.
ifeq ($(OS), Windows_NT)
    EXE_EXT = .exe
else
    EXE_EXT =
endif

BINS  = $(addsuffix $(EXE_EXT), $(EXAMPLES))
TBINS = $(addsuffix $(EXE_EXT), $(TOOLS))

# -----------------------------------------------------------------------
# Default target — build examples + tools
# -----------------------------------------------------------------------
.PHONY: all
all: $(BINS) $(TBINS)

# -----------------------------------------------------------------------
# Pattern rule: compile each example
# -----------------------------------------------------------------------
%$(EXE_EXT): %.c ../vm_builder.h ../vm_loader.h ../vm_assembler.h ../vm_disassembler.h $(VM_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(VM_SRC) $(LDFLAGS)

# vm_run: the standalone bytecode runner (no -lm needed)
vm_run$(EXE_EXT): vm_run.c ../vm_loader.h $(VM_SRC)
	$(CC) $(CFLAGS) -o $@ vm_run.c $(VM_SRC)

# -----------------------------------------------------------------------
# Run all examples
# -----------------------------------------------------------------------
.PHONY: run
run: all
	@for prog in $(EXAMPLES); do \
	    echo ""; \
	    echo "============================================================"; \
	    echo "  $$prog"; \
	    echo "============================================================"; \
	    ./$$prog$(EXE_EXT); \
	done

# Install vm_run to current directory (already built by 'all')
.PHONY: tools
tools: $(TBINS)

# -----------------------------------------------------------------------
# Individual convenience targets (build + run)
# -----------------------------------------------------------------------
.PHONY: $(EXAMPLES) $(TOOLS)
$(EXAMPLES): %: %$(EXE_EXT)
	@echo ""
	@echo "=== $@ ==="
	./$<

$(TOOLS): %: %$(EXE_EXT)
	@echo "Built: $<"

# -----------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------
.PHONY: clean
clean:
	rm -f $(BINS) $(TBINS) *.cvmb
