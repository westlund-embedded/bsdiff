# bsdiff
bsdiff for embedded systems with limited SRAM

The motivation for modifying the standard bsdiff was to add the possibility to configure the size of the RAM used by the OS when applying the bspatch. The current version of bspatch consumes RAM the size of oldfile + newfile + O(1). In this version of bspatch any size of RAM can be configured which will make it possible to use this great algorithm in an embedded system.

The standard version of bsdiff also uses bzip2 which is slow and consumes alot of RAM. This version will be using uzlib by pfalcon because of its small footprint in flash as well as configurable RAM usage. The compression ratio "should" be almost as good as bzip2 but it will be alot faster. Might do some benchmark later.

This version will only be built and tested under Linux. I will port this into NuttX RTOS which is a POSIX operating system much like Linux using smartfs file system and external FLASH. But that port will not be published here.

Note: 
- This version of bspatch is NOT compatible with the standard BSDIFF40 bsdiff
- Works with uzlib v2.9 (if it does not compile, get uzlib and re-compile the libtinf.a)
