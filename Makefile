NCURSES_CFLAGS := $(shell pkg-config --cflags ncursesw)
NCURSES_LIBS := $(shell pkg-config --libs ncursesw)

LIBS += $(NCURSES_LIBS) -lmagic -lpthread
CFLAGS += $(NCURSES_CFLAGS)

SRCS = main.c
OBJS = $(SRCS:.c=.o)

all: $(OBJS)
	gcc $(CFLAGS) $(OBJS) -o fsm $(LIBS)
	sudo ./fsm

.c.o:
	gcc $(CFLAGS) -c $<

clean:
	rm -f *.o
	rm -f *~