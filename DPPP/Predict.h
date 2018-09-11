//# Predict.h: DPPP step class to predict visibilities from a source model
//# Copyright (C) 2013
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
//# $Id:
//#
//# @author Tammo Jan Dijkema

#ifndef DPPP_PREDICT_H
#define DPPP_PREDICT_H

// @file
// @brief DPPP step class to predict visibilities from a source model

#include "DPInput.h"
#include "DPBuffer.h"
#include "Patch.h"
#include "SourceDBUtil.h"
#include "ApplyBeam.h"
#include "ModelComponent.h"

#include <StationResponse/Station.h>
#include <StationResponse/Types.h>
#include <casacore/casa/Arrays/Cube.h>
#include <casacore/casa/Quanta/MVEpoch.h>
#include <casacore/measures/Measures/MEpoch.h>
#include <casacore/casa/Arrays/ArrayMath.h>
#include <utility>

namespace DP3 {

  class ParameterSet;

  namespace DPPP {
    // @ingroup NDPPP

    // This class is a DPStep class to predict visibilities with optionally beam

    typedef std::pair<size_t, size_t> Baseline;
    typedef std::pair<ModelComponent::ConstPtr, Patch::ConstPtr> Source;

    class Predict: public DPStep
    {
    public:
      typedef std::shared_ptr<Predict> ShPtr;

      // Construct the object.
      // Parameters are obtained from the parset using the given prefix.
      Predict (DPInput*, const ParameterSet&, const string& prefix);

      // Constructor with explicit sourcelist
      Predict (DPInput*, const ParameterSet&, const string& prefix,
               const vector<string>& sourcePatterns);

      // The actual constructor
      void init (DPInput*, const ParameterSet&, const string& prefix,
                 const vector<string>& sourcePatterns);

      // Set the applycal substep
      void setApplyCal(DPInput*, const ParameterSet&, const string& prefix);

      // Set the operation type
      void setOperation(const std::string& type);

      Predict();

      virtual ~Predict();

      // Process the data.
      // It keeps the data.
      // When processed, it invokes the process function of the next step.
      virtual bool process (const DPBuffer&);

      // Finish the processing of this step and subsequent steps.
      virtual void finish();

      // Update the general info.
      virtual void updateInfo (const DPInfo&);

      // Show the step parameters.
      virtual void show (std::ostream&) const;

      // Show the timings.
      virtual void showTimings (std::ostream&, double duration) const;

      // Prepare the sources
      void setSources(const vector<string>& sourcePatterns);

      // Return the direction of the first patch
      std::pair<double, double> getFirstDirection() const;

    private:
      LOFAR::StationResponse::vector3r_t dir2Itrf (const casacore::MDirection& dir,
                                     casacore::MDirection::Convert& measConverter);
      void addBeamToData (Patch::ConstPtr patch, double time,
                                   const LOFAR::StationResponse::vector3r_t& refdir,
                                   const LOFAR::StationResponse::vector3r_t& tiledir,
                                   uint thread, uint nSamples, dcomplex* data0);

      //# Data members.
      DPInput*         itsInput;
      string           itsName;
      DPBuffer         itsBuffer;
      string           itsSourceDBName;
      string           itsOperation;
      bool             itsApplyBeam;
      bool             itsStokesIOnly;
      bool             itsUseChannelFreq;
      bool             itsOneBeamPerPatch;
      Position         itsPhaseRef;

      bool             itsDoApplyCal;
      ApplyCal         itsApplyCalStep;
      DPBuffer         itsTempBuffer;
      ResultStep*      itsResultStep; // For catching results from ApplyCal

      uint             itsDebugLevel;

      vector<Baseline> itsBaselines;

      // Vector containing info on converting baseline uvw to station uvw
      vector<int>      itsUVWSplitIndex;

      // UVW coordinates per station (3 coordinates per station)
      casacore::Matrix<double>   itsUVW;

      // The info needed to calculate the station beams.
      vector<vector<LOFAR::StationResponse::Station::Ptr> > itsAntBeamInfo;
      vector<casacore::MeasFrame>                    itsMeasFrames;
      vector<casacore::MDirection::Convert>          itsMeasConverters;
      vector<vector<LOFAR::StationResponse::matrix22c_t> >  itsBeamValues;
      ApplyBeam::BeamMode                            itsBeamMode;

      std::string itsDirectionsStr; // Definition of patches, to pass to applycal
      vector<Patch::ConstPtr> itsPatchList;
      vector<Source> itsSourceList;

      vector<casacore::Cube<dcomplex> > itsModelVis; // one for every thread
      vector<casacore::Cube<dcomplex> > itsModelVisPatch;

      NSTimer          itsTimer;
      NSTimer          itsTimerPredict;
    };

  } //# end namespace
}

#endif
