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

echo 'mpirun version:'
mpirun -version

date
squeue # Check Queue
echo

lib_root="/home_ai2/sthollfelder/bin/mpi_channel"
job_root="$lib_root/ClusterWork/Bachelor_tests/ai2srv"

echo "lib_root: $lib_root"
echo "job_root: $job_root"

cd $lib_root

# Load Modules
module load mpich/3.4
module load gcc/10.3.0

# Build libecho "start Build"

export PERL5LIB=$PERL5LIB:. 
./build all >& $lib_root/test/build.out
make headers >& $lib_root/test/make_headers.out
make dep >& $lib_root/test/make_dep.out
make >& $lib_root/test/make.out
export SOLVE_TABLE='Y0 YN YMIN YMAX IMPL KERNEL N BF METHOD H T0 STEPS NT NP ODE SOLVER BLOCKSIZE'

echo "Done Build"


lib_root="/home_ai2/sttasic/lib/bachlor_code"
job_root="$lib_root/ClusterWork/Bachelor_tests/ai2srv"

# echo "lib_root: $lib_root"
# echo "job_root: $job_root"

cd $lib_root

if [ ! -d "$job_root/OUTPUTS" ]; then
	mkdir "$job_root/OUTPUTS"
fi
if [ ! -d "$job_root/timetable" ]; then
	mkdir "$job_root/timetable"
fi
if [ ! -d "$job_root/timetable/PMD" ]; then
	mkdir "$job_root/timetable/PMD"
fi

BTABLE="AB1 AB4 AB6 RK1 RK4 RKMIX Preditor-corector_based_Randau_IA(3)"
IMPL="impl_001 impl_002 impl_003 impl_005 impl_006 impl_007 impl_008"
Processors="1 4 8"
ODE="BRUSS2D-MIX"

#N bei BRUSS2D-MIX -> 2xN² = 131072 (n=256)
#N bei HEAT2D-Case -> N² = 131044 (n=362)
NEND=260
NBEGINN=10
NINCREMENT=10
N=10

BSEND=8
BSBEGINN=8
BSINCREMENT=10
BS=8

for ode in $ODE; do
	if [ ! -d "$job_root/timetable/PMD/$ode" ]; then
		mkdir "$job_root/timetable/PMD/$ode"
	fi
	if [ ! -d "$job_root/OUTPUTS/$ode" ]; then
		mkdir "$job_root/OUTPUTS/$ode"
	fi
	for btable in $BTABLE; do 
		# echo "$btable"
		if [ ! -d "$job_root/timetable/PMD/$ode/$btable" ]; then
			mkdir "$job_root/timetable/PMD/$ode/$btable"
		fi
		if [ ! -d "$job_root/OUTPUTS/$ode/$btable" ]; then
			mkdir "$job_root/OUTPUTS/$ode/$btable"
		fi
		for impl in $IMPL; do 
			for (( c = $NBEGINN ; c <= $NEND ; c+=$NINCREMENT )) ; do # Das passt nun
				# echo $c
				for (( d = $BSBEGINN ; d <= $BSEND ; d+=$BSINCREMENT )) ; do
					for np in $Processors; do 
						mpirun -hostlist $SLURM_JOB_NODELIST -np $np ./bin/solve -solver:dmGLM -impl:$impl -ode:$ode -N:$c -method:$btable -h:1e-10 -H:1e-8 -t0:0 -bs:$d -time -table:"$job_root/timetable/PMD/$ode/$btable/time_${impl}_P$np.pmd" >& "$job_root/OUTPUTS/$ode/$btable/output_${impl}_P$np.out"
					done	
				done
			done
		done
	done
done


# $job_root/Job_build.sh 



# srun --nodes=1 --ntasks-per-node=1 --time=02:00:00 --exclusive --pty bash -i

# srun --nodes=1 --ntasks-per-node=1 --time=02:00:00 --exclusive --nodelist node07 --pty bash -i