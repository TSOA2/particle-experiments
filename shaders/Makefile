CC := glslcc

VERTS := $(wildcard *.vert)
FRAGS := $(wildcard *.frag)

VERTOUTS := $(patsubst %.vert,bin/%.vert.msl,$(VERTS))
FRAGOUTS := $(patsubst %.frag,bin/%.frag.msl,$(FRAGS))

all: $(VERTOUTS) $(FRAGOUTS)

bin/%.vert.msl: %.vert
	$(CC) -v $^ -o $@ --lang=msl

bin/%.frag.msl: %.frag
	$(CC) -f $^ -o $@ --lang=msl

.PHONY: clean
clean:
	rm -f bin/*
