/*
  Copyright (C) 2021
      Max Planck Institute for Polymer Research & JGU Mainz
  Copyright (C) 2012,2013
      Max Planck Institute for Polymer Research
  Copyright (C) 2008,2009,2010,2011
      Max-Planck-Institute for Polymer Research & Fraunhofer SCAI

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
#ifndef VEC_INTEGRATOR_EXTENSION_HPP
#define VEC_INTEGRATOR_EXTENSION_HPP

#include "vec/include/types.hpp"

#include "log4espp.hpp"
#include "types.hpp"
#include "SystemAccess.hpp"

#include "MDIntegratorVec.hpp"

namespace espressopp
{
namespace vec
{
namespace integrator
{
// abstract base class for extensions
class Extension : public SystemAccess
{
public:
    Extension(std::shared_ptr<System> system);

    virtual ~Extension();

    enum ExtensionType
    {
        all = 0,
        Thermostat = 1,
        Barostat = 2,
        Constraint = 3,
        Adress = 4,
        FreeEnergyCompensation = 5,
        ExtForce = 6,
        ExtAnalysis = 7,
        Reaction = 8
    };

    // type of extension
    ExtensionType type;

    /** Register this class so it can be used from Python. */
    static void registerPython();

    ExtensionType getType() { return type; }
    void setType(ExtensionType k) { type = k; }

protected:
    std::shared_ptr<MDIntegratorVec> integrator;  // this is needed for signal connection

    void setIntegrator(std::shared_ptr<MDIntegratorVec> _integrator);

    // pure virtual functions
    virtual void connect() = 0;
    virtual void disconnect() = 0;

    /** Logger */
    static LOG4ESPP_DECL_LOGGER(theLogger);
};
}  // namespace integrator
}  // namespace vec
}  // namespace espressopp

#endif  // VEC_INTEGRATOR_EXTENSION_HPP
