all: 
	+$(MAKE) -C np_simple_dir
	+$(MAKE) -C np_single_proc_dir
	+$(MAKE) -C np_multi_proc_dir

clean:
	rm -rf np_simple
	rm -rf np_single_proc
	rm -rf np_multi_proc