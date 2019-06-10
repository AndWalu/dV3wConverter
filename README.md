# dV3wConverter

## Overview

3wConverter tool is used to:

* convert 3w file created by XYZware to GCODE file,
* convert GCODE files to 3w file that can by used by XYZware,
* print 3w file on XYZprinting printer.

## Installation
To compile this software is needed the Microsoft Visual Studio C++ compiler with cmake and git support.<br>
This software was tested with version 2019 of Visual Studio.<br>
To download all needed library and compile the program run script: **dV3wConverter.cmd**<br>
This script assumes that Visual Studio IDE is installed in "c:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE" directory.<br>
You can adjust this by changing the variable **MSVSIDE** at line 18 in **dV3wConverter.cmd**  file.<br>
The **git** utility is needed to download all necessary library.<br>
The script try to use a **git** version supplied with Visual Studio but it sometimes fail for no reason.<br>
To disable using Visual Studio git version comment out line 22 in **dV3wConverter.cmd** file (by adding at beginning of this line **REM**) and install **git for Windows**. <br>


## Usage
**Converting:**

  dV3wConvertre ([/W] | [/G]) [/Y] (<input file> | [-]) (<output file> | [-]) [prefix file]

**Printing:**

  dV3wConvertre ([/PW] | [/PG]) (<input file> | [-]) <com port[:com speed]>

Input or/and output file can be "-" to read or write to stdin or stdout.

## Example:
**Extracting GCODE from 3w file:**

  dV3wConvertre /W [/Y] Input_3w_File Output_GCODE_File [Output_Prefix_File]

**Creating 3w file from GCODE:**

  dV3wConvertre /G [/Y] Input_GCODE_File Output_3w_File [Input_Prefix_File]

For creating 3w file the 3w file prefix is needed.
If the 3w file prefix is not specified on the command line it is assumed that it is given at beginning of input file.

**Printing 3W file:**

  dV3wConvertre  /PW Input_3w_File com12

**Printing GCODE file:**

  dV3wConvertre  /PG Input_GCODE_File com12
  
**Switch list:**
 * /W - input file for conversion in 3w format
 * /G - input file for conversion in GCODE format
 * /PW - input file for printing in 3w format
 * /PG - input file for printing in GCODE format
 * /Y - overwrite files
  
  
## 3w file structure
![](https://github.com/AndWalu/dV3wConverter/raw/master/dV3wConverter/Documentation/img/3w_file.gif)
