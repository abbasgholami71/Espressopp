/*
  Copyright (C) 2021
      Max Planck Institute for Polymer Research & JGU Mainz

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

#include "vec/storage/StorageVec.hpp"
#include "vec/storage/DomainDecomposition.hpp"

#include "python.hpp"
#include "bindings.hpp"

namespace espressopp
{
namespace vec
{
namespace storage
{
void registerPython()
{
    StorageVec::registerPython();
    DomainDecomposition::registerPython();
}
}  // namespace storage
}  // namespace vec
}  // namespace espressopp
