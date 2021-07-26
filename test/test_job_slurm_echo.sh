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

# starts local session on node03 to compile
srun --nodes=2 --ntasks-per-node=16 --time=02:00:00 --exclusive --nodelist node03,node04 --pty bash -i

mpi_impl1="openmpi/4.1.1-ucx-no-verbs-no-libfabric"
mpi_impl2="mpich/3.4"
gcc_comp="gcc/10.3.0"

module load $mpi_impl1
echo "Module $mpi_impl1 loaded"
module load $gcc_comp
echo "Module $gcc_comp loaded"

# clean all .o files
find . -type f -name '*.o' -exec rm {} +
echo "Cleaned binaries"

# compile with loaded compiler
mpicc MPI_Hello_World.c -o HelloWorld
echo "Compiled new"

echo "Starting mpi program..."
mpirun --mca osc ucx -np 4 -host $SLURM_JOB_NODELIST HelloWorld 
mpiexec --mca osc ucx -np 4 -host $SLURM_JOB_NODELIST HelloWorld 
echo "Finished mpi program"

# ends local session on node03
exit



srun --nodes=2 --ntasks-per-node=1 --time=02:00:00 --exclusive --nodelist node03,node04 --pty bash -i
module load openmpi/4.1.1-ucx-no-verbs-no-libfabric


# MCA osc: sm (MCA v2.1.0, API v3.0.0, Component v4.1.1)
# MCA osc: monitoring (MCA v2.1.0, API v3.0.0, Component v4.1.1)
# MCA osc: pt2pt (MCA v2.1.0, API v3.0.0, Component v4.1.1)
# MCA osc: rdma (MCA v2.1.0, API v3.0.0, Component v4.1.1)
# MCA osc: ucx (MCA v2.1.0, API v3.0.0, Component v4.1.1)