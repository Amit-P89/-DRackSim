#Prerequisite for running Trace-Based Simulation:
* Copy whole of this directory to $pin-path/source/tools/
* Download DRAMSim2 in this directory and apply DRAMSim2.patch on the extracted 'DRAMSim2' directory. 
```
patch -p1 < DRAMSim2.patch
```
* Make DRAMSim2
* Copy files in DRAMSim2_ini to ini directory in DRAMSim2/ini
* Download boost or use the copy provided and extract it into $pin-path/source/include/pin/
* copy files in Instlib directory to pin/source/tools/Instlib/
* copy Caches.H into $pin-path/source/include/pin/

-Todo: Provide a script to perform all the prerequisite

#Running Trace-Based Simulation
* Trace production and disaggregated memory simulation works independently in two steps.
* The traces are produced first using Pin and the tool inside directory Trace_tool
* Multiple traces can be collected representing multiple nodes
