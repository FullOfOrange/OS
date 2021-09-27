mytop: mytop.o
		gcc -o mytop mytop.o -lncurses -g

mytop.o: mytop.c
		gcc -c mytop.c -lncurses -g

myps: myps.o
		gcc -o myps myps.o -g

myps.o: myps.c
		gcc -c myps.c -g

mylscpu: mylscpu.o
		gcc -o mylscpu mylscpu.o -g

mylscpu.o: mytop.c
		gcc -c mylscpu.c -g