#!/bin/bash

date_today=$(date -d yesterday '+%Y_%m_%d_%H_%M_%S')
file_name="measurements_"$date_today.csv
header="chantype,com_mech,num_procs,num_prod,num_cons,iterations,capacity,is_receiver,rank,byte,byte_indi,time,bandwidth,implementation"
echo $header > $file_name

mpirun -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 1 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 2 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 4 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 8 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 16 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 32 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 64 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 128 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 256 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 512 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI
mpirun -np 2 ./Test --type PT2PT --capacity 1024 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name $file_name --implementation OpenMPI


mpirun -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 1000000 --iterations 10 --file_name dump --implementation OpenMPI


mpirun --mca btl_openib_allow_ib 1 -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 100000 --iterations 2 --file_name $file_name --implementation OpenMPI
