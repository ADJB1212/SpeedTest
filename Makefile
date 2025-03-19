CC = gcc
CFLAGS = -Wno-everything -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -lSDL2 -lSDL2_ttf -lcurl -lm

SRC = main.c
OBJ = $(SRC:.c=.o)
EXECUTABLE = SpeedTest

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)
	rm -f $(OBJ)
	./$(EXECUTABLE)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXECUTABLE)
