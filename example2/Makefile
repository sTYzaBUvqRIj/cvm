# ============================================================================
# Makefile — Example 2: CVM Assembly (.cvma) to Bytecode (.cvmb) Workflow
# ============================================================================
#
# Build compiler (cvma2cvmb) and assemble all .cvma files into .cvmb:
#     make
#
# Assemble and execute all .cvmb bytecode files via vm_run:
#     make run
#
# Clean generated files:
#     make clean
# ============================================================================

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -Wno-unused-function -I..
LDFLAGS = -lm

VM_SRC  = ../vm.c
RUNNER  = ../example/vm_run

# On Windows (MinGW/MSYS2), executables have .exe suffix.
ifeq ($(OS), Windows_NT)
    EXE_EXT = .exe
else
    EXE_EXT =
endif

ASSEMBLER_BIN = cvma2cvmb$(EXE_EXT)

CVMA_FILES = $(wildcard *.cvma)
CVMB_FILES = $(patsubst %.cvma,%.cvmb,$(CVMA_FILES))

.PHONY: all
all: $(ASSEMBLER_BIN) $(CVMB_FILES)

# Compile cvma2cvmb CLI tool
$(ASSEMBLER_BIN): cvma2cvmb.c ../vm_assembler.h ../vm_disassembler.h ../vm_loader.h $(VM_SRC)
	$(CC) $(CFLAGS) -o $@ cvma2cvmb.c $(VM_SRC) $(LDFLAGS)

# Assemble each .cvma file into .cvmb using cvma2cvmb
%.cvmb: %.cvma $(ASSEMBLER_BIN)
	./$(ASSEMBLER_BIN) $< -o $@

# Build runner and execute all assembled .cvmb files
.PHONY: run
run: all
	@$(MAKE) -C ../example vm_run
	@for cvmb in $(CVMB_FILES); do \
	    echo ""; \
	    echo "============================================================"; \
	    echo "  Executing $$cvmb via vm_run"; \
	    echo "============================================================"; \
	    $(RUNNER)$(EXE_EXT) $$cvmb; \
	done

.PHONY: clean
clean:
	rm -f $(ASSEMBLER_BIN) *.cvmb
