#!/bin/bash

#SBATCH --job-name job_001                              # Specifies name for job allocation; default is script name
#SBATCH --output job.%j.out 
#SBATCH --nodes 1                                       # Required number of nodes
#SBATCH --ntasks-per-node 16                            # Maximum processes per node
#SBATCH --time 10:00:00                                 # Sets a time limit
#SBATCH --nodelist node03                               # Requests a specific list of hosts
#SBATCH --exclusive  

# starts local session on node03
srun --nodes=1 --ntasks-per-node=16 --time=10:00:00 --exclusive --nodelist node03 --pty bash -i

echo "SLURM_CLUSTER_NAME:$SLURM_CLUSTER_NAME"
echo "SLURM_CPU_PER_TASK:$SLURM_CPU_PER_TASK"
echo "SLURM_JOB_ID | SLURM_JOB_NAME:$SLURM_JOB_ID | $SLURM_JOB_NAME"
echo "SLURM_JOB_NODELIST:$SLURM_JOB_NODELIST"
echo "SLURM_JOB_NUM_NODES:$SLURM_JOB_NUM_NODES"
echo "HOST:$HOST"

cap="64 128 256 512 1024 2048 4096 8192"
procs="16"
prod=1
rec=1
msgs=1000000
iter=10

mpi_impl1="openmpi/4.1.1-ucx-no-verbs-no-libfabric"
mpi_impl2="mpich/3.4"
gcc_comp="gcc/10.3.0"

# unload all
module purge

# use another compiler
module load $mpi_impl2
echo "Module $mpi_impl2 loaded"
module load $gcc_comp
echo "Module $gcc_comp loaded"

# clean all .o files
find . -type f -name '*.o' -exec rm {} +
echo "Cleaned binaries"

# compile with loaded compiler
make
echo "Compiled new"

for pro in $procs; do
    for ca in $cap; do 
        mpirun -np $pro ./Test --type PT2PT --capacity $ca --producers $(($pro-$prod)) --receivers $rec --msg_num $msgs --iterations $iter --file_name $file_name --implementation $mpi_impl2
    done
done

echo "Finished measurements..."

# ends local session on node03
exit
