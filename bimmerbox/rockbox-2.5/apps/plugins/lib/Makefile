#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id: Makefile,v 1.1 2007/09/21 18:43:40 duke4d Exp $
#

# ../.. for the plugin.h in the apps dir
# .. for stuff in the plugins dir
# . for stuff in the pluginlib dir
INCLUDES=-I$(APPSDIR) -I.. -I. -I$(FIRMDIR)/include -I$(FIRMDIR)/export	\
 -I$(FIRMDIR)/common -I$(FIRMDIR)/drivers -I$(BUILDDIR)

ifdef APPEXTRA
INCLUDES += -I$(APPSDIR)/$(APPEXTRA)
endif

CFLAGS = $(GCCOPTS) \
$(INCLUDES) $(TARGET) $(EXTRA_DEFINES) -DMEM=${MEMORYSIZE} -DPLUGIN

# Sectioned compilation for target
ifndef SIMVER
	CFLAGS += -ffunction-sections -fdata-sections
endif

# This sets up 'SRC' based on the files mentioned in SOURCES
include $(TOOLSDIR)/makesrc.inc

SOURCES = $(SRC)
OBJS := $(SRC:%.c=$(OBJDIR)/%.o)
DEPFILE = $(OBJDIR)/dep-pluginlib
DIRS = .

OUTPUT = $(BUILDDIR)/libplugin.a

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@echo "AR+RANLIB $@"
	@$(AR) ruv $@ $+ >/dev/null 2>&1
	@$(RANLIB) $@

include $(TOOLSDIR)/make.inc

clean:
	@echo "cleaning lib"
	@rm -f $(OBJS) $(OUTPUT) $(DEPFILE)

-include $(DEPFILE)
