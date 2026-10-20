ifdef PS4_PAYLOAD_SDK
    include $(PS4_PAYLOAD_SDK)/toolchain/orbis.mk
else
    $(error PS4_PAYLOAD_SDK is undefined: source scripts/env.sh)
endif

OUT := ../../build/probes
ELF := $(OUT)/hello.elf

CFLAGS := -Wall -Wextra -Werror -O2 -march=btver2 \
          -DPS4LM_LOG_HOST='"$(PS4LM_LOG_HOST)"' -DPS4LM_LOG_PORT=$(or $(PS4LM_LOG_PORT),9999)

all: $(ELF)

# The log host is baked in, so rebuild every time.
$(ELF): main.c FORCE
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -o $@ main.c

clean:
	rm -f $(ELF)

FORCE:
.PHONY: all clean FORCE
