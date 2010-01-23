#CCUDA=/opt/cuda/bin/nvcc
#CXX=g++
#CCUDAFLAGS=--device-emulation -DTHROW_IS_ABORT
#CXXFLAGS=-g -O0 -I /opt/cuda/include -I ./src
#LDFLAGS=-L /opt/cuda/lib64 
CCUDA=/usr/local/cuda/bin/nvcc
CXX=g++
CCUDAFLAGS=--device-emulation -DTHROW_IS_ABORT
CXXFLAGS=-g -O0 -I /usr/local/cuda/include -I ./src
LDFLAGS=-L /usr/local/cuda/lib

LIBPEYTON=src/astro/BinaryStream.o
OBJECTS= src/swarmlib.o src/swarm.cu_o $(LIBPEYTON)
EXEOBJECTS = src/swarm.o src/swarmdump.o src/swarm_test_hermite_gpu.o
ALLOBJECTS = $(OBJECTS) $(EXEOBJECTS)

all: bin/swarm bin/swarmdump bin/swarm_test_hermite_gpu

#### Integrators
include integrators/*/Makefile.mk
####

bench: all
	(cd run && ../bin/easyGen.py && ../bin/swarm)

bin/swarm: $(OBJECTS) src/swarm.o 
	$(CXX) -rdynamic -o $@ $(LDFLAGS) -lcuda -lcudart $(OBJECTS) src/swarm.o

src/swarm.o: src/swarm.h

test: all
	(cd run && (test -f data.0 || ../bin/easyGen.py) && ../bin/swarm_test_hermite_gpu && ../bin/swarmdump)


bin/swarmdump: $(OBJECTS) src/swarmdump.o
	$(CXX) -rdynamic -o $@ $(LDFLAGS) -lcuda -lcudart $(OBJECTS)  src/swarmdump.o
bin/swarmdump.o: src/swarm.h src/swarmio.h

bin/swarm_test_hermite_gpu: $(OBJECTS) src/swarm_test_hermite_gpu.o 
	$(CXX) -rdynamic -o $@ $(LDFLAGS) -lcuda -lcudart $(OBJECTS)  src/swarm_test_hermite_gpu.o
bin/swarm_test_hermite_gpu.o: src/swarm.h src/swarmio.h


src/swarmlib.o: src/swarm.h src/swarmio.h
src/swarm.cu: src/swarm.h $(CUDA_DEPS)
	echo "// AUTO-GENERATED FILE. DO NOT EDIT BY HAND!!!" > $@
	./bin/combine_cu_files.sh src/swarmlib.cu integrators/*/*.cu >> $@

clean:
	rm -f $(ALLOBJECTS) *.linkinfo src/astro/*.o src/swarm.cu bin/swarmdump bin/swarm integrators/*/*.o bin/swarm_test_hermite_gpu

tidy: clean
	rm -f *~ src/*~ src/astro/*~ integrators/*/*~ DEADJOE
	rm -f run/data.* run/observeTimes.dat

%.cu_o:%.cu
	$(CCUDA) -c $(CCUDAFLAGS) $(CXXFLAGS) $(DEBUG) $(PRECOMPILE) $< -o $@

%.o:%.cpp
	$(CXX) -c $(CXXFLAGS) $(PRECOMPILE) $< -o $@

