<h1>DRackSim Simulator</h1>
<h2>Hardware Disaggregated Memory systems Simulations at Scale</h2>  

<h3>Usage Instructions</h3>

***Installing Pin***
* Download Intel pin-3.20-98437-gf02b61307-gcc-linux.tar.gz from 

https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html

***Detailed Cycle-Level Simulation model of DRACKSim:***
* Go in directory DRACKSim-Detailed and follow the instructions in readme.md file
	
***Traced-based Simulation model of DRACKSim:***
* Go in directory DRackSim-Trace and follow the instructions in readme.md file


***For validation of DRACKSim-Detailed with gem5:***
* Clone the copy of gem5 to you system with #Version 22.1.0.0 
* Apply gem5.patch to the original gem5 code, this will calibrate different latencies and add suitable cache-levels to match DRACKSim simulator to the validation target
* You can use the commands mentioned in gem5_commands_to_validate.sh, that we use to validate


***Citation Link:***
```
Puri, A., Jose, J., Venkatesh, T., & Narayanan, V. (2023). DRackSim: Simulator for Rack-scale Memory Disaggregation. ArXiv. /abs/2305.09977
```

***Full Paper Link:***
https://arxiv.org/abs/2305.09977

$${\color{white}Note: In the SC'23 paper, we had not reported results for Trace simulation model}$$
