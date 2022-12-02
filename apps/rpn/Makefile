TARGET = rpn
OUTDIR = target
OBJS = rpn.o asm.o newlib_support.o

CPPFLAGS += -I.
CFLAGS   += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mcmodel=large
CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -mcmodel=large -ffreestanding -fno-exceptions -fno-rtti -std=c++17
LDFLAGS += --entry main -z norelro --image-base 0xFFFF800000000000 --static

.PHONY: all
all: $(OUTDIR)/$(TARGET)

$(OUTDIR)/rpn: $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o $@ $(OBJS) -lc -lc++ -lc++abi

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

%.o: %.c Makefile
	clang $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.asm Makefile
	nasm -f elf64 -o $@ $<