Each real time kernel port consists of three files that contain the core kernel
components and are common to every port, and one or more files that are 
specific to a particular microcontroller and/or compiler.


+ The FreeRTOS/Source/Portable/MemMang directory contains the three sample 
memory allocators as described on the http://www.FreeRTOS.org WEB site.

+ The other directories each contain files specific to a particular 
microcontroller or compiler.



For example, if you are interested in the GCC port for the ARM_CM3
microcontroller then the port specific files are contained in
FreeRTOS/Source/Portable/GCC/ARM_CM3 directory.

