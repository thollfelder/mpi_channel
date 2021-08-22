#!/bin/bash
#SBATCH --job-name job_001                              # Specifies name for job allocation; default is script name
#SBATCH --output job.%j.out 
#SBATCH --nodes 2                                       # Required number of nodes
#SBATCH --ntasks-per-node 1                             # Maximum processes per node
#SBATCH --time 00:30:00                                 # Sets a time limit
#SBATCH --exclusive                                     # Job allocation does not share node with others

echo "SLURM_CLUSTER_NAME:$SLURM_CLUSTER_NAME"
echo "SLURM_CPU_PER_TASK:$SLURM_CPU_PER_TASK"
echo "SLURM_JOB_ID | SLURM_JOB_NAME:$SLURM_JOB_ID | $SLURM_JOB_NAME"
echo "SLURM_JOB_NODELIST:$SLURM_JOB_NODELIST"
echo "SLURM_JOB_NUM_NODES:$SLURM_JOB_NUM_NODES"
echo "HOST:$HOST"
# starts local session on node03 to compile
#srun --nodes=2 --ntasks-per-node=16 --time=02:00:00 --exclusive --nodelist node03,node04 --pty bash -i
srun --nodes=2 --ntasks=2 --ntasks-per-node=1 --exclusive --pty bash -i 

mpi_impl1="openmpi/4.1.1-ucx-no-verbs-no-libfabric"
gcc_comp="gcc/10.3.0"
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

cap="2"
procs="2"
prod=1
rec=1
msgs=100000
iter=1

# create file for measurements
date_today=$(date -d yesterday '+%Y-%m-%d-%T')
file_name="measurements-"$date_today.csv
header="com_mech,chantype,num_procs,num_prod,num_cons,iterations,capacity,is_receiver,rank,byte,byte_indi,time,bandwidth,implementation"
echo $header > $file_name
echo "File $file_name created"
echo "Starting measurements..."
mpirun -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 300000 --iterations 1 --file_name asdasd --implementation impl
mpirun -np 2 ./Test --type RMA --capacity 0 --producers 1 --receivers 1 --msg_num 300000 --iterations 1 --file_name asdasd --implementation impl
mpirun -np 2 ./Test --type PT2PT --capacity 2 --producers 1 --receivers 1 --msg_num 300000 --iterations 1 --file_name asdasd --implementation impl
mpirun -np 2 ./Test --type RMA --capacity 2 --producers 1 --receivers 1 --msg_num 300000 --iterations 1 --file_name asdasd --implementation impl
echo "Finished measurements..."

# ends local session 
exit

srun --nodes=2 --ntasks=2 --ntasks-per-node=1 --exclusive --pty mpirun -np 2 ./Test --type RMA --capacity 2 --producers 1 --receivers 1 --msg_num 4 --iterations 1 --file_name asdasd --implementation impl

srun --nodes=2 --ntasks=2 --ntasks-per-node=1 --exclusive --pty mpirun -np 2 ./Test --type RMA --capacity 2 --producers 1 --receivers 1 --msg_num 4 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=2 --ntasks-per-socket=2 --exclusive --pty mpirun -np 2 ./Test --type RMA --capacity 2 --producers 1 --receivers 1 --msg_num 4 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=2 --ntasks-per-core=2 --exclusive --pty mpirun -np 2 ./Test --type RMA --capacity 2 --producers 1 --receivers 1 --msg_num 4 --iterations 1 --file_name asdasd --implementation impl


srun --nodes=1 --ntasks=2 --ntasks-per-socket=2 --exclusive --pty mpirun -np 2 ./Test --type RMA --capacity 2 --producers 1 --receivers 1 --msg_num 4 --iterations 1 --file_name asdasd --implementation impl


# Shared Nodes
srun --nodes=1 --ntasks=2 --ntasks-per-socket=2 --exclusive --pty mpirun -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=2 --ntasks-per-socket=2 --exclusive --pty mpirun -np 2 ./Test --type RMA   --capacity 0 --producers 1 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=2 --ntasks-per-socket=2 --exclusive --pty mpirun -np 2 ./Test --type PT2PT --capacity 2 --producers 1 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=2 --ntasks-per-socket=2 --exclusive --pty mpirun -np 2 ./Test --type RMA   --capacity 2 --producers 1 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=3 --ntasks-per-socket=2 --exclusive --pty mpirun -np 3 ./Test --type PT2PT --capacity 0 --producers 2 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=3 --ntasks-per-socket=2 --exclusive --pty mpirun -np 3 ./Test --type RMA   --capacity 0 --producers 2 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=3 --ntasks-per-socket=2 --exclusive --pty mpirun -np 3 ./Test --type PT2PT --capacity 2 --producers 2 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=3 --ntasks-per-socket=2 --exclusive --pty mpirun -np 3 ./Test --type RMA   --capacity 2 --producers 2 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=4 --ntasks-per-socket=2 --exclusive --pty mpirun -np 4 ./Test --type PT2PT --capacity 0 --producers 2 --receivers 2 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=4 --ntasks-per-socket=2 --exclusive --pty mpirun -np 4 ./Test --type RMA   --capacity 0 --producers 2 --receivers 2 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
#srun --nodes=1 --ntasks=4 --ntasks-per-socket=2 --exclusive --pty mpirun -np 4 ./Test --type PT2PT --capacity 2 --producers 2 --receivers 2 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=1 --ntasks=4 --ntasks-per-socket=2 --exclusive --pty mpirun -np 4 ./Test --type RMA   --capacity 2 --producers 2 --receivers 2 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl

# Getrennte Nodes
srun --nodes=2 --ntasks=2 --ntasks-per-socket=1 --exclusive --pty mpirun -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=2 --ntasks=2 --ntasks-per-socket=1 --exclusive --pty mpirun -np 2 ./Test --type RMA   --capacity 0 --producers 1 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=2 --ntasks=2 --ntasks-per-socket=1 --exclusive --pty mpirun -np 2 ./Test --type PT2PT --capacity 2 --producers 1 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
#srun --nodes=2 --ntasks=2 --ntasks-per-socket=1 --exclusive --pty mpirun -np 2 ./Test --type RMA   --capacity 2 --producers 1 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=2 --ntasks=3 --ntasks-per-socket=1 --exclusive --pty mpirun -np 3 ./Test --type PT2PT --capacity 0 --producers 2 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
#srun --nodes=2 --ntasks=3 --ntasks-per-socket=1 --exclusive --pty mpirun -np 3 ./Test --type RMA   --capacity 0 --producers 2 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=2 --ntasks=3 --ntasks-per-socket=1 --exclusive --pty mpirun -np 3 ./Test --type PT2PT --capacity 2 --producers 2 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
#srun --nodes=2 --ntasks=3 --ntasks-per-socket=1 --exclusive --pty mpirun -np 3 ./Test --type RMA   --capacity 2 --producers 2 --receivers 1 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
srun --nodes=2 --ntasks=4 --ntasks-per-socket=1 --exclusive --pty mpirun -np 4 ./Test --type PT2PT --capacity 0 --producers 2 --receivers 2 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
#srun --nodes=2 --ntasks=4 --ntasks-per-socket=1 --exclusive --pty mpirun -np 4 ./Test --type RMA   --capacity 0 --producers 2 --receivers 2 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
#srun --nodes=2 --ntasks=4 --ntasks-per-socket=1 --exclusive --pty mpirun -np 4 ./Test --type PT2PT --capacity 2 --producers 2 --receivers 2 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl
#srun --nodes=2 --ntasks=4 --ntasks-per-socket=1 --exclusive --pty mpirun -np 4 ./Test --type RMA   --capacity 2 --producers 2 --receivers 2 --msg_num 30000 --iterations 1 --file_name asdasd --implementation impl




module purge; module load openmpi/4.1.0-ucx-no-verbs-no-libfabric; find . -type f -name '*.o' -exec rm {} +; make; srun --nodes=1 --ntasks=2 --ntasks-per-socket=2 --exclusive --pty mpirun -np 2 ./Test --type PT2PT --capacity 10 --producers 1 --receivers 1 --msg_num 300000 --iterations 10 --file_name asdasd --implementation impl

   gcc/8.3.0     gcc/11.1.0      (D)    openmpi/4.0.2                                  papi/5.6.0
   gcc/8.4.0     libfabric/1.9.0        openmpi/4.1.0-libfabric                        papi/5.7.0
   gcc/8.5.0     likwid/5.0.0           openmpi/4.1.0-ucx-no-verbs-no-libfabric        papi/6.0.0
   gcc/9.2.0     likwid/5.1.0    (D)    openmpi/4.1.0-ucx-no-verbs                     ucx/1.9.0
   gcc/9.3.0     mpich/3.3.2-ofi        openmpi/4.1.0-ucx                              ucx/1.10.1      (D)
   gcc/10.2.0    mpich/3.3.2            openmpi/4.1.0                                  uncrustify/0.67
   gcc/10.3.0    mpich/3.4       (D)    openmpi/4.1.1-ucx-no-verbs-no-libfabric (D)


   mpirun -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 3000000 --iterations 5 --file_name asdasd --implementation impl



module purge; module load openmpi/4.1.0-ucx-no-verbs-no-libfabric; find . -type f -name '*.o' -exec rm {} +; make; mpirun -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 300000 --iterations 5 --file_name asdasd --implementation impl


module purge; module load openmpi/4.1.0-ucx-no-verbs-no-libfabric; find . -type f -name '*.o' -exec rm {} +; make; mpiexec -np 4 -host node01,node02 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 3000000 --iterations 5 --file_name asdasd --implementation impl

mpiexec -np 2 -host node01 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 3000000 --iterations 5 --file_name asdasd --implementation impl

mpirun --hostfile hostfile -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 3000000 --iterations 5 --file_name asdasd --implementation impl
