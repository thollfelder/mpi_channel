#!/bin/bash

#SBATCH --job-name job_001                              # Specifies name for job allocation; default is script name
#SBATCH --output job.%j.out 
#SBATCH --nodes 2                                       # Required number of nodes
#SBATCH --ntasks-per-node 8                             # Maximum processes per node
#SBATCH --time 00:05:00                                 # Sets a time limit
#SBATCH --nodelist node03,node04                       # Requests a specific list of hosts
#SBATCH --exclusive                                     # Job allocation does not share node with others

echo "SLURM_CLUSTER_NAME:$SLURM_CLUSTER_NAME"
echo "SLURM_CPU_PER_TASK:$SLURM_CPU_PER_TASK"
echo "SLURM_JOB_ID | SLURM_JOB_NAME:$SLURM_JOB_ID | $SLURM_JOB_NAME"
echo "SLURM_JOB_NODELIST:$SLURM_JOB_NODELIST"
echo "SLURM_JOB_NUM_NODES:$SLURM_JOB_NUM_NODES"
echo "HOST:$HOST"



echo "== This is the scripting step! =="
sleep 10
echo "== End of Job =="