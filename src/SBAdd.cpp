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

//#define DEBUGLOGGING

#include "SBAdd.h"
#include "SBAddImpl.h"

#ifdef DEBUGLOGGING
#include <fstream>
//std::ostream* dbgout = new std::ofstream("debug.out");
//int verbose_level = 2;
#endif

namespace galsim {

    SBAdd::SBAdd(const std::list<SBProfile>& slist, const GSParamsPtr& gsparams) :
        SBProfile(new SBAddImpl(slist,gsparams)) {}

    SBAdd::SBAdd(const SBAdd& rhs) : SBProfile(rhs) {}

    SBAdd::~SBAdd() {}

    std::list<SBProfile> SBAdd::getObjs() const
    {
        assert(dynamic_cast<const SBAddImpl*>(_pimpl.get()));
        return static_cast<const SBAddImpl&>(*_pimpl).getObjs();
    }

    std::string SBAdd::SBAddImpl::serialize() const
    {
        std::ostringstream oss(" ");
        oss << "galsim._galsim.SBAdd([";
        ConstIter sptr = _plist.begin();
        oss << sptr->serialize();
        for (++sptr; sptr!=_plist.end(); ++sptr) oss << ", " << sptr->serialize();
        oss << "], galsim.GSParams("<<*gsparams<<"))";
        return oss.str();
    }

    SBAdd::SBAddImpl::SBAddImpl(const std::list<SBProfile>& slist,
                                const GSParamsPtr& gsparams) :
        SBProfileImpl(gsparams ? gsparams : GetImpl(slist.front())->gsparams)
    {
        for (ConstIter sptr = slist.begin(); sptr!=slist.end(); ++sptr)
            add(*sptr);
        initialize();
    }

    void SBAdd::SBAddImpl::add(const SBProfile& rhs)
    {
        dbg<<"Start SBAdd::add.  Adding item # "<<_plist.size()+1<<std::endl;
        // Add new summand(s) to the _plist:
        assert(GetImpl(rhs));
        const SBAddImpl *sba = dynamic_cast<const SBAddImpl*>(GetImpl(rhs));
        if (sba) {
            // If rhs is an SBAdd, copy its full list here
            _plist.insert(_plist.end(),sba->_plist.begin(),sba->_plist.end());
        } else {
            _plist.push_back(rhs);
        }
    }

    void SBAdd::SBAddImpl::initialize()
    {
        _sumflux = _sumfx = _sumfy = 0.;
        _maxMaxK = _minStepK = 0.;
        _allAxisymmetric = _allAnalyticX = _allAnalyticK = true;
        _anyHardEdges = false;

        // Accumulate properties of all summands
        for(ConstIter it=_plist.begin(); it!=_plist.end(); ++it) {
            dbg<<"SBAdd component has maxK, stepK = "<<
                it->maxK()<<" , "<<it->stepK()<<std::endl;
            _sumflux += it->getFlux();
            _sumfx += it->getFlux() * it->centroid().x;
            _sumfy += it->getFlux() * it->centroid().y;
            if ( it->maxK() > _maxMaxK)
                _maxMaxK = it->maxK();
            if ( _minStepK<=0. || (it->stepK() < _minStepK) )
                _minStepK = it->stepK();
            _allAxisymmetric = _allAxisymmetric && it->isAxisymmetric();
            _anyHardEdges = _anyHardEdges || it->hasHardEdges();
            _allAnalyticX = _allAnalyticX && it->isAnalyticX();
            _allAnalyticK = _allAnalyticK && it->isAnalyticK();
        }
        dbg<<"Net maxK, stepK = "<<_maxMaxK<<" , "<<_minStepK<<std::endl;
    }

    double SBAdd::SBAddImpl::xValue(const Position<double>& p) const
    {
        ConstIter pptr = _plist.begin();
        assert(pptr != _plist.end());
        double xv = pptr->xValue(p);
        for (++pptr; pptr != _plist.end(); ++pptr)
            xv += pptr->xValue(p);
        return xv;
    }

    std::complex<double> SBAdd::SBAddImpl::kValue(const Position<double>& k) const
    {
        ConstIter pptr = _plist.begin();
        assert(pptr != _plist.end());
        std::complex<double> kv = pptr->kValue(k);
        for (++pptr; pptr != _plist.end(); ++pptr)
            kv += pptr->kValue(k);
        return kv;
    }

    void SBAdd::SBAddImpl::fillXValue(tmv::MatrixView<double> val,
                                      double x0, double dx, int izero,
                                      double y0, double dy, int jzero) const
    {
        dbg<<"SBAdd fillXValue\n";
        dbg<<"x = "<<x0<<" + i * "<<dx<<", izero = "<<izero<<std::endl;
        dbg<<"y = "<<y0<<" + j * "<<dy<<", jzero = "<<jzero<<std::endl;
        ConstIter pptr = _plist.begin();
        assert(pptr != _plist.end());
        GetImpl(*pptr)->fillXValue(val,x0,dx,izero,y0,dy,jzero);
        if (++pptr != _plist.end()) {
            tmv::Matrix<double> val2(val.colsize(),val.rowsize());
            for (; pptr != _plist.end(); ++pptr) {
                GetImpl(*pptr)->fillXValue(val2.view(),x0,dx,izero,y0,dy,jzero);
                val += val2;
            }
        }
    }

    void SBAdd::SBAddImpl::fillKValue(tmv::MatrixView<std::complex<double> > val,
                                      double kx0, double dkx, int izero,
                                      double ky0, double dky, int jzero) const
    {
        dbg<<"SBAdd fillKValue\n";
        dbg<<"kx = "<<kx0<<" + i * "<<dkx<<", izero = "<<izero<<std::endl;
        dbg<<"ky = "<<ky0<<" + j * "<<dky<<", jzero = "<<jzero<<std::endl;
        ConstIter pptr = _plist.begin();
        assert(pptr != _plist.end());
        GetImpl(*pptr)->fillKValue(val,kx0,dkx,izero,ky0,dky,jzero);
        if (++pptr != _plist.end()) {
            tmv::Matrix<std::complex<double> > val2(val.colsize(),val.rowsize());
            for (; pptr != _plist.end(); ++pptr) {
                GetImpl(*pptr)->fillKValue(val2.view(),kx0,dkx,izero,ky0,dky,jzero);
                val += val2;
            }
        }
    }

    void SBAdd::SBAddImpl::fillXValue(tmv::MatrixView<double> val,
                                      double x0, double dx, double dxy,
                                      double y0, double dy, double dyx) const
    {
        dbg<<"SBAdd fillXValue\n";
        dbg<<"x = "<<x0<<" + i * "<<dx<<" + j * "<<dxy<<std::endl;
        dbg<<"y = "<<y0<<" + i * "<<dyx<<" + j * "<<dy<<std::endl;
        ConstIter pptr = _plist.begin();
        assert(pptr != _plist.end());
        GetImpl(*pptr)->fillXValue(val,x0,dx,dxy,y0,dy,dyx);
        if (++pptr != _plist.end()) {
            tmv::Matrix<double> val2(val.colsize(),val.rowsize());
            for (; pptr != _plist.end(); ++pptr) {
                GetImpl(*pptr)->fillXValue(val2.view(),x0,dx,dxy,y0,dy,dyx);
                val += val2;
            }
        }
    }

    void SBAdd::SBAddImpl::fillKValue(tmv::MatrixView<std::complex<double> > val,
                                      double kx0, double dkx, double dkxy,
                                      double ky0, double dky, double dkyx) const
    {
        dbg<<"SBAdd fillKValue\n";
        dbg<<"kx = "<<kx0<<" + i * "<<dkx<<" + j * "<<dkxy<<std::endl;
        dbg<<"ky = "<<ky0<<" + i * "<<dkyx<<" + j * "<<dky<<std::endl;
        ConstIter pptr = _plist.begin();
        assert(pptr != _plist.end());
        GetImpl(*pptr)->fillKValue(val,kx0,dkx,dkxy,ky0,dky,dkyx);
        if (++pptr != _plist.end()) {
            tmv::Matrix<std::complex<double> > val2(val.colsize(),val.rowsize());
            for (; pptr != _plist.end(); ++pptr) {
                GetImpl(*pptr)->fillKValue(val2.view(),kx0,dkx,dkxy,ky0,dky,dkyx);
                val += val2;
            }
        }
    }

    double SBAdd::SBAddImpl::getPositiveFlux() const
    {
        double result = 0.;
        for (ConstIter pptr = _plist.begin(); pptr != _plist.end(); ++pptr) {
            result += pptr->getPositiveFlux();
        }
        return result;
    }

    double SBAdd::SBAddImpl::getNegativeFlux() const
    {
        double result = 0.;
        for (ConstIter pptr = _plist.begin(); pptr != _plist.end(); ++pptr) {
            result += pptr->getNegativeFlux();
        }
        return result;
    }

    boost::shared_ptr<PhotonArray> SBAdd::SBAddImpl::shoot(int N, UniformDeviate u) const
    {
        dbg<<"Add shoot: N = "<<N<<std::endl;
        dbg<<"Target flux = "<<getFlux()<<std::endl;
        double totalAbsoluteFlux = getPositiveFlux() + getNegativeFlux();
        double fluxPerPhoton = totalAbsoluteFlux / N;

        // Initialize the output array
        boost::shared_ptr<PhotonArray> result(new PhotonArray(0));

        double remainingAbsoluteFlux = totalAbsoluteFlux;
        int remainingN = N;

        // Get photons from each summand, using BinomialDeviate to
        // randomize distribution of photons among summands
        for (ConstIter pptr = _plist.begin(); pptr!= _plist.end(); ++pptr) {
            double thisAbsoluteFlux = pptr->getPositiveFlux() + pptr->getNegativeFlux();

            // How many photons to shoot from this summand?
            int thisN = remainingN;  // All of what's left, if this is the last summand...
            std::list<SBProfile>::const_iterator nextPtr = pptr;
            ++nextPtr;
            if (nextPtr!=_plist.end()) {
                // otherwise allocate a randomized fraction of the remaining photons to this summand:
                BinomialDeviate bd(u, remainingN, thisAbsoluteFlux/remainingAbsoluteFlux);
                thisN = bd();
            }
            if (thisN > 0) {
                boost::shared_ptr<PhotonArray> thisPA = pptr->shoot(thisN, u);
                // Now rescale the photon fluxes so that they are each nominally fluxPerPhoton
                // whereas the shoot() routine would have made them each nominally
                // thisAbsoluteFlux/thisN
                thisPA->scaleFlux(fluxPerPhoton*thisN/thisAbsoluteFlux);
                result->append(*thisPA);
            }
            remainingN -= thisN;
            remainingAbsoluteFlux -= thisAbsoluteFlux;
            if (remainingN <=0) break;
            if (remainingAbsoluteFlux <= 0.) break;
        }

        dbg<<"Add Realized flux = "<<result->getTotalFlux()<<std::endl;

        // This process produces correlated photons, so mark the resulting array as such.
        if (_plist.size() > 1) result->setCorrelated();

        return result;
    }

}
