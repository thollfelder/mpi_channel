#!/bin/bash


cap=0
procs=2
prod=1
rec=1
msgs=1000000
iter=10

mpi_impl1="openmpi/4.1.1-ucx-no-verbs-no-libfabric"
mpi_impl2="mpich/3.4"
gcc_comp="gcc/10.3.0"

module load $mpi_impl1
echo "Module $mpi_impl1 loaded"
module load $gcc_comp
echo "Module $gcc_comp loaded"

# create file for measurements
date_today=$(date -d yesterday '+%Y-%m-%d-%T')
file_name="measurements-"$date_today.csv
header="chantype, com_mech, num_procs, num_prod, num_cons, iterations, capacity, is_receiver, rank, byte, byte_indi, time, bandwidth, implementation"
echo $header > $file_name
echo "File $file_name created"
echo "Starting measurements..."

mpirun -np $procs ./Test --type PT2PT --capacity $cap --producers $prod --receivers $rec --msg_num $msgs --iterations $iter --file_name $file_name --implementation $impl









#mpirun -np 2 ./Test --type PT2PT --capacity 1 --producers 1 --receivers 1 --msg_num 10000 --iterations 5
#mpirun -np 3 ./Test --type PT2PT --capacity 0 --producers 2 --receivers 1 --msg_num 10000 --iterations 5
#mpirun -np 3 ./Test --type PT2PT --capacity 1 --producers 2 --receivers 1 --msg_num 10000 --iterations 5
#mpirun -np 4 ./Test --type PT2PT --capacity 0 --producers 2 --receivers 2 --msg_num 10000 --iterations 5
#mpirun -np 4 ./Test --type PT2PT --capacity 1 --producers 2 --receivers 2 --msg_num 10000 --iterations 5

#mpirun -np 2 ./Test --type RMA --capacity 0 --producers 1 --receivers 1 --msg_num 10000 --iterations 5
#mpirun -np 2 ./Test --type RMA --capacity 1 --producers 1 --receivers 1 --msg_num 10000 --iterations 5
#mpirun -np 3 ./Test --type RMA --capacity 0 --producers 2 --receivers 1 --msg_num 10000 --iterations 5
#mpirun -np 3 ./Test --type RMA --capacity 1 --producers 2 --receivers 1 --msg_num 10000 --iterations 5
#mpirun -np 4 ./Test --type RMA --capacity 0 --producers 2 --receivers 2 --msg_num 10000 --iterations 5
#mpirun -np 4 ./Test --type RMA --capacity 1 --producers 2 --receivers 2 --msg_num 10000 --iterations 5

#Processors="1 4 8"
#
#			for (( c = $NBEGINN ; c <= $NEND ; c+=$NINCREMENT )) ; do # Das passt nun
#				# echo $c
#				for (( d = $BSBEGINN ; d <= $BSEND ; d+=$BSINCREMENT )) ; do
#					for np in $Processors; do 
#						mpirun -hostlist $SLURM_JOB_NODELIST -np $np ./bin/solve -solver:dmGLM -impl:$impl -ode:$ode -N:$c -method:$btable -h:1e-10 -H:1e-8 -t0:0 -bs:$d -time -table:"$job_root/timetable/PMD/$ode/$btable/time_${impl}_P$np.pmd" >& "$job_root/OUTPUTS/$ode/$btable/output_${impl}_P$np.out"
#					done	
#				done
#			done