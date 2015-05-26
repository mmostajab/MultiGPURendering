CC=clang++
CFLAGS=-I.
DEPS = helper.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

multigpurendering: main.o
	clang++ -o multigpurendering main.o -lboost_system -lboost_thread -lX11 -lGLEW -lGL
