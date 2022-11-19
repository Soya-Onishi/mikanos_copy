TARGET = rpn
OUTDIR = target

CPPFLAGS += -I.
CFLAGS += -O2 -Wall -g --target=x86_64-elf -freestanding
CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -fno-exceptions -fno-rtti -std=c++17
LDFLAGS += --entry main -z norelro --image-base 0 --static

.PHONY: all
all: $(OUTDIR)/$(TARGET)

$(OUTDIR)/rpn: rpn.o Makefile
	ld.lld $(LDFLAGS) -o $@ $<

%.o:%.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@