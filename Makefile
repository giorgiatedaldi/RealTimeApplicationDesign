CC = g++
CFLAGS = -O3 -Wall -pthread -std=c++11
LFLAGS = -Lrt -pthread -lrt_pthread

OUT = rt/librt_pthread.a application-ok application-err_p application-err_ap

all : $(OUT)
	
application-%: application-%.o executive.o busy_wait.o
	$(CC) -o $@ $^ $(LFLAGS)

application-%.o: application-%.cpp executive.h busy_wait.h
	$(CC) $(CFLAGS) -c -o $@ $<

executive.o: executive.cpp executive.h
	$(CC) $(CFLAGS) -c executive.cpp

busy_wait.o: busy_wait.cpp busy_wait.h
	$(CC) $(CFLAGS) -c busy_wait.cpp

rt/librt_pthread.a:
	cd rt; make

clean:
	rm -f *.o *~ $(OUT)
	cd rt; make clean



