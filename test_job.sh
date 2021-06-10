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
