# Specify the main target
TARGET = ./librpc.a
# Default build type
TYPE = debug
# Which directories contain source files and header file
SRC_DIRS = ./src
HDR_DIRS = ./src
# Which libraries are linked
LIBS = pthreads
# Dynamic libraries
DLIBS =
# Add directories to the include and library paths
INCPATH = ../inc
LIBPATH = 

# The compiler
CC = gcc
AR = ar
LD = ld

# General Option
CFLAGS = 
MACROS = LINUX_ENV
LDFLAGS = 

# The next blocks change some variables depending on the build type
ifeq ($(TYPE),debug)
LDFLAGS += 
CFLAGS += -Wall -g
MACROS += _DEBUG
endif

ifeq ($(TYPE),profile)
LDFLAGS += -pg /lib/libc.so.5
CFLAGS += -Wall -pg
MACROS += NDEBUG
endif

ifeq ($(TYPE), release)
LDFLAGS += -s
CFLAGS += -Wall -O2
MACROS += NDEBUG
endif


# Which files to add to backups, apart from the source code
EXTRA_FILES = Makefile

# Where to store object and dependancy files.
STORE = .make-$(TYPE)
# Makes a list of the source (.c) files.
SOURCE := $(foreach DIR,$(SRC_DIRS),$(wildcard $(DIR)/*.c))
# List of header files.
HEADERS := $(foreach DIR, $(SRC_DIRS) $(HDR_DIRS),$(wildcard $(DIR)/*.h))
# Makes a list of the object files that will have to be created.
OBJECTS := $(addprefix $(STORE)/, $(notdir $(SOURCE:.c=.o)))
#OBJECTS := $(foreach DIR,$(SRC_DIRS),$(wildcard $(DIR)/*.c))
#OBJECTS := $(notdir $(SOURCE))
# Same for the .d (dependancy) files.
DFILES := $(addprefix $(STORE)/, $(notdir $(SOURCE:.c=.dd)))

# Specify phony rules. These are rules that are not real files.
.PHONY: clean backup dirs

# Main target. The @ in front of a command prevents make from displaying
# it to the standard output.
$(TARGET): dirs $(OBJECTS)
	@echo Linking $(TARGET).
#	#@$(CC) -o $(TARGET) $(OBJECTS) $(LDFLAGS) $(foreach LIBRARY, $(LIBS),-l$(LIBRARY)) $(foreach LIB,$(LIBPATH),-L$(LIB))
	$(AR) rcs $@ $(OBJECTS)
# Rule for creating object file and .d file, the sed magic is to add
# the object path at the start of the file because the files gcc
# outputs assume it will be in the same dir as the source file.
$(STORE)/%.o: $(SRC_DIRS)/%.c
	@echo Creating object file for $*...
	$(CC) -Wp,-MMD,$(STORE)/$*.dd $(CFLAGS) $(foreach INC,$(INCPATH),-I$(INC)) $(foreach MACRO,$(MACROS),-D$(MACRO)) -c $< -o $@
	@sed -e '1s/^\(.*\)$$/$(subst /,\/,$(dir $@))\1/' $(STORE)/$*.dd > $(STORE)/$*.d
	@rm -f $(STORE)/$*.dd

# Empty rule to prevent problems when a header is deleted.
%.h: ;

# Cleans up the objects, .d files and executables.
clean:
	@echo Making clean.
	@-rm -f $(foreach DIR,$(SRC_DIRS),$(STORE)/*.d $(STORE)/*.o)
	@-rm -f $(TARGET)

# Backup the source files.
backup:
	@-if [ ! -e .backup ]; then mkdir .backup; fi;
	@zip .backup/backup_`date +%d-%m-%y_%H.%M`.zip $(SOURCE) $(HEADERS) $(EXTRA_FILES)

# Create necessary directories
dirs:
	@-if [ ! -e $(STORE) ]; then mkdir $(STORE); fi;
#	@-$(foreach DIR,$(DIRS), if [ ! -e $(STORE)/$(DIR) ]; then mkdir $(STORE)/$(DIR); fi; )

.PHONY: macro
macro:
	@echo $(MACRO)="$($(MACRO))"

# Includes the .d files so it knows the exact dependencies for every
# source.
-include $(DFILES)
