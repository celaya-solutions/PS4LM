# PS4LM app. `make test` runs the plain-C tests on the Mac.
ROOT     := $(abspath ..)
HOSTOUT  := $(ROOT)/build/app-host
HOST_SRC := src/conf.c

test: $(HOSTOUT)/test_host
	tests/vectors.sh > $(HOSTOUT)/vectors.txt
	$(HOSTOUT)/test_host $(HOSTOUT)/vectors.txt

$(HOSTOUT)/test_host: tests/test_host.c $(HOST_SRC) $(wildcard src/*.h)
	mkdir -p $(HOSTOUT)
	cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -o $@ tests/test_host.c $(HOST_SRC)

.PHONY: test
