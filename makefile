CC = gcc
CFLAGS = -Wall -Wextra
TARGET = myfind
SRCS = myfind.c
OBJS = $(SRCS:.c)

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean