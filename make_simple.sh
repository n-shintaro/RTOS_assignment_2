rmmod simple.ko
make
insmod simple.ko
mknod /dev/simple c 509 0
g++ task6.c -pthread -o task6
./task6
