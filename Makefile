linker: sched.cpp
	g++ -gdwarf-3 -std=c++11 sched.cpp -o sched
clean:
	rm -f sched *~