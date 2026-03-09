BIN=../bin
DEBUG=../src/dgdebug

TARGETS=all test clean

SUBDIRS=$(wildcard */.)

$(TARGETS): $(SUBDIRS)
$(SUBDIRS):
		$(MAKE) --directory=$@ $(MAKECMDGOALS)

all: doc test clean

doc:
	$(BIN)/test.py doc

.PHONY: $(TARGETS) $(SUBDIRS) doc
