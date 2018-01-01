CC          := gcc 
CFLAGS      := $(INCLUDEPATH) -O3 -Wall -Wno-pointer-sign
SRCDIR      := ./src/
SRCS        := $(SRCDIR)/CEPCUI.c
TARGET      := cepcui.exe
LIBS        := -lcep


.PHONY: all
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $(SRCS)

