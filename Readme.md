# MPI Channel #

This channel implementation originated from my bachelor thesis "Design und Implementierung von Channels im
verteilten Adressraum" (eng. Design and Implementation of Channels for distributed address space) at the University of 
Bayreuth, and is intended to be used on computer systems with distributed address space using MPI as a communication library.

# Implementation #

The channels can be classified by the number of sender and receivers (SPSC, MPSC, MPMC), the channel capacity (buffered
and asynchronous or unbuffered and synchronous) and the underlying communication (MPI PT2PT or RMA).

# Tested versions #

- openmpi/4.1.1
- mpich/3.4 

# Known bugs #

No one right now :)