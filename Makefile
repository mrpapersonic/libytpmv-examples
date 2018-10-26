LIBYTPMV_DIR ?= /persist/libytpmv

LIBS= $(LIBYTPMV_DIR)/libytpmv.a -lglfw -lGL -lGLEW -lEGL -lX11 -lgbm -lSoundTouch -lpthread -lasound `pkg-config --libs gstreamer-1.0 gio-2.0`
CC_FLAGS = -I$(LIBYTPMV_DIR)/include -g3 -Wall --std=c++0x `pkg-config --cflags gstreamer-1.0 gio-2.0` -fno-omit-frame-pointer


all: example1


$(LIBYTPMV_DIR)/libytpmv.a: FORCE
	$(MAKE) -C $(LIBYTPMV_DIR) libytpmv.a

FORCE:

%.o: %.C
	g++ -c $(CC_FLAGS) $< -o $@

example1: example1.o $(LIBYTPMV_DIR)/libytpmv.a
	g++ -o $@ $< $(CC_FLAGS) $(LIBS)

example2: example2.o $(LIBYTPMV_DIR)/libytpmv.a
	g++ -o $@ $< $(CC_FLAGS) $(LIBS)

example3: example3.o $(LIBYTPMV_DIR)/libytpmv.a
	g++ -o $@ $< $(CC_FLAGS) $(LIBS)

example4: example4.o $(LIBYTPMV_DIR)/libytpmv.a
	g++ -o $@ $< $(CC_FLAGS) $(LIBS)

r3c: r3c.o $(LIBYTPMV_DIR)/libytpmv.a
	g++ -o $@ $< $(CC_FLAGS) $(LIBS)


