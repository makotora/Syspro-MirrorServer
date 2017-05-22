OBJS = ContentServer.o
SOURCE = ContentServer.c
CC = gcc
CFLAGS = -c -Wall
LFLAGS = -Wall
SFLAGS = -lsocket -lnsl
OUT = MirrorServer, ContentServer

all: MirrorServer ContentServer

MirrorServer: MirrorServer.o
	$(CC) $(LFLAGS) MirrorServer.o -o MirrorServer

ContentServer: ContentServer.o
	$(CC) $(LFLAGS) ContentServer.o -o ContentServer

MirrorServer.o: MirrorServer.c
	$(CC) $(CFLAGS) $(SFLAGS) MirrorServer.c

ContentServer.o: ContentServer.c
	$(CC) $(CFLAGS) $(SFLAGS) ContentServer.c


clean:
	rm -rf $(OBJS) $(OUT)