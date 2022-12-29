dep = ../utils/netio.cpp ../utils/netio.h ../utils/constant.h  ../utils/parser.cpp ../utils/parser.h

all: npshell.cpp npshell.h np_simple.cpp $(dep)
	g++ npshell.cpp np_simple.cpp $(dep) -o ../np_simple

clean: 
	rm -f ../np_simple