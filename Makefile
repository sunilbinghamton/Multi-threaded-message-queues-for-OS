.PHONY: a.out
a.out:
	gcc -c -pthread MQ.c -g
	g++ -pthread -o sender sender.cpp MQ.o -g
	g++ -pthread -o receiver receiver.cpp MQ.o -g
clean:
	rm mq sender receiver MQ.o
