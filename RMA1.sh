module purge
module load openmpi/4.1.1-ucx-no-verbs-no-libfabric
find . -type f -name '*.o' -exec rm {} +
make

mpirun -np 3 ./Test --type RMA --capacity 10 --producers 2 --receivers 1 --msg_num 1200000 --iterations 1 --file_name RMA1 --implementation openmpi/4.1.1-ucx-no-verbs-no-libfabric
mpirun -np 4 ./Test --type RMA --capacity 10 --producers 3 --receivers 1 --msg_num 1200000 --iterations 1 --file_name RMA1 --implementation openmpi/4.1.1-ucx-no-verbs-no-libfabric
mpirun -np 5 ./Test --type RMA --capacity 10 --producers 4 --receivers 1 --msg_num 1200000 --iterations 1 --file_name RMA1 --implementation openmpi/4.1.1-ucx-no-verbs-no-libfabric
mpirun -np 6 ./Test --type RMA --capacity 10 --producers 5 --receivers 1 --msg_num 1200000 --iterations 1 --file_name RMA1 --implementation openmpi/4.1.1-ucx-no-verbs-no-libfabric
mpirun -np 7 ./Test --type RMA --capacity 10 --producers 6 --receivers 1 --msg_num 1200000 --iterations 1 --file_name RMA1 --implementation openmpi/4.1.1-ucx-no-verbs-no-libfabric
mpirun -np 8 ./Test --type RMA --capacity 10 --producers 7 --receivers 1 --msg_num 1200000 --iterations 1 --file_name RMA1 --implementation openmpi/4.1.1-ucx-no-verbs-no-libfabric
mpirun -np 9 ./Test --type RMA --capacity 10 --producers 8 --receivers 1 --msg_num 1200000 --iterations 1 --file_name RMA1 --implementation openmpi/4.1.1-ucx-no-verbs-no-libfabric
