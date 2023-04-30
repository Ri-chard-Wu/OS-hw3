
cd ~/NachOS-4.0_MP3/code/build.linux
make clean
rm nachos
make



cd ~/NachOS-4.0_MP3/code/test
make clean
make hw3t1 hw3t2 hw3t3
../build.linux/nachos -d z -e hw3t1 -e hw3t2 -e hw3t3 2> stderr.out