CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC
LIBS = 
SOURCES = so_stdio.c
OBJS = $(SOURCES:.c=.o)
EXE = libso_stdio.so

build: $(EXE)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ 
$(EXE): $(OBJS)
	$(CC) -shared $(CFLAGS) $^ -o $(EXE)  $(LIBS)
run:
	./$(EXE)
clean:
	rm -f *.o $(EXE)