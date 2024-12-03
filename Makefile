default: monitor

monitor: monitor.o my_popen.o
	g++ -o monitor monitor.o my_popen.o -g

monitor.o: monitor.cc
	g++ -c -o monitor.o monitor.cc -g

my_popen.o: my_popen.c
	gcc -c -o my_popen.o my_popen.c -g

