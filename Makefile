###################################################
# Generic makefile
# for compiling and linking C++ projects on Linux 
# Author: Lucas law
####################################################
### Customising
#
# Adjust the following if necessary; TARGET is the target
# executable's filename, and LIBS is a list of libraries to link in
# (e.g. alleg, stdcx, iostr, etc). You can override these on make's
# command line of course, if you prefer to do it that way.
#
#
TARGET     := main
LIBDIR     :=
LIBS       := pthread
INCLUDES   += .
SRCDIR     := src
#
# # Now after any implicit rules' variables if you like. e.g.:

CC       := gcc
CXX      := g++
CFLAGS   := -g -Wall -O2
CXXFLAGS := $(CFLAGS)
CXXFLAGS += $(addprefix -I,$(INCLUDES))
CXXFLAGS += -MMD -std=c++11
#
# # The next bit checks to see whether rm is in your djgpp bin
# # directory; if not it uses del instead, but this can cause (harmless)
# # 'File not found' error messages. If you are not using DOS at all,
# # set the variable to something which will unquestioningly remove
# # files.
# 
RM-F    := -rm -f
# # You shouldn't need to change anything below this point.
# 
SRCS := $(wildcard *.cpp) $(wildcard $(addsuffix /*.cpp, $(SRCDIR)))
SRCS += $(wildcard *.c) $(wildcard $(addsuffix /*.c, $(SRCDIR)))
OBJS := $(patsubst %.c,%.c.o,$(patsubst %.cpp,%.cpp.o,$(SRCS)))
DEPS := $(patsubst %.c.o,%.c.d,$(patsubst %.cpp.o, %.cpp.d, $(OBJS)))
MISSING_DEPS := $(filter-out $(wildcard $(DEPS)),$(DEPS))
MISSING_DEPS_SOURCES := $(wildcard $(patsubst %.cpp.d,%.cpp,$(patsubst %.c.d, %.c, $(MISSING_DEPS))))

.PHONY : all deps objs clean distclean rebuild info

all : $(TARGET)

deps : $(DEPS)

objs : $(OBJS)

clean distclean :
	$(RM-F) $(OBJS)
	$(RM-F) $(DEPS) 
	$(RM-F) $(TARGET)

rebuild : distclean all

ifneq ($(MISSING_DEPS),)
$(MISSING_DEPS) :
	@$(RM-F) $(patsubst %.d,%.o,$@)
endif

-include $(DEPS)
$(TARGET) : $(OBJS)
	@$(CXX) -o $(TARGET) $(OBJS) $(addprefix -L,$(LIBDIR)) $(addprefix -l,$(LIBS))
	@echo "LD $@"
%.cpp.o : %.cpp
	@$(CXX) $(CXXFLAGS) -c $^ -o $@
	@echo "CXX $@"
%.c.o : %.c
	@$(CC) $(CFLAGS) -c $^ -o $@
	@echo "CC $@"
info:
	@echo srcs: $(SRCS)
	@echo objs: $(OBJS)
	@echo deps: $(DEPS)
	@echo missing_deps: $(MISSING_DEPS)
	@echo missing_deps_sources: $(MISSING_DEPS_SOURCES)


