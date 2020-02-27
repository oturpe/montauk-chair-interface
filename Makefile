CC=gcc
LIBS=-lgpiod -lasound

montauk-chair-interface: montauk-chair-interface.o
	$(CC) -o montauk-chair-interface montauk-chair-interface.o $(LIBS)
