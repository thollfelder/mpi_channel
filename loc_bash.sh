cap="0 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192"
procs="3 4 5 6 7 8 9 10 11 12 13 14 15 16"
prod=1
rec=1
msgs=3000
iter=10

for pro in $procs; do
    for ca in $cap; do 
        mpirun --oversubscribe -np $pro ./Test --type PT2PT --capacity $ca --producers $(($pro-$prod)) --receivers $rec --msg_num $msgs --iterations $iter --file_name $file_name --implementation $mpi_impl1
    done
done