<h1>DRackSim Simulator</h1>
<h2>Hardware Disaggregated Memory systems Simulations at Scale</h2>  

<h3>Usage Instructions</h3>

***Installing Pin***
* Download pin-3.30-98830-g1d7b601b3-gcc-linux.tar.gz from 

https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html

***Detailed Cycle-Level Simulation model of DRACKSim:***
* Go in directory DRACKSim-Detailed and follow the instructions in readme.md file
	
***Traced-based Simulation model of DRACKSim:***
* Go in directory DRackSim-Trace and follow the instructions in readme.md file


***For validation of DRACKSim-Detailed with gem5:***
* Clone the copy of gem5 to you system with #Version 22.1.0.0 
* Apply gem5.patch to the original gem5 code, this will calibrate different latencies and add suitable cache-levels to match DRACKSim simulator to the validation target
* You can use the commands mentioned in gem5_commands_to_validate.sh, that we use to validate


***If you use this tool, please cite it as:***
```
@inproceedings{10.1145/3615979.3656059,
author = {Puri, Amit and Bellamkonda, Kartheek and Narreddy, Kailash and Jose, John and Tamarapalli, Venkatesh and Narayanan, Vijaykrishnan},
title = {DRackSim: Simulating CXL-enabled Large-Scale Disaggregated Memory Systems},
year = {2024},
isbn = {9798400703638},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
url = {https://doi.org/10.1145/3615979.3656059},
doi = {10.1145/3615979.3656059},
booktitle = {Proceedings of the 38th ACM SIGSIM Conference on Principles of Advanced Discrete Simulation},
pages = {3–14},
numpages = {12},
keywords = {CXL, Data Centers, Performance Evaluation, Simulation},
location = {<conf-loc>, <city>Atlanta</city>, <state>GA</state>, <country>USA</country>, </conf-loc>},
series = {SIGSIM-PADS '24}
}
```
