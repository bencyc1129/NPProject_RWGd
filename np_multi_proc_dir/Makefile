dep = ../utils/netio.cpp ../utils/netio.h ../utils/constant.h ../utils/parser.cpp ../utils/parser.h

all: npshell.cpp npshell.h np_multi_proc.cpp user.h user.cpp $(dep)
	g++ npshell.cpp user.cpp np_multi_proc.cpp $(dep) -o ../np_multi_proc -lpthread

clean: 
	rm -f ../np_multi_proc