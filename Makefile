CC = gcc
VERSION = 0.0.1
CFLAGS = -g -O2
LIBS = -lcrypto -lssl -lz 
INSTALL=/bin/install -c
prefix=/usr/local
bindir=$(prefix)${exec_prefix}/bin
FLAGS=$(CFLAGS) -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DSTDC_HEADERS=1 -DHAVE_LIBZ=1 -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -DHAVE_LIBSSL=1 -DHAVE_LIBCRYPTO=1

OBJ=common.o Settings.o FileTypes.o filestore.o ConfigFile.o directory_listing.o commands-file-info.o commands-file-transfer.o commands-parse.o
DRIVERS_C := $(wildcard filestore-types/*.c)
DRIVERS_O := $(patsubst %.c,%.o,$(DRIVERS_C))

all: $(OBJ) DRIVERS LIBUSEFUL fileferry 

fileferry: $(OBJ) $(DRIVERS_O) LIBUSEFUL main.c
	gcc $(FLAGS) $(LIBS) -ofileferry main.c $(OBJ) $(DRIVERS_O) libUseful-2.0/libUseful-2.0.a

DRIVERS: $(DRIVERS_C)
	@cd filestore-types; $(MAKE)

LIBUSEFUL:
	@cd libUseful-2.0; $(MAKE)

FileTypes.o: FileTypes.h FileTypes.c
	gcc $(FLAGS) -c FileTypes.c

filestore.o: filestore.h filestore.c
	gcc $(FLAGS) -c filestore.c

ConfigFile.o: ConfigFile.h ConfigFile.c
	gcc $(FLAGS) -c ConfigFile.c

Settings.o: Settings.h Settings.c
	gcc $(FLAGS) -c Settings.c

directory_listing.o: directory_listing.h directory_listing.c
	gcc $(FLAGS) -c directory_listing.c

common.o: common.h common.c
	gcc $(FLAGS) -c common.c

commands-parse.o: commands-parse.h commands-parse.c
	gcc $(FLAGS) -c commands-parse.c

commands-file-info.o: commands-file-info.h commands-file-info.c
	gcc $(FLAGS) -c commands-file-info.c

commands-file-transfer.o: commands-file-transfer.h commands-file-transfer.c
	gcc $(FLAGS) -c commands-file-transfer.c


clean:
	@rm -f *.o */*.o */*.a

