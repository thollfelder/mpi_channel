#!/bin/bash

mpirun -np 2 ./Test --type PT2PT --capacity 0 --producers 1 --receivers 1 --msg_num 10000 --iterations 5
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