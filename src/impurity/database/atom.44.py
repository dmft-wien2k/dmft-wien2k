n=[2,3,4,5,6] # Atomic valences
nc=[2,3,4,5,6] # Atomic valences for ctqmc
l=2           # Orbital angular momentum of the shel
J=0.35        # Slatter integrals F2=J*11.9219653179191 from the atomic physics program
cx=0.0        # spin-orbit coupling from the atomic physics program
qOCA=1        # OCA diagrams are computes in addition to NCA diagrams
Eoca=1.       # If the atomi energy of any state, involved in the diagram, is higher that Eoca from the ground state, the diagram ms is neglected
mOCA=1e-3     # If maxtrix element of OCA diagram is smaller, it is neglected 
Ncentral=[4]  # OCA diagrams are selected such that central occupancy is in Ncentral

para=1
Ex=1.0
Ep=1.0
qatom=0


