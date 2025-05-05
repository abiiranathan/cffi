CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC -DBUILD_TEST
LDFLAGS = -lrt
SO_NAME = librv.so

all: test_visit_manager $(SO_NAME)

# Rule to build the shared object
$(SO_NAME): recent_visits.o
	$(CC) -shared -o $@ $^ $(LDFLAGS)

test_visit_manager: test_visit_manager.o recent_visits.o
	$(CC) -o $@ $^

test_visit_manager.o: test_visit_manager.c
	$(CC) $(CFLAGS) -c $<

recent_visits.o: recent_visits.c recent_visits.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o test_visit_manager *.dat $(SO_NAME)

run: test_visit_manager
	./test_visit_manager

.PHONY: all clean run
