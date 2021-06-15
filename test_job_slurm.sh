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

mpirun -hostlist $SLURM_JOB_NODELIST -np $procs ./Test --type PT2PT --capacity $cap --producers $prod --receivers $rec --msg_num $msgs --iterations $iter --file_name $file_name --implementation $impl

echo "Finished measurements..."

