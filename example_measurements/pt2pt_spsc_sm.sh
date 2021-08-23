#!/bin/bash

#SBATCH --job-name job_001                              # Specifies name for job allocation; default is script name
#SBATCH --output job.%j.out 
#SBATCH --nodes 1                                       # Required number of nodes
#SBATCH --ntasks 16                                       # Required number of nodes
#SBATCH --ntasks-per-node 16                             # Maximum processes per node
#SBATCH --time 01:00:00                                 # Sets a time limit
#SBATCH --exclusive                                     # Job allocation does not share node with others

echo "SLURM_CLUSTER_NAME:$SLURM_CLUSTER_NAME"
echo "SLURM_CPU_PER_TASK:$SLURM_CPU_PER_TASK"
echo "SLURM_JOB_ID | SLURM_JOB_NAME:$SLURM_JOB_ID | $SLURM_JOB_NAME"
echo "SLURM_JOB_NODELIST:$SLURM_JOB_NODELIST"
echo "SLURM_JOB_NUM_NODES:$SLURM_JOB_NUM_NODES"
echo "HOST:$HOST"

# starts local session on node03 to compile
#srun --nodes=2 --ntasks-per-node=16 --time=02:00:00 --exclusive --nodelist node03,node04 --pty bash -i

# loading modules
mpi_impl1="openmpi/4.1.1-ucx-no-verbs-no-libfabric"
gcc_comp="gcc/11.1.0"
module load $mpi_impl1
echo "Module $mpi_impl1 loaded"
module load $gcc_comp
echo "Module $gcc_comp loaded"
module load libfabric/1.9.0
echo "Module libfabric/1.9.0 loaded"
module load ucx/1.10.1
echo "Module ucx/1.10.1 loaded"

# clean all .o files
find . -type f -name '*.o' -exec rm {} +
echo "Cleaned binaries"

# compile with loaded compiler
make
echo "Compiled new"

# SPSC SYNC AND BUF
cap="0 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192"
procs=2
prod=1
rec=1
msgs=300000
iter=1
# create file for measurements
date_today=$(date -d yesterday '+%Y-%m-%d-%T')
file_name="PT2PT-SPSC-SM-"$date_today.csv
header="com_mech,chantype,num_procs,num_prod,num_cons,iterations,capacity,is_receiver,rank,byte,byte_indi,time,bandwidth,implementation"
echo $header > $file_name
echo "File $file_name created"
echo "Starting measurements..."

# PT2PT SPSC BUF AND SYNC
for ca in $cap; do 
    mpirun -np 2 ./Test --type PT2PT --capacity $ca --producers 1 --receivers 1 --msg_num $msgs --iterations 1 --file_name $file_name --implementation $mpi_impl1
done

echo "Finished measurements..."
