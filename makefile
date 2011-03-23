CC=gcc
DEBUG= -g
CFLAGS=-c -Wall $(DEBUG)
LDFLAGS=-lreadline -g $(DEBUG)

LEXFILE=posh.l
LEXSRC=$(LEXFILE:.l=.yy.c)

BISFILE=posh.y
BISSRC=$(BISFILE:.y=.tab.c)

SOURCES=main.c util.c command.c shellcontrol.c builtins.c jobcontrol.c $(LEXSRC) $(BISSRC)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=posh

all:  tags $(EXECUTABLE)

doc: $(SOURCES)
	doxygen rpc.cfg

tags: $(SOURCES)
	ctags -R

$(LEXSRC): $(LEXFILE)
	lex -o $(LEXSRC) $(LEXFILE)
 
$(BISSRC): $(BISFILE)
	bison --defines=tokens.h -o $(BISSRC) $(BISFILE)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) $(LEXSRC) $(BISSRC) tokens.h tags

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@
