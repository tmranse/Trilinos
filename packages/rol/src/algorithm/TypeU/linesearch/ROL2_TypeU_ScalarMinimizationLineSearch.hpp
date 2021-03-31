// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

#pragma once
#ifndef ROL2_TYPEU_SCALARMINIMIZATIONLINESEARCH_H
#define ROL2_TYPEU_SCALARMINIMIZATIONLINESEARCH_H

/** \class ROL::ROL2::TypeU::ScalarMinimizationLineSearch
    \brief Implements line search methods that attempt to minimize the
           scalar function \f$\phi(t) := f(x+ts)\f$.
*/

#include "ROL2_TypeU_LineSearch.hpp"
#include "ROL_BrentsScalarMinimization.hpp"
#include "ROL_BisectionScalarMinimization.hpp"
#include "ROL_GoldenSectionScalarMinimization.hpp"
#include "ROL_ScalarFunction.hpp"
#include "ROL_Bracketing.hpp"

namespace ROL {
namespace ROL2 {
namespace TypeU {

template<typename Real>
class ScalarMinimizationLineSearch : public LineSearch<Real> {
private:
  Ptr<Vector<Real>>             xnew_; 
  Ptr<Vector<Real>>             g_;
  Ptr<ScalarMinimization<Real>> sm_;
  Ptr<Bracketing<Real>>         br_;
  Ptr<ScalarFunction<Real>>     sf_;

  using CurvatureCondition = typename LineSearch<Real>::CurvatureCondition;
  using DescentType = typename DescentDirection<Real>::Type;

  CurvatureCondition econd_;
  Real c1_;
  Real c2_;
  Real c3_;
  int max_nfval_;

  bool FDdirDeriv_;

  class Phi : public ScalarFunction<Real> {
  private:
    const Ptr<Vector<Real>> xnew_;
    const Ptr<const Vector<Real>> x_, s_;
    const Ptr<Objective<Real>> obj_;
    Real ftol_, alpha_, val_;
    bool FDdirDeriv_;
  public:
    Phi(const Ptr<Vector<Real>> &xnew,
        const Ptr<const Vector<Real>> &x,
        const Ptr<const Vector<Real>> &s,
        const Ptr<Objective<Real>> &obj,
        const bool FDdirDeriv = false)
     : xnew_(xnew), x_(x), s_(s), obj_(obj),
       ftol_(std::sqrt(ROL_EPSILON<Real>())),
       alpha_(ROL_INF<Real>()), val_(ROL_INF<Real>()),
       FDdirDeriv_(FDdirDeriv) {}
    Real value(const Real alpha) {
      if (alpha_ != alpha) {
        alpha_ = alpha;
        xnew_->set(*x_); xnew_->axpy(alpha,*s_);
        obj_->update(*xnew_,UPDATE_TRIAL);
        val_ = obj_->value(*xnew_,ftol_);
      }
      return val_;
    }
    Real deriv(const Real alpha) {
      Real tol = std::sqrt(ROL_EPSILON<Real>());
      Real val(0);
      xnew_->set(*x_); xnew_->axpy(alpha,*s_);
      if (FDdirDeriv_) {
        Real snorm = s_->norm();
        if (snorm > static_cast<Real>(0)) {
          Real xnorm   = xnew_->norm();
          Real cbrteps = std::cbrt(ROL_EPSILON<Real>());
          Real h       = cbrteps*std::max(xnorm/snorm,static_cast<Real>(1));
          Real fnew    = value(alpha);
          xnew_->axpy(h,*s_);
          obj_->update(*xnew_,UPDATE_TRIAL);
          Real ftrial = obj_->value(*xnew_,tol);
          val = (ftrial - fnew) / h;
        }
      }
      else {
        val = obj_->dirDeriv(*xnew_,*s_,tol);
      }
      return val;
    }
  };

  class StatusTest : public ScalarMinimizationStatusTest<Real> {
  private:
    Ptr<ScalarFunction<Real>> phi_;

    const Real f0_;
    const Real g0_;

    const Real c1_;
    const Real c2_;
    const Real c3_;
    const int max_nfval_;
    const CurvatureCondition econd_;

  public:
    StatusTest( Real f0, Real g0,
                Real c1, Real c2, Real c3,
                int max_nfval, CurvatureCondition econd,
               const Ptr<ScalarFunction<Real>> &phi)
      : phi_(phi), f0_(f0), g0_(g0), c1_(c1), c2_(c2), c3_(c3),
        max_nfval_(max_nfval), econd_(econd) {}

    bool check(Real &x, Real &fx, Real &gx,
               int &nfval, int &ngval, const bool deriv = false) {
      Real one(1), two(2);
      bool armijo = (fx <= f0_ + c1_*x*g0_);
//      bool itcond = (nfval >= max_nfval_);
      bool curvcond = false;
//      if (armijo && !itcond) {
      if (armijo) {
        if (econd_ == CurvatureCondition::Goldstein) {
          curvcond = (fx >= f0_ + (one-c1_)*x*g0_);
        }
        else if (econd_ == CurvatureCondition::Null) {
          curvcond = true;
        }
        else {
          if (!deriv) {
            gx = phi_->deriv(x); ngval++;
          }
          if (econd_ == CurvatureCondition::Wolfe) {
            curvcond = (gx >= c2_*g0_);
          }
          else if (econd_ == CurvatureCondition::StrongWolfe) {
            curvcond = (std::abs(gx) <= c2_*std::abs(g0_));
          }
          else if (econd_ == CurvatureCondition::GeneralizedWolfe) {
            curvcond = (c2_*g0_ <= gx && gx <= -c3_*g0_);
          }
          else if (econd_ == CurvatureCondition::ApproximateWolfe) {
            curvcond = (c2_*g0_ <= gx && gx <= (two*c1_ - one)*g0_);
          }
        }
      }
      //return (armijo && curvcond) || itcond;
      return (armijo && curvcond);
    }
  };

  using LineSearch<Real>::getInitialAlpha;
  using LineSearch<Real>::setNextInitialAlpha;

public:
  // Constructor
  ScalarMinimizationLineSearch( ParameterList &parlist, 
    const Ptr<ScalarMinimization<Real>> &sm = nullPtr,
    const Ptr<Bracketing<Real>>         &br = nullPtr,
    const Ptr<ScalarFunction<Real>>     &sf = nullPtr )
    : LineSearch<Real>(parlist) {
    const Real zero(0), p4(0.4), p6(0.6), p9(0.9), oem4(1.e-4), oem10(1.e-10), one(1);
    ParameterList &list0 = parlist.sublist("Step").sublist("Line Search");
    FDdirDeriv_ = list0.get("Finite Difference Directional Derivative",false);
    ParameterList &lsmlist  = list0.sublist("Line-Search Method");
    // Get Bracketing Method
    if( br == nullPtr ) {
      br_ = makePtr<Bracketing<Real>>();
    }
    else {
      br_ = br;
    }
    // Get ScalarMinimization Method
    std::string type = lsmlist.get("Type","Brent's");
    Real tol         = lsmlist.sublist(type).get("Tolerance",oem10);
    int niter        = lsmlist.sublist(type).get("Iteration Limit",1000);
    ROL::ParameterList plist;
    plist.sublist("Scalar Minimization").set("Type",type);
    plist.sublist("Scalar Minimization").sublist(type).set("Tolerance",tol);
    plist.sublist("Scalar Minimization").sublist(type).set("Iteration Limit",niter);

    if( sm == nullPtr ) { // No user-provided ScalarMinimization object
      if ( type == "Brent's" ) {
        sm_ = makePtr<BrentsScalarMinimization<Real>>(plist);
      }
      else if ( type == "Bisection" ) {
        sm_ = makePtr<BisectionScalarMinimization<Real>>(plist);
      }
      else if ( type == "Golden Section" ) {
        sm_ = makePtr<GoldenSectionScalarMinimization<Real>>(plist);
      }
      else {
        ROL_TEST_FOR_EXCEPTION(true, std::invalid_argument,
          ">>> (ROL::ScalarMinimizationLineSearch): Undefined ScalarMinimization type!");
      }
    }
    else {
      sm_ = sm;
    }
    sf_ = sf;

    // Status test for line search

    econd_ = LineSearch<Real>::cc_dict[list0.sublist("Curvature Condition").get("Type","Strong Wolfe Conditions")];
    max_nfval_ = list0.get("Function Evaluation Limit",20);
    c1_        = list0.get("Sufficient Decrease Tolerance",oem4);
    c2_        = list0.sublist("Curvature Condition").get("General Parameter",p9);
    c3_        = list0.sublist("Curvature Condition").get("Generalized Wolfe Parameter",p6);
    // Check status test inputs
    c1_ = ((c1_ < zero) ? oem4 : c1_);
    c2_ = ((c2_ < zero) ? p9   : c2_);
    c3_ = ((c3_ < zero) ? p9   : c3_);
    if ( c2_ <= c1_ ) {
      c1_ = oem4;
      c2_ = p9;
    }
    auto edesc = DescentDirection<Real>::type_dict[list0.sublist("Descent Method").get("Type","Quasi-Newton Method")];
    if ( edesc == DescentType::NonlinearCG ) {
      c2_ = p4;
      c3_ = std::min(one-c2_,c3_);
    }
  }

  void initialize(const Vector<Real> &x, const Vector<Real> &g) {
    LineSearch<Real>::initialize(x,g);
    xnew_ = x.clone();
    g_    = g.clone();
  }

  // Find the minimum of phi(alpha) = f(x + alpha*s) using Brent's method
  void run( Real &alpha, Real &fval, int &ls_neval, int &ls_ngrad,
            const Real &gs, const Vector<Real> &s, const Vector<Real> &x, 
            Objective<Real> &obj ) override {
    ls_neval = 0; ls_ngrad = 0;

    // Get initial line search parameter
    alpha = getInitialAlpha(ls_neval,ls_ngrad,fval,gs,x,s,obj);

    // Build ScalarFunction and ScalarMinimizationStatusTest
    Ptr<const Vector<Real>> x_ptr = ROL::makePtrFromRef(x);
    Ptr<const Vector<Real>> s_ptr = ROL::makePtrFromRef(s);
    Ptr<Objective<Real>> obj_ptr = ROL::makePtrFromRef(obj);

    Ptr<ScalarFunction<Real>> phi;
    if( sf_ == ROL::nullPtr ) {
      phi = ROL::makePtr<Phi>(xnew_,x_ptr,s_ptr,obj_ptr,FDdirDeriv_);
    }
    else {
      phi = sf_;
    }

    Ptr<ScalarMinimizationStatusTest<Real>> test
      = makePtr<StatusTest>(fval,gs,c1_,c2_,c3_,max_nfval_,econd_,phi);

    // Run Bracketing
    int nfval = 0, ngrad = 0;
    Real A(0),      fA = fval;
    Real B = alpha, fB = phi->value(B);
    br_->run(alpha,fval,A,fA,B,fB,nfval,ngrad,*phi,*test); 
    B = alpha;
    ls_neval += nfval; ls_ngrad += ngrad;

    // Run ScalarMinimization
    nfval = 0, ngrad = 0;
    sm_->run(fval, alpha, nfval, ngrad, *phi, A, B, *test);
    ls_neval += nfval; ls_ngrad += ngrad;

    setNextInitialAlpha(alpha); 
  }
}; // class ROL::ROL2::TypeU::ScalarMinimization

} // namespace TypeU 
} // namespace ROL2 
} // namespace ROL

namespace ROL2 {
namespace TypeU {
using ROL::ROL2::TypeU::ScalarMinimizationLineSearch;
} // namespace TypeU 
} // namespace ROL2 
#endif
