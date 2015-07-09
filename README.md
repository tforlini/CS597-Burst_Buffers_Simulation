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


