
#tell the linker the rpath so that we don't have to muck with LD_LIBRARY_PATH, etc
main: main.cpp
	$(CXX) -g -o TraceDRackSim main.cpp -I./DRAMSim2/ -L./DRAMSim2/ -ldramsim -Wl,-rpath=./DRAMSim2/ -lpthread

clean: 
	rm TraceDRackSim
