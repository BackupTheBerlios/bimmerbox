#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id: Makefile,v 1.1 2007/09/21 18:43:37 duke4d Exp $
#

INCLUDES = -I$(FIRMDIR)/include -I$(FIRMDIR)/export -I$(FIRMDIR)/common \
 -I$(FIRMDIR)/drivers -I$(APPSDIR) -Ilib -I$(BUILDDIR)
CFLAGS = $(GCCOPTS) $(INCLUDES) $(TARGET) $(EXTRA_DEFINES) \
 -DMEM=${MEMORYSIZE} -DCODEC

ifdef APPEXTRA
   INCLUDES += -I$(APPSDIR)/$(APPEXTRA)
endif

ifdef SOFTWARECODECS
   CODECLIBS = -lmad -la52 -lFLAC -lTremor -lwavpack -lmusepack
endif

# we "borrow" the plugin LDS file
LDS := $(APPSDIR)/plugins/plugin.lds

LINKCODEC := $(OBJDIR)/codeclink.lds
DEPFILE = $(OBJDIR)/dep-codecs

# This sets up 'SRC' based on the files mentioned in SOURCES
include $(TOOLSDIR)/makesrc.inc

ROCKS := $(SRC:%.c=$(OBJDIR)/%.codec)
SOURCES = $(SRC)
ELFS := $(SRC:%.c=$(OBJDIR)/%.elf)
OBJS := $(SRC:%.c=$(OBJDIR)/%.o)
# as created by the cross-compiler for win32:
DEFS := $(SRC:%.c=$(OBJDIR)/%.def)
DIRS = .

CODECDEPS = $(LINKCODEC) $(BUILDDIR)/libcodec.a

.PHONY: libmad liba52 libFLAC libTremor libwavpack dumb libmusepack

OUTPUT = $(SOFTWARECODECS)

all: $(OUTPUT) $(ROCKS) $(DEPFILE)

ifndef SIMVER
ELFIT =	$(SILENT)(file=`basename $@`; \
	echo "LD $$file"; \
	$(CC) $(GCCOPTS) -O -nostdlib -o $@ $< -L$(BUILDDIR) $(CODECLIBS) -lcodec -lgcc -T$(LINKCODEC) -Wl,-Map,$(OBJDIR)/$*.map)

.elf.o:
	$(ELFIT)

$(OBJDIR)/mpa.elf: $(OBJDIR)/mpa.o $(CODECDEPS) $(BUILDDIR)/libmad.a
$(OBJDIR)/a52.elf: $(OBJDIR)/a52.o $(CODECDEPS) $(BUILDDIR)/liba52.a
$(OBJDIR)/flac.elf: $(OBJDIR)/flac.o $(CODECDEPS) $(BUILDDIR)/libFLAC.a
$(OBJDIR)/vorbis.elf: $(OBJDIR)/vorbis.o $(CODECDEPS) $(BUILDDIR)/libTremor.a
$(OBJDIR)/mpc.elf: $(OBJDIR)/mpc.o $(CODECDEPS) $(BUILDDIR)/libmusepack.a
$(OBJDIR)/wav.elf: $(OBJDIR)/wav.o $(CODECDEPS)
$(OBJDIR)/wavpack.elf: $(OBJDIR)/wavpack.o $(CODECDEPS) $(BUILDDIR)/libwavpack.a

$(OBJDIR)/%.elf: $(OBJDIR)/%.o $(CODECDEPS)
	$(ELFIT)

$(OBJDIR)/%.codec : $(OBJDIR)/%.elf
	@echo "OBJCOPY "`basename $@`
	@$(OC) -O binary $< $@
else

ifeq ($(SIMVER), x11)
###################################################
# This is the X11 simulator version

$(OBJDIR)/%.codec : $(OBJDIR)/%.o  $(BUILDDIR)/libcodec.a
	@echo "LD "`basename $@`
	@$(CC) $(CFLAGS) -shared $< -L$(BUILDDIR)  $(CODECLIBS) -lcodec -o $@
ifeq ($(findstring CYGWIN,$(UNAME)),CYGWIN)
# 'x' must be kept or you'll have "Win32 error 5"
#     $ fgrep 5 /usr/include/w32api/winerror.h | head -1
#         #define ERROR_ACCESS_DENIED 5L
else
	@chmod -x $@
endif

else # end of x11-simulator
###################################################
# This is the win32 simulator version
DLLTOOLFLAGS = --export-all
DLLWRAPFLAGS = -s --entry _DllMain@12 --target=i386-mingw32 -mno-cygwin

$(OBJDIR)/%.codec : $(OBJDIR)/%.o  $(BUILDDIR)/libcodec.a
	@echo "DLL "`basename $@`
	@$(DLLTOOL) $(DLLTOOLFLAGS) -z $(OBJDIR)/$*.def $<
	@$(DLLWRAP) $(DLLWRAPFLAGS) --def $(OBJDIR)/$*.def $< $(BUILDDIR)/libcodec.a \
                $(patsubst -l%,$(BUILDDIR)/lib%.a,$(CODECLIBS)) -o $@
ifeq ($(findstring CYGWIN,$(UNAME)),CYGWIN)
# 'x' must be kept or you'll have "Win32 error 5"
#     $ fgrep 5 /usr/include/w32api/winerror.h | head -1
#         #define ERROR_ACCESS_DENIED 5L
else
	@chmod -x $@
endif
endif # end of win32-simulator

endif # end of simulator section

include $(TOOLSDIR)/make.inc

$(BUILDDIR)/libcodec.a:
	@echo "MAKE in codecs/lib"
	@mkdir -p $(OBJDIR)/lib
	@$(MAKE) -C lib OBJDIR=$(OBJDIR)/lib

$(LINKCODEC): $(LDS)
	@echo "build $@"
	@cat $< | $(CC) -DMEMORYSIZE=$(MEMORYSIZE) -DCODEC $(INCLUDES) $(TARGET) $(DEFINES) -E -P - >$@

libmad:
	@echo "MAKE in libmad"
	@mkdir -p $(OBJDIR)/libmad
	@$(MAKE) -C libmad OBJDIR=$(OBJDIR)/libmad OUTPUT=$(BUILDDIR)/libmad.a

liba52:
	@echo "MAKE in liba52"
	@mkdir -p $(OBJDIR)/liba52
	@$(MAKE) -C liba52 OBJDIR=$(OBJDIR)/liba52 OUTPUT=$(BUILDDIR)/liba52.a

libFLAC:
	@echo "MAKE in libFLAC"
	@mkdir -p $(OBJDIR)/libFLAC
	@$(MAKE) -C libFLAC OBJDIR=$(OBJDIR)/libFLAC OUTPUT=$(BUILDDIR)/libFLAC.a

libTremor:
	@echo "MAKE in Tremor"
	@mkdir -p $(OBJDIR)/Tremor
	@$(MAKE) -C Tremor OBJDIR=$(OBJDIR)/Tremor OUTPUT=$(BUILDDIR)/libTremor.a

libwavpack:
	@echo "MAKE in libwavpack"
	@mkdir -p $(OBJDIR)/libwavpack
	@$(MAKE) -C libwavpack OBJDIR=$(OBJDIR)/libwavpack OUTPUT=$(BUILDDIR)/libwavpack.a

dumb:
	@echo "MAKE in dumb"
	@mkdir -p $(OBJDIR)/dumb
	@$(MAKE) -C dumb OBJDIR=$(OBJDIR)/dumb LIBDIR=$(BUILDDIR) OFLAGS="$(CFLAGS)"

libmusepack:
	@echo "MAKE in libmusepack"
	@mkdir -p $(OBJDIR)/libmusepack
	@$(MAKE) -C libmusepack OBJDIR=$(OBJDIR)/libmusepack OUTPUT=$(BUILDDIR)/libmusepack.a

clean:
	@echo "cleaning codecs"
	$(SILENT)rm -fr $(OBJDIR)/libmad $(BUILDDIR)/libmad.a $(OBJDIR)/liba52 $(OBJDIR)/libFLAC $(OBJDIR)/Tremor $(OBJDIR)/libwavpack $(OBJDIR)/dumb $(BUILDDIR)/libdumb.a $(BUILDDIR)/libdumbd.a $(OBJDIR)/libmusepack $(BUILDDIR)/libmusepack.a
	@$(MAKE) -C libmad clean OBJDIR=$(OBJDIR)/libmad
	@$(MAKE) -C liba52 clean OBJDIR=$(OBJDIR)/liba52
	@$(MAKE) -C libFLAC clean OBJDIR=$(OBJDIR)/libFLAC
	@$(MAKE) -C Tremor clean OBJDIR=$(OBJDIR)/Tremor
	@$(MAKE) -C libwavpack clean OBJDIR=$(OBJDIR)/libwavpack
	@$(MAKE) -C libmusepack clean OBJDIR=$(OBJDIR)/libmusepack
	@$(MAKE) -C dumb clean OBJDIR=$(OBJDIR)/dumb
	@$(MAKE) -C lib clean OBJDIR=$(OBJDIR)/lib
