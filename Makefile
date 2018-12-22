LIBYTPMV_DIR ?= /persist/libytpmv

CFLAGS ?= -g3
CXX ?= g++


LIBS= $(LIBYTPMV_DIR)/libytpmv.a -lglfw -lGL -lGLEW -lEGL -lX11 -lgbm -lSoundTouch -lpthread -lasound `pkg-config --libs gstreamer-1.0 gio-2.0`

CC_FLAGS = $(CFLAGS) -I$(LIBYTPMV_DIR)/include -Wall --std=c++14 `pkg-config --cflags gstreamer-1.0 gio-2.0` -fno-omit-frame-pointer
LD_FLAGS = $(LDFLAGS) $(LIBS)

all: example0 example1 example2 example3 example4 example5 example6 \
	example7 example8 example9 example10 example11 example12 example13\
	example14 example15 example16 example17 example18 example19 r3c r3c_2

clean:
	rm -f *.o
	for i in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17; do \
		rm -f example$$i; \
	done
	rm -f r3c r3c_2


$(LIBYTPMV_DIR)/libytpmv.a: FORCE
	$(MAKE) -C $(LIBYTPMV_DIR) libytpmv.a

FORCE:

%.o: %.C
	$(CXX) -c $(CC_FLAGS) $< -o $@

example0: example0.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example1: example1.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example2: example2.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example3: example3.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example4: example4.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example5: example5.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example6: example6.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example7: example7.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example8: example8.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example9: example9.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example10: example10.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example11: example11.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example12: example12.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example13: example13.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example14: example14.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example15: example15.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example16: example16.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example17: example17.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example18: example18.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

example19: example19.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

r3c: r3c.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)

r3c_2: r3c_2.o $(LIBYTPMV_DIR)/libytpmv.a
	$(CXX) -o $@ $< $(CC_FLAGS) $(LD_FLAGS)


