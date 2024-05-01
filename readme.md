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


***If you use this tool, please cite it as***
```
@misc{puri2023dracksim,
title={DRackSim: Simulator for Rack-scale Memory Disaggregation}, 
author={Amit Puri and John Jose and Tamarapalli Venkatesh and Vijaykrishnan Narayanan},
year={2023},
eprint={2305.09977},
archivePrefix={arXiv},
primaryClass={cs.DC}}
```


***This work is Accepted For Presentation at SIGSIM PADS'24, Atlanta, GA, USA:***
https://sigsim.acm.org/conf/pads/2024/

***DRackSim: Simulating CXL-enabled Large-Scale Disaggregated Memory Systems***
**Amit Puri, Kartheek Bellamkonda, Kailash Narreddy, John Jose, Tamarapalli Venkatesh, Vijaykrishnan Narayanan**
38th ACM SIGSIM Conference on Principles of Advanced Discrete Simulation [PADS-2024], Atlanta, Georgia, USA, 2024. (To appear)
