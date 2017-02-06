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

#include "SBSpergel.h"
#include "SBSpergelImpl.h"
#include "Solve.h"
#include "math/Bessel.h"

namespace galsim {

    SBSpergel::SBSpergel(double nu, double size, RadiusType rType, double flux,
                         const GSParamsPtr& gsparams) :
        SBProfile(new SBSpergelImpl(nu, size, rType, flux, gsparams)) {}

    SBSpergel::SBSpergel(const SBSpergel& rhs) : SBProfile(rhs) {}

    SBSpergel::~SBSpergel() {}

    double SBSpergel::getNu() const
    {
        assert(dynamic_cast<const SBSpergelImpl*>(_pimpl.get()));
        return static_cast<const SBSpergelImpl&>(*_pimpl).getNu();
    }

    double SBSpergel::getScaleRadius() const
    {
        assert(dynamic_cast<const SBSpergelImpl*>(_pimpl.get()));
        return static_cast<const SBSpergelImpl&>(*_pimpl).getScaleRadius();
    }

    double SBSpergel::getHalfLightRadius() const
    {
        assert(dynamic_cast<const SBSpergelImpl*>(_pimpl.get()));
        return static_cast<const SBSpergelImpl&>(*_pimpl).getHalfLightRadius();
    }

    std::string SBSpergel::SBSpergelImpl::serialize() const
    {
        std::ostringstream oss(" ");
        oss.precision(std::numeric_limits<double>::digits10 + 4);
        oss << "galsim._galsim.SBSpergel("<<getNu()<<", "<<getScaleRadius();
        oss << ", None, "<<getFlux();
        oss << ", galsim.GSParams("<<*gsparams<<"))";
        return oss.str();
    }

    double SBSpergel::calculateIntegratedFlux(const double& r) const
    {
        assert(dynamic_cast<const SBSpergelImpl*>(_pimpl.get()));
        return static_cast<const SBSpergelImpl&>(*_pimpl).calculateIntegratedFlux(r);
    }

    double SBSpergel::calculateFluxRadius(const double& f) const
    {
        assert(dynamic_cast<const SBSpergelImpl*>(_pimpl.get()));
        return static_cast<const SBSpergelImpl&>(*_pimpl).calculateFluxRadius(f);
    }

    LRUCache<Tuple<double,GSParamsPtr>,SpergelInfo> SBSpergel::SBSpergelImpl::cache(
        sbp::max_spergel_cache);

    SBSpergel::SBSpergelImpl::SBSpergelImpl(double nu, double size, RadiusType rType,
                                            double flux, const GSParamsPtr& gsparams) :
        SBProfileImpl(gsparams),
        _nu(nu), _flux(flux), _info(cache.get(MakeTuple(_nu, this->gsparams.duplicate())))
    {
        dbg<<"Start SBSpergel constructor:\n";
        dbg<<"nu = "<<_nu<<std::endl;
        dbg<<"size = "<<size<<"  rType = "<<rType<<std::endl;
        dbg<<"flux = "<<_flux<<std::endl;

        // Set size of this instance according to type of size given in constructor
        switch(rType) {
          case HALF_LIGHT_RADIUS:
              {
                  _re = size;
                  _r0 = _re / _info->getHLR();
              }
              break;
          case SCALE_RADIUS:
              {
                  _r0 = size;
                  _re = _r0 * _info->getHLR();
              }
              break;
        }

        _r0_sq = _r0 * _r0;
        _inv_r0 = 1. / _r0;
        _shootnorm = _flux * _info->getXNorm();
        _xnorm = _shootnorm / _r0_sq;

        dbg<<"scale radius = "<<_r0<<std::endl;
        dbg<<"HLR = "<<_re<<std::endl;
    }

    double SBSpergel::SBSpergelImpl::maxK() const { return _info->maxK() * _inv_r0; }
    double SBSpergel::SBSpergelImpl::stepK() const { return _info->stepK() * _inv_r0; }

    double SBSpergel::SBSpergelImpl::calculateIntegratedFlux(const double& r) const
    { return _info->calculateIntegratedFlux(r*_inv_r0);}
    double SBSpergel::SBSpergelImpl::calculateFluxRadius(const double& f) const
    { return _info->calculateFluxRadius(f) * _r0; }

    // Equations (3, 4) of Spergel (2010)
    double SBSpergel::SBSpergelImpl::xValue(const Position<double>& p) const
    {
        double r = sqrt(p.x * p.x + p.y * p.y) * _inv_r0;
        return _xnorm * _info->xValue(r);
    }

    // Equation (2) of Spergel (2010)
    std::complex<double> SBSpergel::SBSpergelImpl::kValue(const Position<double>& k) const
    {
        double ksq = (k.x*k.x + k.y*k.y) * _r0_sq;
        return _flux * _info->kValue(ksq);
    }

    void SBSpergel::SBSpergelImpl::fillXImage(ImageView<double> im,
                                              double x0, double dx, int izero,
                                              double y0, double dy, int jzero) const
    {
        dbg<<"SBSpergel fillXImage\n";
        dbg<<"x = "<<x0<<" + i * "<<dx<<", izero = "<<izero<<std::endl;
        dbg<<"y = "<<y0<<" + j * "<<dy<<", jzero = "<<jzero<<std::endl;
        if (izero != 0 || jzero != 0) {
            xdbg<<"Use Quadrant\n";
            fillXImageQuadrant(im,x0,dx,izero,y0,dy,jzero);
        } else {
            xdbg<<"Non-Quadrant\n";
            const int m = im.getNCol();
            const int n = im.getNRow();
            double* ptr = im.getData();
            const int skip = im.getNSkip();
            assert(im.getStep() == 1);

            x0 *= _inv_r0;
            dx *= _inv_r0;
            y0 *= _inv_r0;
            dy *= _inv_r0;

            for (int j=0; j<n; ++j,y0+=dy,ptr+=skip) {
                double x = x0;
                double ysq = y0*y0;
                for (int i=0; i<m; ++i,x+=dx)
                    *ptr++ = _xnorm * _info->xValue(sqrt(x*x + ysq));
            }
        }
    }

    void SBSpergel::SBSpergelImpl::fillXImage(ImageView<double> im,
                                              double x0, double dx, double dxy,
                                              double y0, double dy, double dyx) const
    {
        dbg<<"SBSpergel fillXImage\n";
        dbg<<"x = "<<x0<<" + i * "<<dx<<" + j * "<<dxy<<std::endl;
        dbg<<"y = "<<y0<<" + i * "<<dyx<<" + j * "<<dy<<std::endl;
        const int m = im.getNCol();
        const int n = im.getNRow();
        double* ptr = im.getData();
        const int skip = im.getNSkip();
        assert(im.getStep() == 1);

        x0 *= _inv_r0;
        dx *= _inv_r0;
        dxy *= _inv_r0;
        y0 *= _inv_r0;
        dy *= _inv_r0;
        dyx *= _inv_r0;

        for (int j=0; j<n; ++j,x0+=dxy,y0+=dy,ptr+=skip) {
            double x = x0;
            double y = y0;
            for (int i=0; i<m; ++i,x+=dx,y+=dyx)
                *ptr++ = _xnorm * _info->xValue(sqrt(x*x + y*y));
        }
    }

    void SBSpergel::SBSpergelImpl::fillKImage(ImageView<std::complex<double> > im,
                                              double kx0, double dkx, int izero,
                                              double ky0, double dky, int jzero) const
    {
        dbg<<"SBSpergel fillKImage\n";
        dbg<<"kx = "<<kx0<<" + i * "<<dkx<<", izero = "<<izero<<std::endl;
        dbg<<"ky = "<<ky0<<" + j * "<<dky<<", jzero = "<<jzero<<std::endl;
        if (izero != 0 || jzero != 0) {
            xdbg<<"Use Quadrant\n";
            fillKImageQuadrant(im,kx0,dkx,izero,ky0,dky,jzero);
        } else {
            xdbg<<"Non-Quadrant\n";
            const int m = im.getNCol();
            const int n = im.getNRow();
            std::complex<double>* ptr = im.getData();
            int skip = im.getNSkip();
            assert(im.getStep() == 1);

            kx0 *= _r0;
            dkx *= _r0;
            ky0 *= _r0;
            dky *= _r0;

            for (int j=0; j<n; ++j,ky0+=dky,ptr+=skip) {
                double kx = kx0;
                double kysq = ky0*ky0;
                for (int i=0;i<m;++i,kx+=dkx)
                    *ptr++ = _flux * _info->kValue(kx*kx + kysq);
            }
        }
    }

    void SBSpergel::SBSpergelImpl::fillKImage(ImageView<std::complex<double> > im,
                                              double kx0, double dkx, double dkxy,
                                              double ky0, double dky, double dkyx) const
    {
        dbg<<"SBSpergel fillKImage\n";
        dbg<<"kx = "<<kx0<<" + i * "<<dkx<<" + j * "<<dkxy<<std::endl;
        dbg<<"ky = "<<ky0<<" + i * "<<dkyx<<" + j * "<<dky<<std::endl;
        const int m = im.getNCol();
        const int n = im.getNRow();
        std::complex<double>* ptr = im.getData();
        int skip = im.getNSkip();
        assert(im.getStep() == 1);

        kx0 *= _r0;
        dkx *= _r0;
        dkxy *= _r0;
        ky0 *= _r0;
        dky *= _r0;
        dkyx *= _r0;

        for (int j=0; j<n; ++j,kx0+=dkxy,ky0+=dky) {
            double kx = kx0;
            double ky = ky0;
            for (int i=0; i<m; ++i,kx+=dkx,ky+=dkyx)
                *ptr++ = _flux * _info->kValue(kx*kx + ky*ky);
        }
    }

    SpergelInfo::SpergelInfo(double nu, const GSParamsPtr& gsparams) :
        _nu(nu), _gsparams(gsparams),
        _gamma_nup1(std::tgamma(_nu+1.0)),
        _gamma_nup2(_gamma_nup1 * (_nu+1)),
        _xnorm0((_nu > 0.) ? _gamma_nup1 / (2. * _nu) * std::pow(2., _nu) : INFINITY),
        _maxk(0.), _stepk(0.), _re(0.)
    {
        dbg<<"Start SpergelInfo constructor for nu = "<<_nu<<std::endl;

        if (_nu < sbp::minimum_spergel_nu || _nu > sbp::maximum_spergel_nu)
            throw SBError("Requested Spergel index out of range");
    }

    class SpergelIntegratedFlux
    {
    public:
        SpergelIntegratedFlux(double nu, double gamma_nup2, double flux_frac=0.0)
            : _nu(nu), _gamma_nup2(gamma_nup2),  _target(flux_frac) {}

        double operator()(double u) const
        {
            // Return flux integrated up to radius `u` in units of r0, minus `flux_frac`
            // (i.e., make a residual so this can be used to search for a target flux.
            // This result is derived in Spergel (2010) eqn. 8 by going to Fourier space
            // and integrating by parts.
            // The key Bessel identities:
            // int(r J0(k r), r=0..R) = R J1(k R) / k
            // d[-J0(k R)]/dk = R J1(k R)
            // The definition of the radial surface brightness profile and Fourier transform:
            // Sigma_nu(r) = (r/2)^nu K_nu(r)/Gamma(nu+1)
            //             = int(k J0(k r) / (1+k^2)^(1+nu), k=0..inf)
            // and the main result:
            // F(R) = int(2 pi r Sigma(r), r=0..R)
            //      = int(r int(k J0(k r) / (1+k^2)^(1+nu), k=0..inf), r=0..R) // Do the r-integral
            //      = int(R J1(k R)/(1+k^2)^(1+nu), k=0..inf)
            // Now integrate by parts with
            //      u = 1/(1+k^2)^(1+nu)                 dv = R J1(k R) dk
            // =>  du = -2 k (1+nu)/(1+k^2)^(2+nu) dk     v = -J0(k R)
            // => F(R) = u v | k=0,inf - int(v du, k=0..inf)
            //         = (0 + 1) - 2 (1+nu) int(k J0(k R) / (1+k^2)^2+nu, k=0..inf)
            //         = 1 - 2 (1+nu) (R/2)^(nu+1) K_{nu+1}(R) / Gamma(nu+2)
            double fnup1 = std::pow(u/2., _nu+1.) * math::cyl_bessel_k(_nu+1., u) / _gamma_nup2;
            double f = 1.0 - 2.0 * (1.+_nu)*fnup1;
            return f - _target;
        }
    private:
        double _nu;
        double _gamma_nup2;
        double _target;
    };

    double SpergelInfo::calculateFluxRadius(const double& flux_frac) const
    {
        // Calcute r such that L(r/r0) / L_tot == flux_frac

        // These bracket the range of calculateFluxRadius(0.5) for -0.85 < nu < 4.0.
        double z1=0.1;
        double z2=3.5;
        SpergelIntegratedFlux func(_nu, _gamma_nup2, flux_frac);
        Solve<SpergelIntegratedFlux> solver(func, z1, z2);
        solver.setXTolerance(1.e-25); // Spergels can be super peaky, so need a tight tolerance.
        solver.setMethod(Brent);
        if (flux_frac < 0.5)
            solver.bracketLowerWithLimit(0.0);
        else
            solver.bracketUpper();
        double R = solver.root();
        dbg<<"flux_frac = "<<flux_frac<<std::endl;
        dbg<<"r/r0 = "<<R<<std::endl;
        return R;
    }

    double SpergelInfo::calculateIntegratedFlux(const double& r) const
    {
        SpergelIntegratedFlux func(_nu, _gamma_nup2);
        return func(r);
    }

    double SpergelInfo::stepK() const
    {
        if (_stepk == 0.) {
            double R = calculateFluxRadius(1.0 - _gsparams->folding_threshold);
            // Go to at least 5*re
            R = std::max(R,_gsparams->stepk_minimum_hlr);
            dbg<<"R => "<<R<<std::endl;
            _stepk = M_PI / R;
            dbg<<"stepk = "<<_stepk<<std::endl;
        }
        return _stepk;
    }

    double SpergelInfo::maxK() const
    {
        if(_maxk == 0.) {
            // Solving (1+k^2)^(-1-nu) = maxk_threshold for k
            // exact:
            // _maxk = std::sqrt(std::pow(gsparams->maxk_threshold, -1./(1+_nu))-1.0);
            // approximate 1+k^2 ~ k^2 => good enough:
            _maxk = std::pow(_gsparams->maxk_threshold, -1./(2*(1+_nu)));
        }
        return _maxk;
    }

    double SpergelInfo::getHLR() const
    {
        if (_re == 0.0) _re = calculateFluxRadius(0.5);
        return _re;
    }

    double SpergelInfo::getXNorm() const
    { return std::pow(2., -_nu) / _gamma_nup1 / (2.0 * M_PI); }

    double SpergelInfo::xValue(double r) const
    {
        if (r == 0.) return _xnorm0;
        else return math::cyl_bessel_k(_nu, r) * std::pow(r, _nu);
    }

    double SpergelInfo::kValue(double ksq) const
    {
        return std::pow(1. + ksq, -1. - _nu);
    }

    class SpergelNuPositiveRadialFunction: public FluxDensity
    {
    public:
        SpergelNuPositiveRadialFunction(double nu, double xnorm0):
            _nu(nu), _xnorm0(xnorm0) {}
        double operator()(double r) const {
            if (r == 0.) return _xnorm0;
            else return math::cyl_bessel_k(_nu, r) * std::pow(r,_nu);
        }
    private:
        double _nu;
        double _xnorm0;
    };

    class SpergelNuNegativeRadialFunction: public FluxDensity
    {
    public:
        SpergelNuNegativeRadialFunction(double nu, double rmin, double a, double b):
            _nu(nu), _rmin(rmin), _a(a), _b(b) {}
        double operator()(double r) const {
            if (r <= _rmin) return _a + _b*r;
            else return math::cyl_bessel_k(_nu, r) * std::pow(r,_nu);
        }
    private:
        double _nu;
        double _rmin;
        double _a;
        double _b;
    };

    boost::shared_ptr<PhotonArray> SpergelInfo::shoot(int N, UniformDeviate ud) const
    {
        dbg<<"SpergelInfo shoot: N = "<<N<<std::endl;
        dbg<<"Target flux = 1.0\n";

        if (!_sampler) {
            // Set up the classes for photon shooting
            double shoot_rmax = calculateFluxRadius(1. - _gsparams->shoot_accuracy);
            if (_nu > 0.) {
                std::vector<double> range(2,0.);
                range[1] = shoot_rmax;
                _radial.reset(new SpergelNuPositiveRadialFunction(_nu, _xnorm0));
                _sampler.reset(new OneDimensionalDeviate( *_radial, range, true, _gsparams));
            } else {
                // exact s.b. profile diverges at origin, so replace the inner most circle
                // (defined such that enclosed flux is shoot_acccuracy) with a linear function
                // that contains the same flux and has the right value at r = rmin.
                // So need to solve the following for a and b:
                // int(2 pi r (a + b r) dr, 0..rmin) = shoot_accuracy
                // a + b rmin = K_nu(rmin) * rmin^nu
                double flux_target = _gsparams->shoot_accuracy;
                double shoot_rmin = calculateFluxRadius(flux_target);
                double knur = math::cyl_bessel_k(_nu, shoot_rmin)*std::pow(shoot_rmin, _nu);
                double b = 3./shoot_rmin*(knur - flux_target/(M_PI*shoot_rmin*shoot_rmin));
                double a = knur - shoot_rmin*b;
                dbg<<"flux target: "<<flux_target<<std::endl;
                dbg<<"shoot rmin: "<<shoot_rmin<<std::endl;
                dbg<<"shoot rmax: "<<shoot_rmax<<std::endl;
                dbg<<"knur: "<<knur<<std::endl;
                dbg<<"b: "<<b<<std::endl;
                dbg<<"a: "<<a<<std::endl;
                dbg<<"a+b*rmin:"<<a+b*shoot_rmin<<std::endl;
                std::vector<double> range(3,0.);
                range[1] = shoot_rmin;
                range[2] = shoot_rmax;
                _radial.reset(new SpergelNuNegativeRadialFunction(_nu, shoot_rmin, a, b));
                _sampler.reset(new OneDimensionalDeviate( *_radial, range, true, _gsparams));
            }
        }

        assert(_sampler.get());
        boost::shared_ptr<PhotonArray> result = _sampler->shoot(N,ud);
        dbg<<"SpergelInfo Realized flux = "<<result->getTotalFlux()<<std::endl;
        return result;
    }

    boost::shared_ptr<PhotonArray> SBSpergel::SBSpergelImpl::shoot(int N, UniformDeviate ud) const
    {
        dbg<<"Spergel shoot: N = "<<N<<std::endl;
        // Get photons from the SpergelInfo structure, rescale flux and size for this instance
        boost::shared_ptr<PhotonArray> result = _info->shoot(N,ud);
        result->scaleFlux(_shootnorm);
        result->scaleXY(_r0);
        dbg<<"Spergel Realized flux = "<<result->getTotalFlux()<<std::endl;
        return result;
    }
}
