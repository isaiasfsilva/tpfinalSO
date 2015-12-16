all: fs main
	gcc main.o fs.o -o a.out -g
fs: fs.c
	gcc -c fs.c -o fs.o -g
main:
	gcc -c main.c -o main.o -g
newfile:
	dd bs=64MB count=1 if=/dev/zero of=disk.img
checkdisk:
	hexdump -v -e '/4 "%08X\n"' disk.img