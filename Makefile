PACKAGES := libpng12
CFLAGS := -O2 -std=c99 -pedantic -Wall -fPIC $(shell pkg-config --cflags $(PACKAGES))
LIBS := -lm -ltiff $(shell pkg-config --libs $(PACKAGES))
INC := -Iinclude
OBJS := turtle.o turtle_projection.o turtle_map.o turtle_datum.o turtle_client.o geotiff16.o jsmn.o

.PHONY: lib clean examples

lib: lib/libturtle.so
	@rm -f *.o

clean:
	@rm -rf example-* lib *.o

examples: example-demo example-projection example-pthread

lib/libturtle.so: $(OBJS)
	@mkdir -p lib
	@gcc -o $@ $(CFLAGS) -shared $(INC) $(OBJS) $(LIBS)

example-pthread: examples/example-pthread.c
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -lturtle -lpthread

example-%: examples/example-%.c
	@gcc -o $@ $(CFLAGS) $(INC) $< -Llib -lturtle

%.o: src/%.c include/%.h
	@gcc $(CFLAGS) $(INC) -o $@ -c $<
