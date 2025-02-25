#############################################################################
# Makefile for EECS226 project "Canny on RISC-V VP" (S21)
#############################################################################
#
# 06/23/21 RD	updated to share project assignments with other instructors
# 05/31/21 RD	adjusted for final exam
# 05/23/21 RD	adjusted for A8
# 05/20/21 RD	adjusted for A7
# 03/29/21 RD	initial version

### APPLICATION DEFINITIONS #################################################

VIDEO	= jku
SVIDEO	= jku
FRAMES	= $(VIDEO)[0-9][0-9][0-9]_edges.pgm
SFRAMES	= $(SVIDEO)[0-9][0-9][0-9]_edges.pgm

### C DEFINITIONS ###########################################################

GCC	= gcc
CFLAGS	= -Wall -ansi
LFLAGS	= -lm

### RISC-V VP DEFINITIONS ###################################################

RISCV_GCC    = riscv32-unknown-elf-gcc
RISCV_CFLAGS = irq.c ipu.h bootstrap.S -march=rv32ima -mabi=ilp32 -misa-spec=2.2 \
		-Wall -Wno-unused-variable -O3
RISCV_LFLAGS = -nostartfiles -Wl,--no-relax

RISCV_VP     = riscv-vp
RISCV_VPOPT  = --intercept-syscalls --use-dmi --tlm-global-quantum 1000

### TARGETS #################################################################

all: ipu_C_ref

test: test_ipu_C_ref

clean:
	rm -f *.bak *~
	rm -f *.o
	rm -f ${FRAMES}
	rm -f ${SFRAMES}
	rm -f ipu_C_ref

### MODELS ##################################################################

# A7: sequential Canny on RISC-V VP (with various optimization options)

ipu_C_ref: ipu.h ipu.c irq.c irq.h bootstrap.S
	${RISCV_GCC} ipu.c ${RISCV_CFLAGS} ${RISCV_LFLAGS} -lm -o ipu_C_ref

test_ipu_C_ref: ipu_C_ref
	${RISCV_VP} ${RISCV_VPOPT} ipu_C_ref

# EOF Makefile
