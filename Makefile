CC=gcc
LIBS=gpiod
montauk-chair-interface: montauk-chair-interface.o
	$(CC) -o montauk-chair-interface montauk-chair-interface.o -l$(LIBS)
