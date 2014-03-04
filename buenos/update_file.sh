cd tests
make
cd ..
./util/tfstool delete store.file $1
./util/tfstool write store.file tests/$1 $1
