# PS4LM app (OpenOrbis fake PKG, title PSLM00001). Needs OO_PS4_TOOLCHAIN.
#   make test      plain-C tests on the Mac
#   make           build/app/PSLM00001.pkg (scripts/build-app.sh wraps this)
#   make PROBE=1   the probe app instead of the launcher
ROOT     := $(abspath ..)
HOSTOUT  := $(ROOT)/build/app-host
HOST_SRC := src/conf.c src/net.c src/state.c

TITLE      := PS4LM
VERSION    := 1.00
TITLE_ID   := PSLM00001
CONTENT_ID := IV0000-PSLM00001_00-PS4LMLAUNCHER000

OO       := $(OO_PS4_TOOLCHAIN)
LLVM     ?= /opt/homebrew/opt/llvm@18/bin
OUT      := $(ROOT)/build/app
OBJDIR   := $(OUT)/obj
STAGE    := $(OUT)/pkgroot
OOBIN    := $(OO)/bin/$(if $(filter Darwin,$(shell uname -s)),macos,linux)
FSELF    := $(OOBIN)/$(if $(filter Darwin,$(shell uname -s)),create-fself-macos,create-fself)
PKGTOOL  := $(OOBIN)/PkgTool.Core
SAMPLES  := $(OO)/samples

LIBS     := -lc -lkernel -lc++ -lSceVideoOut -lSceSysmodule -lSceFreeType -lScePad \
            -lSceUserService -lSceNetCtl -lSceSystemService
FLAGS    := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O2 -Wall \
            -isysroot $(OO) -isystem $(OO)/include -DGRAPHICS_USES_FONT \
            -I$(SAMPLES)/_common -Isrc -Ithird_party/qrcodegen
CFLAGS   := $(FLAGS) -std=gnu11
CXXFLAGS := $(FLAGS) -isystem $(OO)/include/c++/v1 -std=gnu++17
LDFLAGS  := -m elf_x86_64 -pie --script $(OO)/link.x --eh-frame-hdr -L$(OO)/lib $(LIBS) $(OO)/lib/crt1.o

MAIN     := $(if $(PROBE),src/probe.cpp,src/main.cpp)
C_SRC    = $(HOST_SRC) $(C_EXTRA)
CXX_SRC  := $(MAIN) $(SAMPLES)/_common/graphics.cpp $(if $(PROBE),,src/screen.cpp)
C_EXTRA  := $(if $(PROBE),,third_party/qrcodegen/qrcodegen.c)
OBJS     := $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(C_SRC))) $(patsubst %.cpp,$(OBJDIR)/%.o,$(notdir $(CXX_SRC)))
vpath %.c src third_party/qrcodegen
vpath %.cpp src $(SAMPLES)/_common $(SAMPLES)/input/input

all: $(OUT)/$(TITLE_ID).pkg

check-oo:
	@test -n "$(OO)" -a -d "$(OO)/lib" || { echo "set OO_PS4_TOOLCHAIN (docs/TOOLCHAIN.md)" >&2; exit 1; }

$(OBJDIR)/%.o: %.c src/*.h | check-oo
	@mkdir -p $(OBJDIR)
	$(LLVM)/clang $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.cpp src/*.h | check-oo
	@mkdir -p $(OBJDIR)
	$(LLVM)/clang++ $(CXXFLAGS) -c -o $@ $<

# Sony-free files the toolchain ships with its samples are copied in at build
# time, not kept in git: the module stubs, right.sprx and the OFL font.
$(OUT)/$(TITLE_ID).pkg: $(OBJS) assets/icon0.png $(wildcard assets/pic1.png) Makefile
	rm -rf $(STAGE) && mkdir -p $(STAGE)/sce_sys/about $(STAGE)/sce_module $(STAGE)/assets/fonts
	$(LLVM)/ld.lld $(OBJS) -o $(OUT)/eboot.elf $(LDFLAGS)
	cd $(STAGE) && $(FSELF) -in=$(OUT)/eboot.elf -out=$(OUT)/eboot.oelf --eboot eboot.bin --paid 0x3800000000000011
	cp $(SAMPLES)/hello_world/sce_module/*.prx $(STAGE)/sce_module/
	cp $(SAMPLES)/hello_world/sce_sys/about/right.sprx $(STAGE)/sce_sys/about/
	cp $(SAMPLES)/font/assets/fonts/Gontserrat-Regular.ttf $(STAGE)/assets/fonts/font.ttf
	cp $(SAMPLES)/font/assets/fonts/OFL.txt $(STAGE)/assets/fonts/OFL.txt
	cp assets/icon0.png $(STAGE)/sce_sys/icon0.png
	$(if $(wildcard assets/pic1.png),cp assets/pic1.png $(STAGE)/sce_sys/pic1.png)
	cd $(STAGE) && $(PKGTOOL) sfo_new sce_sys/param.sfo && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1 && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)' && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value 0 && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value 'gd' && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)' && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0 && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 0 && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value '$(TITLE)' && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)' && \
	  $(PKGTOOL) sfo_setentry sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'
	cd $(STAGE) && $(OOBIN)/create-gp4 -out pkg.gp4 --content-id=$(CONTENT_ID) \
	  --files "$$(find eboot.bin sce_sys sce_module assets -type f | sort | tr '\n' ' ')"
	cd $(STAGE) && $(PKGTOOL) pkg_build pkg.gp4 $(OUT)
	mv $(OUT)/$(CONTENT_ID).pkg $@

test: $(HOSTOUT)/test_host
	tests/vectors.sh > $(HOSTOUT)/vectors.txt
	$(HOSTOUT)/test_host $(HOSTOUT)/vectors.txt tests/fixture.py

$(HOSTOUT)/test_host: tests/test_host.c $(HOST_SRC) $(wildcard src/*.h) tests/fixture.py
	mkdir -p $(HOSTOUT)
	cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -o $@ tests/test_host.c $(HOST_SRC)

clean:
	rm -rf $(OUT) $(HOSTOUT)

.PHONY: all test clean check-oo
