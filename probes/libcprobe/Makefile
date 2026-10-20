ifdef PS4_PAYLOAD_SDK
    include $(PS4_PAYLOAD_SDK)/toolchain/orbis.mk
else
    $(error PS4_PAYLOAD_SDK is undefined: source scripts/env.sh)
endif

OUT := ../../build/probes
ELF := $(OUT)/libcprobe.elf

COMPAT := ../../ps4/compat
CFLAGS := -Wall -Wextra -Werror -O2 -march=btver2 -I$(COMPAT) \
          -DPS4LM_LOG_HOST='"$(PS4LM_LOG_HOST)"' -DPS4LM_LOG_PORT=$(or $(PS4LM_LOG_PORT),9999)
LDLIBS := -lpthread

all: $(ELF)

# The log host is baked in, so rebuild every time.
$(ELF): main.c $(COMPAT)/log.c FORCE
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -o $@ main.c $(COMPAT)/log.c $(LDLIBS)

clean:
	rm -f $(ELF)

FORCE:
.PHONY: all clean FORCE
