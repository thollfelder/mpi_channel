#!/bin/bash

#SBATCH --job-name job_001                              # Specifies name for job allocation; default is script name
#SBATCH --output job.%j.out 
#SBATCH --nodes 2                                       # Required number of nodes
#SBATCH --ntasks-per-node 2                             # Maximum processes per node
#SBATCH --time 00:05:00                                 # Sets a time limit
#SBATCH --nodelist node03,node04                        # Requests a specific list of hosts
#SBATCH --exclusive                                     # Job allocation does not share node with others

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

module load openmpi/4.1.1-ucx-no-verbs-no-libfabric
#echo "Module $mpi_impl1 loaded"
#module load $gcc_comp
#echo "Module $gcc_comp loaded"

# clean all .o files
find . -type f -name '*.o' -exec rm {} +
#echo "Cleaned binaries"

# compile with loaded compiler
#make
#ech "Compiled new"

# create file for measurements
date_today=$(date -d yesterday '+%Y-%m-%d-%T')
file_name="measurements-"$date_today.csv
header="chantype, com_mech, num_procs, num_prod, num_cons, iterations, capacity, is_receiver, rank, byte, byte_indi, time, bandwidth, implementation"
echo $header > $file_name
echo "File $file_name created"
echo "Starting measurements..."

mpirun -hostlist $SLURM_JOB_NODELIST -np $procs ./Test --type PT2PT --capacity $cap --producers $prod --receivers $rec --msg_num $msgs --iterations $iter --file_name $file_name --implementation $mpi_impl1

echo "Finished measurements..."

