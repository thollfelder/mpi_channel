# MPI_Channel #

This is an implementation of channels builds on MPI. It offers implementations of different queues (SPSC, MPSC, MPMC) 
with both communication mechanisms MPI offers (two-sided communication/pt2pt and one-sided communication/rma).

# BUGS #

- openmpi/4.1.1-ucx-no-verbs-no-libfabric
- mpich/3.4 

- PT2PT MPMC BUF:
    - Läuft aufgrund der Implementierung nicht durch!

- RMA läuft mit OpenMPI alles durch