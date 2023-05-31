/*
  Copyright (C) 2018
      Max Planck Institute for Polymer Research

  This file is part of ESPResSo++.

  ESPResSo++ is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  ESPResSo++ is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// ESPP_CLASS
#ifndef _INTERACTION_VERLETLISTADRESSATINTERACTIONTEMPLATE_HPP
#define _INTERACTION_VERLETLISTADRESSATINTERACTIONTEMPLATE_HPP

//#include <typeinfo>

#include "System.hpp"
#include "bc/BC.hpp"

#include "types.hpp"
#include "Interaction.hpp"
#include "Real3D.hpp"
#include "Tensor.hpp"
#include "Particle.hpp"
#include "VerletListAdress.hpp"
#include "FixedTupleListAdress.hpp"
#include "esutil/Array2D.hpp"

namespace espressopp
{
namespace interaction
{
template <typename _Potential>
class VerletListAdressATInteractionTemplate : public Interaction
{
protected:
    typedef _Potential Potential;

public:
    VerletListAdressATInteractionTemplate(std::shared_ptr<VerletListAdress> _verletList,
                                          std::shared_ptr<FixedTupleListAdress> _fixedtupleList)
        : verletList(_verletList), fixedtupleList(_fixedtupleList)
    {
        potentialArray = esutil::Array2D<Potential, esutil::enlarge>(0, 0, Potential());

        // AdResS stuff
        pidhy2 = M_PI / (verletList->getHy() * 2);
        dex = verletList->getEx();
        dex2 = dex * dex;
        dhy = verletList->getHy();
        dexdhy = dex + dhy;
        dexdhy2 = dexdhy * dexdhy;

        ntypes = 0;
    }

    void setVerletList(std::shared_ptr<VerletListAdress> _verletList) { verletList = _verletList; }

    std::shared_ptr<VerletListAdress> getVerletList() { return verletList; }

    void setFixedTupleList(std::shared_ptr<FixedTupleListAdress> _fixedtupleList)
    {
        fixedtupleList = _fixedtupleList;
    }

    void setPotential(int type1, int type2, const Potential &potential)
    {
        // typeX+1 because i<ntypes
        ntypes = std::max(ntypes, std::max(type1 + 1, type2 + 1));

        potentialArray.at(type1, type2) = potential;
        if (type1 != type2)
        {  // add potential in the other direction
            potentialArray.at(type2, type1) = potential;
        }
    }

    Potential &getPotential(int type1, int type2) { return potentialArray.at(type1, type2); }

    std::shared_ptr<Potential> getPotentialPtr(int type1, int type2)
    {
        return std::make_shared<Potential>(potentialArray.at(type1, type2));
    }

    virtual void addForces();
    virtual real computeEnergy();
    virtual real computeEnergyDeriv();
    virtual real computeEnergyAA();
    virtual real computeEnergyCG();
    virtual real computeEnergyAA(int atomtype);
    virtual real computeEnergyCG(int atomtype);
    virtual void computeVirialX(std::vector<real> &p_xx_total, int bins);
    virtual real computeVirial();
    virtual void computeVirialTensor(Tensor &w);
    virtual void computeVirialTensor(Tensor &w, real z);
    virtual void computeVirialTensor(Tensor *w, int n);
    virtual real getMaxCutoff();
    virtual int bondType() { return Nonbonded; }

protected:
    int ntypes;
    std::shared_ptr<VerletListAdress> verletList;
    std::shared_ptr<FixedTupleListAdress> fixedtupleList;
    esutil::Array2D<Potential, esutil::enlarge> potentialArray;

    // AdResS stuff
    real pidhy2;   // pi / (dhy * 2)
    real dexdhy;   // dex + dhy
    real dexdhy2;  // dexdhy^2
    real dex;
    real dhy;
    real dex2;  // dex^2
    // std::map<Particle*, real> weights;
};

//////////////////////////////////////////////////
// INLINE IMPLEMENTATION
//////////////////////////////////////////////////
template <typename _Potential>
inline void VerletListAdressATInteractionTemplate<_Potential>::addForces()
{
    LOG4ESPP_INFO(theLogger, "add forces computed by the Verlet List");

    // We only compute the atomistic contribution of all forces in this template. Hence, we do not
    // loop over the pairs in the CG region.

    // Compute atomistic forces of Pairs inside AdResS zone
    for (PairList::Iterator it(verletList->getAdrPairs()); it.isValid(); ++it)
    {
        // these are the two VP interacting
        Particle &p1 = *it->first;
        Particle &p2 = *it->second;

        // read weights
        real w1 = p1.lambda();
        real w2 = p2.lambda();
        real w12 = w1 * w2;  // AdResS

        // force between AT particles
        if (w12 != 0)
        {  // calculate AT force if both VP are outside CG region (HY-HY, HY-AT, AT-AT)
            FixedTupleListAdress::iterator it3;
            FixedTupleListAdress::iterator it4;
            it3 = fixedtupleList->find(&p1);
            it4 = fixedtupleList->find(&p2);

            if (it3 != fixedtupleList->end() && it4 != fixedtupleList->end())
            {
                std::vector<Particle *> atList1;
                std::vector<Particle *> atList2;
                atList1 = it3->second;
                atList2 = it4->second;

                for (std::vector<Particle *>::iterator itv = atList1.begin(); itv != atList1.end();
                     ++itv)
                {
                    Particle &p3 = **itv;

                    for (std::vector<Particle *>::iterator itv2 = atList2.begin();
                         itv2 != atList2.end(); ++itv2)
                    {
                        Particle &p4 = **itv2;

                        // AT forces
                        const Potential &potential = getPotential(p3.type(), p4.type());
                        Real3D force(0.0, 0.0, 0.0);
                        if (potential._computeForce(force, p3, p4))
                        {
                            force *= w12;
                            p3.force() += force;
                            p4.force() -= force;
                        }
                    }
                }
            }
            else
            {  // this should not happen
                std::stringstream ss;
                ss << "One of the VP particles not found in tuples: " << p1.id() << "-"
                   << p1.ghost() << ", " << p2.id() << "-" << p2.ghost() << " (" << p1.position()
                   << ") (" << p2.position();
                throw std::runtime_error(ss.str());
            }
        }
    }

    // Loop over CG particles and overwrite AT forces and velocity.
    // This makes the AT particles move along with CG particles.

    // Note (Karsten): This is a different approach compared to H-AdResS. Here, we overwrite
    // intra-atomistic rotations and vibrations in the CG zone. This leads to failures in the
    // kinetic energy. However, in Force-AdResS there is no energy conservation anyway. Here we
    // calculate CG forces/velocities and distribute them to AT particles. In contrast, in H-AdResS,
    // we calculate AT forces from intra-molecular interactions and inter-molecular center-of-mass
    // interactions and just update the positions of the center-of-mass CG particles.
    std::set<Particle *> cgZone = verletList->getCGZone();
    for (std::set<Particle *>::iterator it = cgZone.begin(); it != cgZone.end(); ++it)
    {
        Particle &vp = **it;

        FixedTupleListAdress::iterator it3;
        it3 = fixedtupleList->find(&vp);

        if (it3 != fixedtupleList->end())
        {
            std::vector<Particle *> atList1;
            atList1 = it3->second;

            // Real3D vpfm = vp.force() / vp.getMass();
            for (std::vector<Particle *>::iterator itv = atList1.begin(); itv != atList1.end();
                 ++itv)
            {
                Particle &at = **itv;
                at.velocity() = vp.velocity();  // Overwrite velocity - Note (Karsten): See comment
                                                // above. at.force() += at.mass() * vpfm;
            }
        }
        else
        {  // this should not happen
            std::cout << " VP particle not found in tuples: " << vp.id() << "-" << vp.ghost();
            exit(1);
            return;
        }
    }
}

template <typename _Potential>
inline real VerletListAdressATInteractionTemplate<_Potential>::computeEnergy()
{
    std::set<Particle *> cgZone = verletList->getCGZone();
    for (std::set<Particle *>::iterator it = cgZone.begin(); it != cgZone.end(); ++it)
    {
        Particle &vp = **it;
        vp.lambda() = 0.0;
    }

    std::set<Particle *> adrZone = verletList->getAdrZone();
    for (std::set<Particle *>::iterator it = adrZone.begin(); it != adrZone.end(); ++it)
    {
        Particle &vp = **it;

        FixedTupleListAdress::iterator it3;
        it3 = fixedtupleList->find(&vp);

        if (it3 != fixedtupleList->end())
        {
            std::vector<Particle *> atList;
            atList = it3->second;

            // compute center of mass
            Real3D cmp(0.0, 0.0, 0.0);  // center of mass position
            Real3D cmv(0.0, 0.0, 0.0);  // center of mass velocity
            for (std::vector<Particle *>::iterator it2 = atList.begin(); it2 != atList.end(); ++it2)
            {
                Particle &at = **it2;
                cmp += at.mass() * at.position();
                cmv += at.mass() * at.velocity();
            }
            cmp /= vp.getMass();
            cmv /= vp.getMass();

            // update (overwrite) the position and velocity of the VP
            vp.position() = cmp;
            vp.velocity() = cmv;

            // calculate distance to nearest adress particle or center
            std::vector<Real3D *>::iterator it2 = verletList->getAdrPositions().begin();
            Real3D pa = **it2;  // position of adress particle
            Real3D d1;
            verletList->getSystem()->bc->getMinimumImageVector(d1, vp.position(), pa);
            // set min1sq before loop
            real min1sq;
            if (verletList->getAdrRegionType())
            {  // spherical adress region
                min1sq = d1.sqr();
            }
            else
            {  // slab-type adress region
                min1sq = d1[0] * d1[0];
            }
            ++it2;
            for (; it2 != verletList->getAdrPositions().end(); ++it2)
            {
                pa = **it2;
                verletList->getSystem()->bc->getMinimumImageVector(d1, vp.position(), pa);
                real distsq1;
                if (verletList->getAdrRegionType())
                {  // spherical adress region
                    distsq1 = d1.sqr();
                }
                else
                {  // slab-type adress region
                    distsq1 = d1[0] * d1[0];
                }
                if (distsq1 < min1sq) min1sq = distsq1;
            }

            // calculate weight and write it in the map
            real w;
            if (dex2 > min1sq)
                w = 1;
            else if (dexdhy2 < min1sq)
                w = 0;
            else
            {
                w = cos(pidhy2 * (sqrt(min1sq) - dex));
                w *= w;
                // real argument = sqrt(min1sq) - dex;
                // w
                // = 1.0-(30.0/(pow(dhy, 5.0)))*(1.0/5.0*pow(argument, 5.0)-dhy/2.0*pow(argument, 4.0)+1.0/3.0*pow(argument,
                // 3.0)*dhy*dhy);
            }
            vp.lambda() = w;
        }
        else
        {  // this should not happen
            std::cout << " VP particle " << vp.id() << "-" << vp.ghost() << " not found in tuples ";
            std::cout << " (" << vp.position() << ")\n";
            exit(1);
        }
    }

    LOG4ESPP_INFO(theLogger, "compute energy of the Verlet list pairs");

    real e = 0.0;

    for (PairList::Iterator it(verletList->getAdrPairs()); it.isValid(); ++it)
    {
        Particle &p1 = *it->first;
        Particle &p2 = *it->second;
        real w1 = p1.lambda();
        real w2 = p2.lambda();
        real w12 = w1 * w2;

        FixedTupleListAdress::iterator it3;
        FixedTupleListAdress::iterator it4;
        it3 = fixedtupleList->find(&p1);
        it4 = fixedtupleList->find(&p2);

        if (it3 != fixedtupleList->end() && it4 != fixedtupleList->end())
        {
            std::vector<Particle *> atList1;
            std::vector<Particle *> atList2;
            atList1 = it3->second;
            atList2 = it4->second;

            for (std::vector<Particle *>::iterator itv = atList1.begin(); itv != atList1.end();
                 ++itv)
            {
                Particle &p3 = **itv;
                for (std::vector<Particle *>::iterator itv2 = atList2.begin();
                     itv2 != atList2.end(); ++itv2)
                {
                    Particle &p4 = **itv2;

                    // AT energies
                    const Potential &potential = getPotential(p3.type(), p4.type());
                    e += w12 * potential._computeEnergy(p3, p4);
                }
            }
        }
    }
    real esum;
    boost::mpi::all_reduce(*getVerletList()->getSystem()->comm, e, esum, std::plus<real>());
    return esum;
}

template <typename _Potential>
inline real VerletListAdressATInteractionTemplate<_Potential>::computeEnergyDeriv()
{
    std::cout << "Warning! At the moment computeEnergyDeriv in "
                 "VerletListAdressATInteractionTemplate does not work."
              << std::endl;
    return 0.0;
}

template <typename _Potential>
inline real VerletListAdressATInteractionTemplate<_Potential>::computeEnergyAA()
{
    std::cout << "Warning! At the moment computeEnergyAA in VerletListAdressATInteractionTemplate "
                 "does not work."
              << std::endl;
    return 0.0;
}

template <typename _Potential>
inline real VerletListAdressATInteractionTemplate<_Potential>::computeEnergyAA(int atomtype)
{
    std::cout << "Warning! At the moment computeEnergyAA(int atomtype) in "
                 "VerletListAdressATInteractionTemplate does not work."
              << std::endl;
    return 0.0;
}

template <typename _Potential>
inline real VerletListAdressATInteractionTemplate<_Potential>::computeEnergyCG()
{
    std::cout << "Warning! At the moment computeEnergyCG in VerletListAdressATInteractionTemplate "
                 "does not work."
              << std::endl;
    return 0.0;
}

template <typename _Potential>
inline real VerletListAdressATInteractionTemplate<_Potential>::computeEnergyCG(int atomtype)
{
    std::cout << "Warning! At the moment computeEnergyCG(int atomtype) in "
                 "VerletListAdressATInteractionTemplate does not work."
              << std::endl;
    return 0.0;
}

template <typename _Potential>
inline void VerletListAdressATInteractionTemplate<_Potential>::computeVirialX(
    std::vector<real> &p_xx_total, int bins)
{
    std::cout << "Warning! At the moment computeVirialX in VerletListAdressATInteractionTemplate "
                 "does'n work"
              << std::endl;
}

template <typename _Potential>
inline real VerletListAdressATInteractionTemplate<_Potential>::computeVirial()
{
    LOG4ESPP_INFO(theLogger, "compute the virial for the Verlet List");
    std::cout << "Warning! At the moment computeVirial in VerletListAdressATInteractionTemplate "
                 "does not work."
              << std::endl;
    return 0.0;
}

template <typename _Potential>
inline void VerletListAdressATInteractionTemplate<_Potential>::computeVirialTensor(Tensor &w)
{
    LOG4ESPP_INFO(theLogger, "compute the virial tensor for the Verlet List");
    std::cout << "Warning! At the moment computeVirialTensor in "
                 "VerletListAdressATInteractionTemplate does'n work"
              << std::endl;
}

template <typename _Potential>
inline void VerletListAdressATInteractionTemplate<_Potential>::computeVirialTensor(Tensor &w,
                                                                                   real z)
{
    LOG4ESPP_INFO(theLogger, "compute the virial tensor for the Verlet List");
    std::cout << "Warning! At the moment computeVirialTensor in "
                 "VerletListAdressATInteractionTemplate does'n work"
              << std::endl;
}

template <typename _Potential>
inline void VerletListAdressATInteractionTemplate<_Potential>::computeVirialTensor(Tensor *w, int n)
{
    std::cout << "Warning! At the moment computeVirialTensor in "
                 "VerletListAdressATInteractionTemplate does'n work"
              << std::endl;
}

template <typename _Potential>
inline real VerletListAdressATInteractionTemplate<_Potential>::getMaxCutoff()
{
    real cutoff = 0.0;
    for (int i = 0; i < ntypes; i++)
    {
        for (int j = 0; j < ntypes; j++)
        {
            cutoff = std::max(cutoff, getPotential(i, j).getCutoff());
        }
    }
    return cutoff;
}
}  // namespace interaction
}  // namespace espressopp
#endif
