# Update the following 3 variables for your own system:
CODESBASE=/home/tforlini/codes-base/install/
CODESNET=/home/tforlini/codes-net/install/
ROSS=/home/tforlini/ROSS/install


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




 pvfs: Burst_Buffer.c



 clean: rm -f Burst_Buffer
