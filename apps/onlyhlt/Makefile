TARGET = onlyhlt
OUTDIR = target

.PHONY: all
all: $(OUTDIR)/$(TARGET)

$(OUTDIR)/onlyhlt: onlyhlt.asm Makefile
	nasm -f bin -o $@ $<