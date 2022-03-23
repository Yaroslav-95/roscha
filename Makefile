VERSION=0.1.0
CC?=gcc
XFLAGS=
CFLAGS?=-std=c11 -O2 -Wall $(XFLAGS)

LIBS:=
IDIRS:=$(addprefix -iquote,include ./)

BUILDIR?=build/release

ifdef DEBUG
BUILDIR:=build/debug
CFLAGS:=-std=c11 -O0 -DDEBUG $(XFLAGS) -g
endif
ifdef ASAN
CFLAGS+= -fsanitize=address -fno-omit-frame-pointer
endif

OBJDIR=$(BUILDIR)/obj

ROSCHA_SRCS:=$(shell find . -name '*.c' -not -path '*/tests/*')
ROSCHA_OBJS:=$(ROSCHA_SRCS:%.c=$(OBJDIR)/%.o)
ALL_OBJS:=$(ROSCHA_OBJS)
TEST_OBJS:=$(filter-out $(OBJDIR)/src/roscha.o,$(ALL_OBJS))

all: roscha

test: tests/slice tests/lexer tests/parser tests/roscha

tests/%: $(OBJDIR)/src/tests/%.o $(TEST_OBJS)
	mkdir -p $(BUILDIR)/$(@D)
	$(CC) -o $(BUILDIR)/$@ $^ $(IDIRS) $(LIBS) $(CFLAGS)

$(OBJDIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) -c $(IDIRS) -o $@ $< $(LIBS) $(CFLAGS)

roscha: $(ALL_OBJS)
	mkdir -p $(@D)
	$(CC) -o $(BUILDIR)/$@ $^ $(LIBS) $(CFLAGS)

clean:
	rm -r build

.PHONY: clean all test

.PRECIOUS: $(OBJDIR)/src/tests/%.o
