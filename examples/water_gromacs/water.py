#!/usr/bin/env python3
#  Copyright (C) 2016-2017(H)
#      Max Planck Institute for Polymer Research
#
#  This file is part of ESPResSo++.
#
#  ESPResSo++ is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  ESPResSo++ is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

###########################################################################
#                                                                         #
#  ESPResSo++ Python script for tabulated GROMACS simulation              #
#                                                                         #
###########################################################################

import sys
import time
import espressopp
import mpi4py.MPI as MPI
import logging
from espressopp import Real3D, Int3D
from espressopp.tools import gromacs
from espressopp.tools import decomp
from espressopp.tools import timers

# This function assists users to calculate type-specific radial distribution function
def computeRDF(system, rdf_pair, rdf_dr, num_particles, Lx, Ly, Lz, types, atomtypes):
    list_t = []
    M_PI = 3.1415926535897932384626433832795029
    type_end = []
    type_end.append(0)
    for k in range(len(atomtypes)):
        for pid in range(num_particles):
            if types[pid]==k:
                list_t.append(pid+1)
        type_end.append(len(list_t))
    
    rdf_maxr = min(Lx,Ly,Lz)
    rdf_bin = int(0.5*rdf_maxr/rdf_dr)
    
    rdf_array = []
    for k in range(len(rdf_pair)):
        arr_tmp=[0]*rdf_bin
        rdf_array.append(arr_tmp)
        del arr_tmp

    pos = espressopp.analysis.Configurations(system)
    pos.capacity=1
    pos.gather()
    
    for p in range(len(rdf_pair)):
        itype=rdf_pair[p][0]
        jtype=rdf_pair[p][1]
        
        istart=type_end[itype]
        ilast=type_end[itype+1]
        
        for i in range(istart,ilast):
            idx=list_t[i]
            
            jlast=type_end[jtype+1]
            jstart=type_end[jtype]
                
            for j in range(jstart,jlast):
                jdx=list_t[j]
                if idx!=jdx:
                    l=pos[0][jdx]-pos[0][idx]
                    if l[0]<-Lx/2.0: l[0]+=Lx
                    elif l[0]>Lx/2.0: l[0]-=Lx
                    if l[1]<-Ly/2.0: l[1]+=Ly
                    elif l[1]>Ly/2.0: l[1]-=Ly
                    if l[2]<-Lz/2.0: l[2]+=Lz
                    elif l[2]>Lz/2.0: l[2]-=Lz
                    dist=l.abs()
                    bin_index=int(dist/rdf_dr);
                    if bin_index<rdf_bin:
                        rdf_array[p][bin_index]+=1
                        
    for p in range(len(rdf_pair)):
        idx=rdf_pair[p][0]
        jdx=rdf_pair[p][1]
        itot_f=float(type_end[idx+1]-type_end[idx])
        jtot_f=float(type_end[jdx+1]-type_end[jdx])
        rho_j=jtot_f/Lx/Ly/Lz
        
        if jdx==idx:
            rad_tmp=rdf_bin*rdf_dr
            vol_tmp=4.0/3.0*M_PI*pow(rad_tmp,3)
            rho_j*=1.0-1.0/vol_tmp
        
        typeI=str(list(atomtypes.keys())[list(atomtypes.values()).index(idx)])
        typeJ=str(list(atomtypes.keys())[list(atomtypes.values()).index(jdx)])
        filename="rdf."+typeI+"-"+typeJ+".dat"
        out_stream = open(filename, 'w')
        for i in range(rdf_bin):
            rdf_rad=(i+0.5)*rdf_dr
            rdf_fac=4.0*M_PI*rdf_rad*rdf_rad*rdf_dr*itot_f*rho_j
            out_stream.write("%.4f\t%.6f\n" %(rdf_rad, rdf_array[p][i]/rdf_fac))
        out_stream.close()
        print("The calculation to g(r) for %s-%s pairs is finished. See in %s" % (typeI,typeJ,filename))
    
    print("") 

# This example reads in a gromacs water system (SPC/Fw) treated with reaction field.
# See the corresponding gromacs grompp.mdp paramter file.
# Output of gromacs energies and esp energies should be the same

# simulation parameters (nvt = False is nve)
steps = 10000
check = steps/100
rc    = 0.9  # Verlet list cutoff
skin  = 0.03
timestep = 0.0005
# parameters to convert GROMACS tabulated potential file
sigma = 1.0
epsilon = 1.0
c6 = 1.0
c12 = 1.0
# parameters to calculate partial radial distribution functions
rdf_pair = [[0,0],[1,0],[1,1]]  # type-type [(H,H),(O,H),(O,O)]
rdf_dr = 0.002                  # width for each ridial shell

# GROMACS setup files
grofile = "conf.gro"
topfile = "topol.top"


# this calls the gromacs parser for processing the top file (and included files) and the conf file
# The variables at the beginning defaults, types, etc... can be found by calling
# gromacs.read(grofile,topfile) without return values. It then prints out the variables to be unpacked
defaults, types, atomtypes, masses, charges, atomtypeparameters, bondtypes, bondtypeparams, angletypes, angletypeparams, exclusions, x, y, z, vx, vy, vz, resname, resid, Lx, Ly, Lz =gromacs.read(grofile,topfile)

######################################################################
##  IT SHOULD BE UNNECESSARY TO MAKE MODIFICATIONS BELOW THIS LINE  ##
######################################################################
#types, bonds, angles, dihedrals, x, y, z, vx, vy, vz, Lx, Ly, Lz = gromacs.read(grofile,topfile)
#defaults, types, masses, charges, atomtypeparameters, bondtypes, bondtypeparams, angletypes, angletypeparams, exclusions, x, y, z, vx, vy, vz, Lx, Ly, Lz = gromacs.read(grofile,topfile)
num_particles = len(x)

density = num_particles / (Lx * Ly * Lz)
size = (Lx, Ly, Lz)
print(size)

sys.stdout.write('Setting up simulation ...\n')
system = espressopp.System()
system.rng = espressopp.esutil.RNG()
system.bc = espressopp.bc.OrthorhombicBC(system.rng, size)
system.skin = skin

comm = MPI.COMM_WORLD
nodeGrid = decomp.nodeGrid(comm.size,size,rc,skin)
cellGrid = decomp.cellGrid(size, nodeGrid, rc, skin)
system.storage = espressopp.storage.DomainDecomposition(system, nodeGrid, cellGrid)

# setting up GROMACS interaction stuff
# create a force capped Lennard-Jones interaction that uses a verlet list
verletlist  = espressopp.VerletList(system, rc)
interaction = espressopp.interaction.VerletListLennardJonesGromacs(verletlist)

# add particles to the system and then decompose
props = ['id', 'pos', 'v', 'type', 'mass', 'q']
allParticles = []
for pid in range(num_particles):
    part = [pid + 1, Real3D(x[pid], y[pid], z[pid]),
            Real3D(vx[pid], vy[pid], vz[pid]), types[pid], masses[pid], charges[pid]]
    allParticles.append(part)
system.storage.addParticles(allParticles, *props)
system.storage.decompose()

# set up LJ interaction according to the parameters read from the .top file
ljinteraction=gromacs.setLennardJonesInteractions(system, defaults, atomtypeparameters, verletlist,rc)

# set up angle interactions according to the parameters read from the .top file
angleinteractions=gromacs.setAngleInteractions(system, angletypes, angletypeparams)

# set up coulomb interactions according to the parameters read from the .top file
# !! Warning: this only works for reaction-field now!
qq_interactions=gromacs.setCoulombInteractions(system, verletlist, rc, types, epsilon1=1, epsilon2=80, kappa=0)

# set up bonded interactions according to the parameters read from the .top file
bondedinteractions=gromacs.setBondedInteractions(system, bondtypes, bondtypeparams)

# exlusions, i.e. pairs of atoms not considered for the non-bonded part. Those are defined either by bonds which automatically generate an exclusion. Or by the nregxcl variable
verletlist.exclude(exclusions)

# langevin thermostat
langevin = espressopp.integrator.LangevinThermostat(system)
langevin.gamma = 2.0
langevin.temperature = 2.4942 # kT in gromacs units
integrator = espressopp.integrator.VelocityVerlet(system)
integrator.addExtension(langevin)
integrator.dt = timestep

# print simulation parameters
print('')
print('number of particles =', num_particles)
print('density = %.4f' % (density))
print('rc =', rc)
print('dt =', integrator.dt)
print('skin =', system.skin)
print('steps =', steps)
print('NodeGrid = %s' % (nodeGrid,))
print('CellGrid = %s' % (cellGrid,))
print('')


# analysis
configurations = espressopp.analysis.Configurations(system)
configurations.gather()
temperature = espressopp.analysis.Temperature(system)
pressure = espressopp.analysis.Pressure(system)
pressureTensor = espressopp.analysis.PressureTensor(system)

print("i*timestep,Eb, EAng, ELj, EQQ, Ek, Etotal")
fmt='%5.5f %15.8g %15.8g %15.8g %15.8g %15.8g %15.8f\n'
outfile = open("esp.dat", "w")
start_time = time.process_time()

for i in range(int(check)):
    T = temperature.compute()
    P = pressure.compute()
    Eb = 0
    EAng = 0
    for bd in list(bondedinteractions.values()): Eb+=bd.computeEnergy()
    for ang in list(angleinteractions.values()): EAng+=ang.computeEnergy()
    ELj= ljinteraction.computeEnergy()
    EQQ= qq_interactions.computeEnergy()
    T = temperature.compute()
    Ek = 0.5 * T * (3 * num_particles)
    Etotal = Ek+Eb+EAng+EQQ+ELj
    outfile.write(fmt%(i*steps/check*timestep,Eb, EAng, ELj, EQQ, Ek, Etotal))
    print((fmt%(i*steps/check*timestep,Eb, EAng, ELj, EQQ, Ek, Etotal)), end='')

    #espressopp.tools.pdb.pdbwrite("traj.pdb", system, append=True)
    integrator.run(int(steps/check)) # print out every steps/check steps
    #system.storage.decompose()

# print timings and neighbor list information
end_time = time.process_time()
timers.show(integrator.getTimers(), precision=2)

# compute partial RDF for selected type-type pairs
print("Now compute radial distribution functions for SPC/fw water in the last frame")
computeRDF(system, rdf_pair, rdf_dr, num_particles, Lx, Ly, Lz, types, atomtypes)

sys.stdout.write('Integration steps = %d\n' % integrator.step)
sys.stdout.write('CPU time = %.1f\n' % (end_time - start_time))
