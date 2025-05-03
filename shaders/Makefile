# Wow
CC_MSL := glslcc
CC_SPV := glslc
SPV_FLAGS := -Os

VERTS := $(wildcard *.vert)
FRAGS := $(wildcard *.frag)
COMPS := $(wildcard *.comp)

SPV_VERTOUTS := $(patsubst %.vert,bin/%.vert.spv,$(VERTS))
SPV_FRAGOUTS := $(patsubst %.frag,bin/%.frag.spv,$(FRAGS))

MSL_VERTOUTS := $(patsubst %.vert,bin/%.vert.msl,$(VERTS))
MSL_FRAGOUTS := $(patsubst %.frag,bin/%.frag.msl,$(FRAGS))
MSL_COMPOUTS := $(patsubst %.comp,bin/%.comp.msl,$(COMPS))

all: spv msl

spv: $(SPV_VERTOUTS) $(SPV_FRAGOUTS)

msl: $(MSL_VERTOUTS) $(MSL_FRAGOUTS) $(MSL_COMPOUTS)

bin/%.vert.spv: %.vert
	$(CC_SPV) $(SPV_FLAGS) $^ -o $(basename $(basename $@))_vs.vert.spv

bin/%.frag.spv: %.frag
	$(CC_SPV) $(SPV_FLAGS) $^ -o $(basename $(basename $@))_fs.frag.spv

bin/%.vert.msl: %.vert
	$(CC_MSL) -v $^ -o $@ --lang=msl

bin/%.frag.msl: %.frag
	$(CC_MSL) -f $^ -o $@ --lang=msl

bin/%.comp.msl: %.comp
	$(CC_MSL) -c $^ -o $@ --lang=msl

.PHONY: clean
clean:
	rm -f bin/*
