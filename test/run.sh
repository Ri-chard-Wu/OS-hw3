
cd ~/NachOS-4.0_MP3/code/build.linux
make clean
rm nachos
make



cd ~/NachOS-4.0_MP3/code/test
# cp /home/os2023/share/hw3t{1,2,3}.c ./
make clean
make hw3t1 hw3t2 hw3t3
../build.linux/nachos -e hw3t1 -e hw3t2 > run.out