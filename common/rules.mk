CC:=$(CROSS_COMPILE)gcc

CFLAGS:=-Wall -Ofast -fopenmp -fopt-info-vec-optimized
LDFLAGS:=-Wall -Ofast -fopenmp -fopt-info-vec-optimized

ifndef CROSS_COMPILE
	CFLAGS += -march=native -flto
	LDFLAGS += -march=native -flto
endif

INCLUDE := -I../common/external/include
LIB := -L../common/external/lib -ljpeg -lfreetype -lpng -lasound -lz -lc -lm

EXESRCS := ../common/graphic.c ../common/touch.c ../common/image.c ../common/task.c $(EXESRCS)

EXEOBJS := $(patsubst %.c, %.o, $(EXESRCS))

$(EXENAME): $(EXEOBJS)
	$(CC) $(LDFLAGS) -o $(EXENAME) $(EXEOBJS) $(LIB)
	mv $(EXENAME) ../out/

clean:
	rm -f $(EXENAME) $(EXEOBJS)

%.o: %.c ../common/common.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

