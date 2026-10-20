# Builds every probe into build/probes. Source scripts/env.sh first.
PROBES := hello memprobe

all clean:
	for p in $(PROBES); do $(MAKE) -C $$p $@ || exit 1; done

.PHONY: all clean
