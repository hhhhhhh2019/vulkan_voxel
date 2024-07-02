OUT=build/main

LIBS=glfw3 vulkan

DEBUG=1

CC?=gcc
LD=gcc

GLSL_CC?=glslc


CC_FLAGS=-c `pkg-config --cflags $(LIBS)`
LD_FLAGS=`pkg-config --libs $(LIBS)`

GLSL_CC_FLAGS=


ifeq ($(DEBUG),1)
	CC_FLAGS+=-g3
	LD_FLAGS+=-g3
else
	CC_FLAGS+=-DNDEBUG
endif


SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:src/%.c=build/%.o)

SHADER_SOURCES = $(wildcard src/*.glsl src/*.comp src/*.vert src/*.frag)
# SHADERS = $(SHADER_SOURCES:src/%.glsl=build/%.spv:src/%.frag=build/%.spv)
# SHADERS = $(patsubst src/%.frag,build/%.spv,$(patsubst src/%.vert,build/%.spv,$(SHADER_SOURCES)))
SHADERS = $(addsuffix .spv,$(SHADER_SOURCES:src/%=build/%))


build/%.o: src/%.c
	$(CC) $(CC_FLAGS) $< -o $@

build/%.spv: src/%
	$(GLSL_CC) $(GLSL_CC_FLAGS) $< -o $@

main: $(OBJECTS)
	$(LD) $(LD_FLAGS) $^ -o $(OUT)

all: main $(SHADERS)

clean:
	rm build/* -f
	rm $(OUT) -f
