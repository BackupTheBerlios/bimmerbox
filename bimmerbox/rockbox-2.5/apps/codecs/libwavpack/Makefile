#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id: Makefile,v 1.1 2007/09/21 18:43:39 duke4d Exp $
#

INCLUDES=-I$(APPSDIR) -I.. -I. -I$(FIRMDIR)/include -I$(FIRMDIR)/export	\
 -I$(FIRMDIR)/common -I$(FIRMDIR)/drivers -I$(BUILDDIR)

ifdef APPEXTRA
INCLUDES += -I$(APPSDIR)/$(APPEXTRA)
endif

CFLAGS = $(GCCOPTS) \
$(INCLUDES) $(TARGET) $(EXTRA_DEFINES) -DMEM=${MEMORYSIZE} -O2 \

# This sets up 'SRC' based on the files mentioned in SOURCES
include $(TOOLSDIR)/makesrc.inc

SOURCES = $(SRC)
OBJS2 := $(SRC:%.c=$(OBJDIR)/%.o)
OBJS = $(patsubst %.S, $(OBJDIR)/%.o, $(OBJS2))
DEPFILE = $(OBJDIR)/dep-libwavpack
DIRS = 

OUTPUT = $(BUILDDIR)/libwavpack.a

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@echo "AR $@"
	@$(AR) ruv $@ $+ >/dev/null 2>&1

$(OBJDIR)/libwavpack/%.o: $(APPSDIR)/codecs/libwavpack/%.c
	@echo "(libwavpack) CC $<"
	@$(CC) -c -O2 $(CFLAGS) -I$(APPSDIR)/codecs/libwavpack/ $< -o $@

include $(TOOLSDIR)/make.inc

clean:
	@echo "cleaning libwavpack"
	@rm -f $(OBJS) $(OUTPUT) $(DEPFILE)

-include $(DEPFILE)
