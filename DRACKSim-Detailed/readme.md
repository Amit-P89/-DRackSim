#Run Simulation Instructions:
* Simulation has to be started in two parts:
	- In first part, instances of pintool need to be initiated with the respective workloads so as to start production of instruction trace.
	- In second part, the simulator need to be started so as to simulate over the produced instructions traces of each node (one trace for each node).
	- Instruction traces of multiple workloads can also be produced to represent trace for one-node (Multi-program support, not properly tested)

#Instructions to compile Detailed Cycle-Level Simulation model:
* The tool needs to setup paths, either add pin-paths or copy this whole directory to the $pin-path/source/tools/detail/
* Download DRACKSim2 from https://github.com/umd-memsys/DRAMSim2
* Extract DRAMSim2 in this directory and name it as DRAMSim2
* Apply DRAMSim2.patch on extracted DRAMSim2 directory to apply all the modifications that we made on it and copy files in DRAMSIM2_ini/ to ini/ in DRAMSIM2: 
```
patch -p1 < DRAMSim2.patch
```
* Download and install boost library in your system from https://www.boost.org/users/download/ or use the copy that we provide in this repo.
* Extract boost in the same directory
* Set the number of nodes and number of remote memory pools in the mem_defs.h
* Set the number of instructions or simulation cycles to simulate in the main.cpp line-no #125 
* Create executable using 'make sim'
	
#Instructions for producing instruction traces:
* After copying this to $pin-path/source/tools/ use 'make pin' command to compile the instruction trace tool
* Start as many instruction trace Pintools, as the number of nodes that you set in the mem_defs.h (This is required, otherwise simulation will not start)
* Samples for using the instruction trace tool are given in the 'bash start_sim.sh'
* After every simulation use 'bash clear_sm.sh' , because the simualtor creates shared memory variables and might create trouble during next execution if the execution was stopped
in-between. clear_sm.sh will clear all the shared memory variables

#Starting Simulation:
* For simplification, you can either use multiple terminal windows or install 'screen' utility on your system
* In one terminal, run 'bash start_sim.sh' to start production of isntruction traces. You can specif different options while starting this tool.
	- '- N n' : Mention the node number (mandatory argument and node numbers should start from 1 and should be in-order)
	- '-T 1/0' : '1' means only simulate multi-threaded part of the workload
	- '-S n' : skip N initial instructions
	- '-S' and '-T' can be used together, skip instructions won;t include single-threaded part if '-T 1' is used.
* In another terminal, start simulator DRackSim and pass a name of output directory to it, e.g. ' ./DRackSim abc '
* The simulation will start and continue until the exit condition arrives.
* The frequency of printing results can be changed using pre-processor #Result_cycle in main.cpp
