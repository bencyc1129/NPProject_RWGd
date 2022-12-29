dep = ../utils/netio.cpp ../utils/netio.h ../utils/constant.h ../utils/parser.cpp ../utils/parser.h

all: npshell.cpp npshell.h np_single_proc.cpp user.cpp user.h $(dep)
	g++ npshell.cpp np_single_proc.cpp user.cpp $(dep) -o ../np_single_proc

clean: 
	rm -f ../np_single_proc