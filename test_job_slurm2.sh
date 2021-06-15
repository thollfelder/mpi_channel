#!/bin/bash

#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --time=00:10:00
#SBATCH --partition=shas-testing
#SBATCH --output=sample-%j.out

echo "== This is the scripting step! =="
sleep 30
echo "== End of Job =="