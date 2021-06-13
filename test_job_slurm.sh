#!/bin/bash
#SBATCH -J job_001
#SBATCH -o job.%j.out 
#SBATCH -N 1 
#SBATCH --ntasks-per-node 8 
#SBATCH -t 01:30:00 
#SBATCH --nodelist node20
#SBATCH --exclusive

echo "SLURM_CLUSTER_NAME:$SLURM_CLUSTER_NAME"
echo "SLURM_CPU_PER_TASK:$SLURM_CPU_PER_TASK"
echo "SLURM_JOB_ID | SLURM_JOB_NAME:$SLURM_JOB_ID | $SLURM_JOB_NAME"
echo "SLURM_JOB_NODELIST:$SLURM_JOB_NODELIST"
echo "SLURM_JOB_NUM_NODES:$SLURM_JOB_NUM_NODES"
echo "HOST:$HOST"

# Load Modules
# module load mpich/3.4
module load openmpi/4.1.1-ucx-no-verbs-no-libfabric
module load gcc/10.3.0
echo `modules loaded`

date_today=$(date -d yesterday '+%Y-%m-%d-%T')
file_name="measurements-"$date_today.csv
header="chantype, com_mech, num_procs, num_prod, num_cons, iterations, capacity, is_receiver, rank, byte, byte_indi, time, bandwidth"
echo $header > $file_name

mpirun -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 10000 --iterations 5 --file_name $file_name
mpirun -np 2 ./Test --type PT2PT --capacity 1 --producers 1 --receivers 1 --msg_num 10000 --iterations 5
mpirun -np 3 ./Test --type PT2PT --capacity 0 --producers 2 --receivers 1 --msg_num 10000 --iterations 5
mpirun -np 3 ./Test --type PT2PT --capacity 1 --producers 2 --receivers 1 --msg_num 10000 --iterations 5
mpirun -np 4 ./Test --type PT2PT --capacity 0 --producers 2 --receivers 2 --msg_num 10000 --iterations 5
mpirun -np 4 ./Test --type PT2PT --capacity 1 --producers 2 --receivers 2 --msg_num 10000 --iterations 5

mpirun -np 2 ./Test --type RMA --capacity 0 --producers 1 --receivers 1 --msg_num 10000 --iterations 5
mpirun -np 2 ./Test --type RMA --capacity 1 --producers 1 --receivers 1 --msg_num 10000 --iterations 5
mpirun -np 3 ./Test --type RMA --capacity 0 --producers 2 --receivers 1 --msg_num 10000 --iterations 5
mpirun -np 3 ./Test --type RMA --capacity 1 --producers 2 --receivers 1 --msg_num 10000 --iterations 5
mpirun -np 4 ./Test --type RMA --capacity 0 --producers 2 --receivers 2 --msg_num 10000 --iterations 5
mpirun -np 4 ./Test --type RMA --capacity 1 --producers 2 --receivers 2 --msg_num 10000 --iterations 5

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