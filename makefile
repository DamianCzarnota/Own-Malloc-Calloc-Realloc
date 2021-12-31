alok: main.o custom_unistd.o 
	gcc -g -O0 -Wall $^ -o $@ -lpthread

main.o: main.c custom_unistd.h 
	gcc -g -c -O0 -Wall $< -o $@ -lpthread

custom_unistd.o: custom_unistd.c custom_unistd.h
	gcc -g -c -O0 -Wall $< -o $@ -lpthread

.PHONY: clean

clean:
	-rm main.o custom_unistd.o alok
