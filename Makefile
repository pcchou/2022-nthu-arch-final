all : project

project : final.cpp
	g++ final.cpp -o project

clean :
	rm -f project
