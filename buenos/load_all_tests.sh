cd tests
make
cd ..

rm store.file
./util/tfstool create store.file 4000 testi 
for f in tests/*.c; 
do  
	name="${f##*/}"
	name="${name%.*}"
	./util/tfstool delete store.file $name
	./util/tfstool write store.file tests/$name $name
done
