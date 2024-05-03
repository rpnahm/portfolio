all: fractal fractalthread fractaltask

fractal: fractal.c gfx.c
	gcc fractal.c gfx.c -g -Wall --std=c99 -lX11 -lm -o fractal

fractalthread: fractalthread.c gfx.c
	gcc fractalthread.c gfx.c -g -Wall --std=c99 -lX11 -lm -pthread -o fractalthread

fractaltask: fractaltask.c gfx.c
	gcc fractaltask.c gfx.c -g -Wall --std=c99 -lX11 -lm -pthread -o fractaltask

clean:
	rm fractal fractalthread fractaltask