# CS597-Burst_Buffers_Simulation

## ROSS Installation
Checkout, build, and install the trunk version of ROSS:
```
git clone http://github.com/carothersc/ROSS.git
cd ROSS
mkdir build
cd build
ARCH=x86_64 CC=mpicc CXX=mpicxx cmake -DCMAKE_INSTALL_PREFIX=../install ../
make -j 3
make install
```

The result should be that the latest version of ROSS is installed in the ROSS/install/ directory.

## CODES base installation
An automated building script is provided:
```
git clone git://git.mcs.anl.gov/radix/codes-base
cd codes-base
./prepare.sh
mkdir build
cd build
../configure --with-ross=/path/to/ross/install --prefix=/path/to/codes-base/install CC=mpicc
```
Where you need to configure the according path to your ROSS installation and your path to codes-base repository.
To enable network tracing with dumpi, use the option:
```
--with-dumpi = /path/to/dumpi/install with configure
```
To enable using the darshan I/O generator, use the option
```
--with-darshan=/path/to/darshan with configure
```
Then make and install codes-base:
```
make && make install
make tests && make check
```

## CODES Net installation
An automated building script is provided:
```
git clone git://git.mcs.anl.gov/radix/codes-net
cd codes-net
./prepare.sh
mkdir build
cd build
../configure --with-codes-base=/path/to/codes-base/install --prefix=/path/to/codes-net/install CC=mpicc
```
Where you need to configure the according path to your CODES-base installation and your path to codes-net repository.
Then make and install codes-net:
```
make && make install
make tests && make check
```
## Burst Buffer Simulator
Compile and run the Burst Buffer simulator code:
```
make Busrt_Buffer
```
To run the simulator, run the run.sh script with the following arguments:
-NP-number of processors
-SYNC-synchronization protocol (currently only works for SYNC=1 by default)
```
./run.sh <NP> <SYNC>
```
Example:
```
./run.sh 1 1
```

