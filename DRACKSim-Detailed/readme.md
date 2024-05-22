<h1>Cycle-level Disaggregated Memory Simulator</h1>

***Instructions to use cycle-level Disaggregated Memory Simulation Tool***:
* Simulation has to be started in two steps simultaneously:

 	- In the first step, instances (as per the number of nodes required to simulate) of pintool need to be started with the respective workloads to start production of instruction trace.

	- In the second step, the simulator needs to be started to simulate the produced instructions traces of each node (one trace for each node).
	- Instruction traces of multiple workloads can also be produced to represent traces for one-node (Multi-program support, not properly tested)

***Instructions to compile Detailed Cycle-Level Simulation model:***
* The tool needs to set paths, either add pin-paths or copy this whole directory to the **$pin-path/source/tools/detail/** (/detail is the new directory to be created)
* Download DRACKSim2 from **https://github.com/umd-memsys/DRAMSim2**
* Extract DRAMSim2 in this directory and name it as DRAMSim2
* Apply DRAMSim2.patch on the extracted DRAMSim2 directory to apply all the modifications that we made on it and copy files from **DRAMSIM2_ini/** to **ini/** in **DRAMSIM2**: 
```
patch -p1 < DRAMSim2.patch
```

go inside DRAMSim2 and build DRAMSim2 as a library:
```
cd DRAMSim2
make libdramsim.so
cd ..
```

* Download and install the boost library in your system from **https://www.boost.org/users/download/** or use the copy we provide in this repo.
* Extract boost in the same directory
* Set the number of nodes and remote memory pools in the **mem_defs.h**
* Set the number of instructions or simulation cycles to simulate in the **main.cpp** 
* Create executable using '**make sim**'
	
***Instructions for producing instruction traces:***
* After copying this to **$pin-path/source/tools/** use '**make pin**' command to compile the instruction trace tool
* Start as many instruction trace Pintools, as the number of nodes you set in the mem_defs.h (This is required, otherwise simulation will not start)
* Samples for using the instruction trace tool are given in the '**bash start_sim.sh**'
* After every simulation, use '**bash clear_sm.sh**', because the simulator creates shared memory variables and might create trouble during the next simulation if the execution was stopped in-between. clear_sm.sh will clear all the shared memory variables. Also kill the running workload (to be automated in next updates).

***#Starting Simulation:***
* For simplification, you can either use multiple terminal windows or install '**screen**' utility on your system
* In first terminal, run '**bash start_sim.sh**' to start production of instruction traces. You can specify different options while starting this tool.
	- '**- N x**' : Mention the node number '**x**' (mandatory argument)
	- '**-T 1/0**' : '**1**' will only simulate multi-threaded part of the workload
	- '**-S n**' : skip '**n**' initial instructions
	- '**-S**' and '**-T**' can be used together; skipping will be done in multi-threaded part if '**-T 1**' is used.
	- '**-M**' : Number of instructions to trace and simulate (default 10000000) 	
* In the second terminal, start simulator DRackSim and pass the name of the output directory to it, e.g., ' **./DRackSim abc** '
* The simulation will start and continue until the exit condition arrives.
* The frequency of printing results can be changed using pre-processor #Result_cycle in main.cpp
