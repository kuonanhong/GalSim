/* -*- c++ -*-
 * Copyright (c) 2012-2016 by the GalSim developers team on GitHub
 * https://github.com/GalSim-developers
 *
 * This file is part of GalSim: The modular galaxy image simulation toolkit.
 * https://github.com/GalSim-developers/GalSim
 *
 * GalSim is free software: redistribution and use in source and binary forms,
 * with or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions, and the disclaimer given in the accompanying LICENSE
 *    file.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the disclaimer given in the documentation
 *    and/or other materials provided with the distribution.
 */

#ifndef GalSim_SBDeltaFunctionImpl_H
#define GalSim_SBDeltaFunctionImpl_H

#include "SBProfileImpl.h"
#include "SBDeltaFunction.h"

namespace galsim {

    class SBDeltaFunction::SBDeltaFunctionImpl : public SBProfileImpl
    {
    public:
        SBDeltaFunctionImpl(double flux, const GSParamsPtr& gsparams);

        ~SBDeltaFunctionImpl() {}

        double xValue(const Position<double>& p) const;
        std::complex<double> kValue(const Position<double>& k) const;

        bool isAxisymmetric() const { return true; }
        bool hasHardEdges() const { return true; }
        bool isAnalyticX() const { return true; }
        bool isAnalyticK() const { return true; }

        double maxK() const;
        double stepK() const;

        Position<double> centroid() const { return Position<double>(0., 0.); }

        double getFlux() const { return _flux; }
        double maxSB() const { return _flux; }
        
        /**
         * @brief Shoot photons through this SBDeltaFunction.
         *
         * SBDeltaFunction shoots photons by analytic transformation of the unit disk.  Slightly
         * more than 2 uniform deviates are drawn per photon, with some analytic function calls
         * (sqrt,etc.)
         *
         * @param[in] N Total number of photons to produce.
         * @param[in] ud UniformDeviate that will be used to draw photons from distribution.
         * @returns PhotonArray containing all the photons' info.
         */
        boost::shared_ptr<PhotonArray> shoot(int N, UniformDeviate ud) const;

        void getXRange(double& xmin, double& xmax, std::vector<double>& splits) const;

        void getYRange(double& ymin, double& ymax, std::vector<double>& splits) const;
        
        std::string serialize() const;

    private:
        double _flux; ///< Flux of the Surface Brightness Profile.

        // Copy constructor and op= are undefined.
        SBDeltaFunctionImpl(const SBDeltaFunctionImpl& rhs);
        void operator=(const SBDeltaFunctionImpl& rhs);
    };
}

#endif