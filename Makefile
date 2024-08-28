# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -I.
LDFLAGS = -L./ -lcalc -lpthread

# Targets
all: test server client

# Build server
server: servermain.o libcalc.a
	$(CXX) $(CXXFLAGS) -o server servermain.o $(LDFLAGS)

# Build client
client: clientmain.o libcalc.a
	$(CXX) $(CXXFLAGS) -o client clientmain.o $(LDFLAGS)

# Build test
test: main.o libcalc.a
	$(CXX) $(CXXFLAGS) -o test main.o $(LDFLAGS)

# Compile servermain.cpp
servermain.o: servermain.cpp
	$(CXX) $(CXXFLAGS) -c servermain.cpp

# Compile clientmain.cpp
clientmain.o: clientmain.cpp
	$(CXX) $(CXXFLAGS) -c clientmain.cpp

# Compile main.cpp
main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp

# Archive library
libcalc.a: calcLib.o
	ar -rc libcalc.a calcLib.o

# Compile calcLib.c
calcLib.o: calcLib.c
	$(CXX) $(CXXFLAGS) -fPIC -c calcLib.c

# Clean up
clean:
	rm -f *.o *.a test server client
