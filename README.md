# bsdiff
bsdiff for embedded systems with limited SRAM

The motivation for modifying the standard bsdiff was to add the possibility to configure the size of the RAM used by the OS when applying the bspatch. The current version of bspatch consumes RAM the size of oldfile + newfile + O(1). In this version of bspatch any size of RAM can be configured which will make it possible to use this great algorithm in an embedded system.

The standard version of bsdiff also uses bzip2 which is slow and consumes alot of RAM. This version will be using uzlib by pfalcon because of its small footprint in flash as well as configurable RAM usage. The uzlib is compatible with the standard zlib so patch files can be generated with standard zlib (deflate). 

The first version is at the moment only built and tested under Linux. The next step is to port this into NuttX RTOS which is a POSIX operating system much like Linux.

This version of bspatch is not compatible with the standard BSDIFF40 bsdiff

