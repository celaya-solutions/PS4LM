ifdef PS4_PAYLOAD_SDK
    include $(PS4_PAYLOAD_SDK)/toolchain/orbis.mk
else
    $(error PS4_PAYLOAD_SDK is undefined: source scripts/env.sh)
endif

OUT := ../../build/probes
ELF := $(OUT)/memprobe.elf

# The PS4's CPU is an 8-core AMD Jaguar: AVX and F16C, no AVX2 or FMA.
# libkernel_sys exports sceKernelGetHwModelName and statfs; libkernel_web does not.
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -O2 -march=btver2
LDLIBS   := -lkernel_sys -lpthread

all: $(ELF)

$(ELF): main.cpp
	mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(ELF)

.PHONY: all clean
