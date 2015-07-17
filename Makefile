# Update the following 3 variables for your own system:
CODESBASE=/home/ubuntu/codes-base/install/
CODESNET=/home/ubuntu/codes-net/install/
ROSS=/home/ubuntu/ROSS/install


ifndef CODESBASE

$(error CODESBASE is undefined, see README.txt)

  endif

  ifndef CODESNET

  $(error CODESNET is undefined, see README.txt)

  endif

  ifndef ROSS

  $(error ROSS is undefined, see README.txt)

  endif

override CPPFLAGS += $(shell $(ROSS)/bin/ross-config --cflags) -I$(CODESBASE)/include -I$(CODESNET)/include
CC = $(shell $(ROSS)/bin/ross-config --cc)
  LDFLAGS = $(shell $(ROSS)/bin/ross-config --ldflags) -L$(CODESBASE)/lib -L$(CODESNET)/lib
  LDLIBS = $(shell $(ROSS)/bin/ross-config --libs) -lcodes-net -lcodes-base




 pvfs: Burst_Buffers.c



 clean: rm -f Burst_Buffer
