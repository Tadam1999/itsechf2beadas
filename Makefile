CXXFLAGS = -Wall -D_FORTIFY_SOURCE=2 -fpie -Wl, -pie

parser: main.o toojpeg.o
	g++ $(CPPFLAGS) main.o toojpeg.o -o parser

toojpeg.o: toojpeg.cpp toojpeg.h
	g++ $(CPPFLAGS) -c toojpeg.cpp

main.o: main.cpp
	g++ $(CPPFLAGS) -c main.cpp
