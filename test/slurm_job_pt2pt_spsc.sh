#!/bin/bash

#SBATCH --job-name job_001                              # Specifies name for job allocation; default is script name
#SBATCH --output job.%j.out 
#SBATCH --nodes 1                                       # Required number of nodes
#SBATCH --ntasks-per-node 2                             # Maximum processes per node
#SBATCH --time 02:00:00                                 # Sets a time limit
#SBATCH --nodelist node03                               # Requests a specific list of hosts
#SBATCH --exclusive  

# starts local session on node03
srun --nodes=1 --ntasks-per-node=1 --time=02:00:00 --exclusive --nodelist node03 --pty bash -i

echo "SLURM_CLUSTER_NAME:$SLURM_CLUSTER_NAME"
echo "SLURM_CPU_PER_TASK:$SLURM_CPU_PER_TASK"
echo "SLURM_JOB_ID | SLURM_JOB_NAME:$SLURM_JOB_ID | $SLURM_JOB_NAME"
echo "SLURM_JOB_NODELIST:$SLURM_JOB_NODELIST"
echo "SLURM_JOB_NUM_NODES:$SLURM_JOB_NUM_NODES"
echo "HOST:$HOST"

cap="0 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192"
procs=2
prod=1
rec=1
msgs=2500000
iter=10

mpi_impl1="openmpi/4.1.1-ucx-no-verbs-no-libfabric"
mpi_impl2="mpich/3.4"
gcc_comp="gcc/10.3.0"

# unload all
module purge

module load $mpi_impl1
echo "Module $mpi_impl1 loaded"
module load $gcc_comp
echo "Module $gcc_comp loaded"

# clean all .o files
find . -type f -name '*.o' -exec rm {} +
echo "Cleaned binaries"

# compile with loaded compiler
make
echo "Compiled new"

# create file for measurements
date_today=$(date -d yesterday '+%Y-%m-%d-%T')
file_name="measurements-"$date_today.csv
header="chantype,com_mech,num_procs,num_prod,num_cons,iterations,capacity,is_receiver,rank,byte,byte_indi,time,bandwidth,implementation,node"
echo $header > $file_name
echo "File $file_name created"
echo "Starting measurements..."

for ca in $cap; do 
    mpirun -np $procs ./Test --type PT2PT --capacity $ca --producers $prod --receivers $rec --msg_num $msgs --iterations $iter --file_name $file_name --implementation $mpi_impl1
done

# use another compiler
module unload $mpi_impl1
echo "Module $mpi_impl1 unloaded"
module load $mpi_impl2
echo "Module $mpi_impl2 loaded"

# clean all .o files
find . -type f -name '*.o' -exec rm {} +
echo "Cleaned binaries"

# compile with loaded compiler
make
echo "Compiled new"

for ca in $cap; do 
    mpirun -np $procs ./Test --type PT2PT --capacity $ca --producers $prod --receivers $rec --msg_num $msgs --iterations $iter --file_name $file_name --implementation $mpi_impl2
done

echo "Finished measurements..."

# ends local session on node03
exit