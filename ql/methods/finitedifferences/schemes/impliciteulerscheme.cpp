/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2009 Andreas Gaida
 Copyright (C) 2009 Ralph Schreyer
 Copyright (C) 2009, 2017 Klaus Spanderen

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/math/matrixutilities/gmres.hpp>
#include <ql/math/matrixutilities/bicgstab.hpp>
#include <ql/methods/finitedifferences/schemes/impliciteulerscheme.hpp>
#if defined(__GNUC__) && (((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)) || (__GNUC__ > 4))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#include <boost/bind.hpp>
#if defined(__GNUC__) && (((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)) || (__GNUC__ > 4))
#pragma GCC diagnostic pop
#endif
#include <ql/function.hpp>
#include <utility>

namespace QuantLib {

    ImplicitEulerScheme::ImplicitEulerScheme(
        ext::shared_ptr<FdmLinearOpComposite>  map,
        const bc_set& bcSet,
        Real relTol,
        SolverType solverType)
    : dt_        (Null<Real>()),
      iterations_(ext::make_shared<Size>(0u)),
      relTol_    (relTol),
      map_       (std::move(map)),
      bcSet_     (bcSet),
      solverType_(solverType){
    }

    Disposable<Array> ImplicitEulerScheme::apply(const Array& r) const {
        return r - dt_*map_->apply(r);
    }

    void ImplicitEulerScheme::step(array_type& a, Time t) {
        QL_REQUIRE(t-dt_ > -1e-8, "a step towards negative time given");
        map_->setTime(std::max(0.0, t-dt_), t);
        bcSet_.setTime(std::max(0.0, t-dt_));

        bcSet_.applyBeforeSolving(*map_, a);

        if (map_->size() == 1) {
            a = map_->solve_splitting(0, a, -dt_);
        }
        else {
            const ext::function<Disposable<Array>(const Array&)>
                preconditioner(boost::bind(
                    &FdmLinearOpComposite::preconditioner, map_, _1, -dt_));

            const ext::function<Disposable<Array>(const Array&)> applyF(
                boost::bind(&ImplicitEulerScheme::apply, this, _1));

            if (solverType_ == BiCGstab) {
                const BiCGStabResult result =
                    QuantLib::BiCGstab(applyF, std::max(Size(10), a.size()),
                        relTol_, preconditioner).solve(a, a);

                (*iterations_) += result.iterations;
                a = result.x;
            }
            else if (solverType_ == GMRES) {
                const GMRESResult result =
                    QuantLib::GMRES(applyF, std::max(Size(10), a.size()/10u),
                        relTol_, preconditioner).solve(a, a);

                (*iterations_) += result.errors.size();
                a = result.x;
            }
            else
                QL_FAIL("unknown/illegal solver type");
        }
        bcSet_.applyAfterSolving(a);
    }

    void ImplicitEulerScheme::setStep(Time dt) {
        dt_=dt;
    }

    Size ImplicitEulerScheme::numberOfIterations() const {
        return *iterations_;
    }
}
