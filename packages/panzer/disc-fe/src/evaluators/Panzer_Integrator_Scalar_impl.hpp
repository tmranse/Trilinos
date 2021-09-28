// @HEADER
// ***********************************************************************
//
//           Panzer: A partial differential equation assembly
//       engine for strongly coupled complex multiphysics systems
//                 Copyright (2011) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
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
// Questions? Contact Roger P. Pawlowski (rppawlo@sandia.gov) and
// Eric C. Cyr (eccyr@sandia.gov)
// ***********************************************************************
// @HEADER

#ifndef PANZER_EVALUATOR_SCALAR_IMPL_HPP
#define PANZER_EVALUATOR_SCALAR_IMPL_HPP

#include "Intrepid2_FunctionSpaceTools.hpp"
#include "Panzer_IntegrationRule.hpp"
#include "Panzer_Workset_Utilities.hpp"
#include "Kokkos_ViewFactory.hpp"
#include "Phalanx_DataLayout_MDALayout.hpp"

namespace panzer {

//**********************************************************************
template<typename EvalT, typename Traits>
Integrator_Scalar<EvalT, Traits>::
Integrator_Scalar(
  const Teuchos::ParameterList& p) : quad_index(0)
{
  Teuchos::RCP<Teuchos::ParameterList> valid_params = this->getValidParameters();
  p.validateParameters(*valid_params);

  Teuchos::RCP<panzer::IntegrationRule> ir = p.get< Teuchos::RCP<panzer::IntegrationRule> >("IR");
  quad_order = ir->cubature_degree;

  Teuchos::RCP<PHX::DataLayout> dl_cell = Teuchos::rcp(new PHX::MDALayout<Cell>(ir->dl_scalar->extent(0)));
  integral = PHX::MDField<ScalarT>( p.get<std::string>("Integral Name"), dl_cell);
  scalar = PHX::MDField<const ScalarT,Cell,IP>( p.get<std::string>("Integrand Name"), ir->dl_scalar);

  this->addEvaluatedField(integral);
  this->addDependentField(scalar);
    
  multiplier = 1.0;
  if(p.isType<double>("Multiplier"))
     multiplier = p.get<double>("Multiplier");

  if (p.isType<Teuchos::RCP<const std::vector<std::string> > >("Field Multipliers")) {
    const std::vector<std::string>& field_multiplier_names = 
      *(p.get<Teuchos::RCP<const std::vector<std::string> > >("Field Multipliers"));

    field_multipliers_h = typename PHX::View<PHX::UnmanagedView<const ScalarT**>* >::HostMirror("FieldMultipliersHost", field_multiplier_names.size());
    field_multipliers = PHX::View<PHX::UnmanagedView<const ScalarT**>* >("FieldMultipliersDevice", field_multiplier_names.size());

    int cnt=0;
    for (std::vector<std::string>::const_iterator name = 
	   field_multiplier_names.begin(); 
	 name != field_multiplier_names.end(); ++name, ++cnt) {
      PHX::MDField<const ScalarT,Cell,IP> tmp_field(*name, p.get< Teuchos::RCP<panzer::IntegrationRule> >("IR")->dl_scalar);
      this->addDependentField(tmp_field.fieldTag(),field_multipliers_h(cnt));
    }
  }

  std::string n = "Integrator_Scalar: " + integral.fieldTag().name();
  this->setName(n);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void
Integrator_Scalar<EvalT, Traits>::
postRegistrationSetup(
  typename Traits::SetupData sd,
  PHX::FieldManager<Traits>& /* fm */)
{
  num_qp = scalar.extent(1);
  tmp = Kokkos::createDynRankView(scalar.get_static_view(),"tmp", scalar.extent(0), num_qp);
  quad_index =  panzer::getIntegrationRuleIndex(quad_order,(*sd.worksets_)[0], this->wda);

  Kokkos::deep_copy(field_multipliers, field_multipliers_h);
}


//**********************************************************************
template<typename EvalT, typename Traits>
void
Integrator_Scalar<EvalT, Traits>::
evaluateFields(
  typename Traits::EvalData workset)
{ 

  const auto wm = this->wda(workset).int_rules[quad_index]->weighted_measure;

  auto l_tmp = tmp;
  auto l_multiplier = multiplier;
  auto l_scalar = scalar;
  auto l_field_multipliers = field_multipliers;
  auto l_num_qp = num_qp;
  auto l_integral = integral.get_static_view();

  Kokkos::parallel_for("IntegratorScalar", workset.num_cells, KOKKOS_LAMBDA (int cell) {
    for (std::size_t qp = 0; qp < l_num_qp; ++qp) {
      l_tmp(cell,qp) = l_multiplier * l_scalar(cell,qp);
      for (size_t f=0;f<l_field_multipliers.extent(0); ++f)
        l_tmp(cell,qp) *= l_field_multipliers(f)(cell,qp);  
    }
    l_integral(cell) = 0.0;
    for (std::size_t qp = 0; qp < l_num_qp; ++qp) {
      l_integral(cell) += l_tmp(cell, qp)*wm(cell, qp);
    }
  } );
  Kokkos::fence();
}

//**********************************************************************
template<typename EvalT, typename TRAITS>
Teuchos::RCP<Teuchos::ParameterList> 
Integrator_Scalar<EvalT, TRAITS>::getValidParameters() const
{
  Teuchos::RCP<Teuchos::ParameterList> p = Teuchos::rcp(new Teuchos::ParameterList);
  p->set<std::string>("Integral Name", "?");
  p->set<std::string>("Integrand Name", "?");

  Teuchos::RCP<panzer::IntegrationRule> ir;
  p->set("IR", ir);
  p->set<double>("Multiplier", 1.0);

  Teuchos::RCP<const std::vector<std::string> > fms;
  p->set("Field Multipliers", fms);
  return p;
}

//**********************************************************************

}

#endif
