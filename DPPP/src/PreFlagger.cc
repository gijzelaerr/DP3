//# PreFlagger.cc: DPPP step class to flag data based on median filtering
//# Copyright (C) 2010
//# ASTRON (Netherlands Institute for Radio Astronomy)
//# P.O.Box 2, 7990 AA Dwingeloo, The Netherlands
//#
//# This file is part of the LOFAR software suite.
//# The LOFAR software suite is free software: you can redistribute it and/or
//# modify it under the terms of the GNU General Public License as published
//# by the Free Software Foundation, either version 3 of the License, or
//# (at your option) any later version.
//#
//# The LOFAR software suite is distributed in the hope that it will be useful,
//# but WITHOUT ANY WARRANTY; without even the implied warranty of
//# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//# GNU General Public License for more details.
//#
//# You should have received a copy of the GNU General Public License along
//# with the LOFAR software suite. If not, see <http://www.gnu.org/licenses/>.
//#
//# $Id$
//#
//# @author Ger van Diepen

#include <lofar_config.h>
#include <DPPP/PreFlagger.h>
#include <DPPP/DPBuffer.h>
#include <DPPP/AverageInfo.h>
#include <Common/ParameterSet.h>
#include <Common/StreamUtil.h>
#include <Common/LofarLogger.h>
#include <casa/Arrays/ArrayMath.h>
#include <iostream>
#include <algorithm>

using namespace casa;

namespace LOFAR {
  namespace DPPP {

    PreFlagger::PreFlagger (const ParameterSet& parset, const string& prefix,
                            const Vector<String>& antNames)
      : itsName (prefix)
    {
      itsMinUV    = parset.getDouble (prefix+"uvmin", -1);
      itsMaxUV    = parset.getDouble (prefix+"uvmax", -1);
      itsChannels = parset.getUintVector (prefix+"channels", vector<uint>());
      itsFlagAnt1 = parset.getStringVector (prefix+"antenna1",
                                            vector<string>());
      itsFlagAnt2 = parset.getStringVector (prefix+"antenna2",
                                            vector<string>());
      itsFlagAnt  = parset.getStringVector (prefix+"antenna",
                                            vector<string>());
      fillBLMatrix (antNames);
      // Determine if the flag on UV distance.
      // If so, square the distances to avoid having to take the sqrt in flagUV.
      itsFlagOnUV = itsMinUV > 0;
      itsMinUV   *= itsMinUV;
      if (itsMaxUV > 0) {
        itsFlagOnUV = true;
        itsMaxUV   *= itsMaxUV;
      } else {
        // Make it a very high number.
        itsMaxUV = 1e30;
      }
    }

    PreFlagger::~PreFlagger()
    {}

    void PreFlagger::show (std::ostream& os) const
    {
      os << "PreFlagger " << itsName << std::endl;
    }

    void PreFlagger::updateAverageInfo (AverageInfo& info)
    {
      // Check for channels exceeding nr of channels.
      for (uint i=0; i<itsChannels.size(); ++i) {
        ASSERTSTR (itsChannels[i] < info.nchan(),
                   "PreFlagger: given channel to be flagged exceeds "
                   "nr of channels (=" << info.nchan() << ')');
      }
    }

    bool PreFlagger::process (const DPBuffer& buf)
    {
      DPBuffer out(buf);
      // The flags will be changed, so make sure we have a unique array.
      out.getFlags().unique();
      // Flag on UV distance if necessary.
      if (itsFlagOnUV) {
        flagUV (itsInput->fetchUVW(buf, buf.getRowNrs()), out.getFlags());
      }
      // Flag on baseline if necessary.
      if (itsFlagOnBL) {
        flagBL (itsInput->getAnt1(), itsInput->getAnt2(), out.getFlags());
      }
      // Flag on channel if necessary.
      if (! itsChannels.empty()) {
        flagChannels (out.getFlags());
      }
      // Let the next step do its processing.
      getNextStep()->process (out);
      return true;
    }

    void PreFlagger::finish()
    {
      // Let the next step finish its processing.
      getNextStep()->finish();
    }

    void PreFlagger::flagUV (const Matrix<double>& uvw,
                             Cube<bool>& flags)
    {
      const IPosition& shape = flags.shape();
      uint nr = shape[0] * shape[1];
      uint nrbl = shape[2];
      const double* uvwPtr = uvw.data();
      bool* flagPtr = flags.data();
      for (uint i=0; i<nrbl; ++i) {
        // UV-distance is sqrt(u^2 + v^2).
        // The sqrt is not needed because minuv and maxuv are squared.
        double uvdist = uvwPtr[0] * uvwPtr[0] + uvwPtr[1] * uvwPtr[1];
        if (uvdist < itsMinUV  ||  uvdist > itsMaxUV) {
          // UV-dist mismatches, so flag entire baseline.
          std::fill (flagPtr, flagPtr+nr, true);
        }
        uvwPtr  += 3;
        flagPtr += nr;
      }
    }

    void PreFlagger::flagBL (const Vector<int>& ant1,
                             const Vector<int>& ant2,
                             Cube<bool>& flags)
    {
      const IPosition& shape = flags.shape();
      uint nr = shape[0] * shape[1];
      uint nrbl = shape[2];
      const int* ant1Ptr = ant1.data();
      const int* ant2Ptr = ant2.data();
      bool* flagPtr = flags.data();
      for (uint i=0; i<nrbl; ++i) {
        if (itsFlagBL(*ant1Ptr, *ant2Ptr)) {
          // Flag this baseline.
          std::fill (flagPtr, flagPtr+nr, true);
        }
        ant1Ptr++;
        ant2Ptr++;
        flagPtr += nr;
      }
    }

    void PreFlagger::flagChannels (Cube<bool>& flags)
    {
      const IPosition& shape = flags.shape();
      uint nrcorr = shape[0];
      uint nr     = nrcorr * shape[1];
      uint nrbl   = shape[2];
      bool* flagPtr = flags.data();
      for (uint i=0; i<nrbl; ++i) {
        for (vector<uint>::const_iterator iter = itsChannels.begin();
             iter != itsChannels.end(); ++iter) {
          // Flag this channel.
          bool* ptr = flagPtr + *iter * nrcorr;
          std::fill (ptr, ptr+nrcorr, true);
        }
        flagPtr += nr;
      }
    }

    void PreFlagger::fillBLMatrix (const Vector<String>& antNames)
    {
      // Initialize the matrix.
      itsFlagBL.resize (antNames.size(), antNames.size());
      itsFlagBL = false;
      ASSERTSTR (itsFlagAnt1.size() == itsFlagAnt2.size(),
                 "PreFlagger parameters antenna1/2 must have equal length");
      itsFlagOnBL = itsFlagAnt1.size() > 0  ||  itsFlagAnt.size() > 0;
      // Set matrix flags for matching baselines.
      for (uint i=0; i<itsFlagAnt1.size(); ++i) {
        // Turn the given antenna name pattern into a regex.
        Regex regex1(Regex::fromPattern (itsFlagAnt1[i]));
        Regex regex2(Regex::fromPattern (itsFlagAnt2[i]));
        // Loop through all antenna names and set matrix for matching ones.
        for (uint i2=0; i2<antNames.size(); ++i2) {
          if (antNames[i2].matches (regex2)) {
            // Antenna2 matches, now try Antenna1.
            for (uint i1=0; i1<antNames.size(); ++i1) {
              if (antNames[i1].matches (regex1)) {
                itsFlagBL(i1,i2) = true;
              }
            }
          }
        }
      }
      // Set matrix flags for matching antennae.
      for (uint i=0; i<itsFlagAnt.size(); ++i) {
        // Turn the given antenna name pattern into a regex.
        Regex regex(Regex::fromPattern (itsFlagAnt[i]));
        // Loop through all antenna names and set matrix for matching ones.
        for (uint i2=0; i2<antNames.size(); ++i2) {
          if (antNames[i2].matches (regex)) {
            // Antenna matches, so set all corresponding flags.
            for (uint j=0; j<antNames.size(); ++j) {
              itsFlagBL(i2,j) = true;
              itsFlagBL(j,i2) = true;
            }
          }
        }
      }
    }

  } //# end namespace
}
