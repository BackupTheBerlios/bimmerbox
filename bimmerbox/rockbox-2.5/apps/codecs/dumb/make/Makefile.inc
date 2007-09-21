# This file contains the main rules for compiling the library. It is included
# twice with different values for CFLAGS and OBJDIR, so the optimised and
# debugging libraries are both built.

CORE_OBJECTS := $(addprefix $(OBJDIR)/, $(notdir $(patsubst %.c, %.o, $(CORE_MODULES))))
ALLEGRO_OBJECTS := $(addprefix $(OBJDIR)/, $(notdir $(patsubst %.c, %.o, $(ALLEGRO_MODULES))))


# Pass the current value of CFLAGS through to the commands. Or, more
# accurately, create a local copy of the current CFLAGS variable. This is
# necessary because Make doesn't expand variables in commands until they are
# executed.
$(CORE_LIB_FILE): CFLAGS := $(CFLAGS)
$(ALLEGRO_LIB_FILE): CFLAGS := $(CFLAGS)


$(OBJDIR)/%.o: src/core/%.c include/dumb.h include/internal/dumb.h
	@echo "(dumb) CC $<"
	@$(CC) -c -o $@ $< $(CFLAGS)

$(OBJDIR)/%.o: src/helpers/%.c include/dumb.h
	@echo "(dumb) CC $<"
	@$(CC) -c -o $@ $< $(CFLAGS)

$(OBJDIR)/%.o: src/it/%.c include/dumb.h include/internal/it.h
	@echo "(dumb) CC $<"
	@$(CC) -c -o $@ $< $(CFLAGS)

$(OBJDIR)/%.o: src/allegro/%.c include/aldumb.h include/dumb.h \
                      include/internal/aldumb.h include/internal/dumb.h
	@echo "(dumb) CC $<"
	@$(CC) -c -o $@ $< $(CFLAGS) $(WFLAGS_ALLEGRO)

$(CORE_LIB_FILE): $(CORE_OBJECTS)
	@echo "(dumb) AR $<"
	@$(AR) rs $@ $^

$(ALLEGRO_LIB_FILE): $(ALLEGRO_OBJECTS)
	@echo "(dumb) AR $<"
	@$(AR) rs $@ $^