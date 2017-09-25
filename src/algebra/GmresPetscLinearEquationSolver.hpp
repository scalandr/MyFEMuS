/*=========================================================================

  Program: FEMUS
  Module: PetscLinearEquationSolver
  Authors: Eugenio Aulisa, Simone Bnà

  Copyright (c) FEMTTU
  All rights reserved.

  This software is distributed WITHOUT ANY WARRANTY; without even
  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the above copyright notice for more information.

  =========================================================================*/

#ifndef __femus_algebra_GmresPetscLinearEquationSolver_hpp__
#define __femus_algebra_GmresPetscLinearEquationSolver_hpp__

#include "FemusConfig.hpp"

#ifdef HAVE_PETSC

#ifdef HAVE_MPI
#include <mpi.h>
#endif

//----------------------------------------------------------------------------
// includes :
//----------------------------------------------------------------------------
#include "LinearEquationSolver.hpp"

namespace femus {

  /**
   * This class inherits the abstract class LinearEquationSolver. In this class the solver is implemented using the PETSc package
   */

  class GmresPetscLinearEquationSolver : public LinearEquationSolver {

    public:

      /**  Constructor. Initializes Petsc data structures */
      GmresPetscLinearEquationSolver(const unsigned &igrid, Solution *other_solution);

      /// Destructor.
      ~GmresPetscLinearEquationSolver();

    protected:
      
      double _scale;

      /// Release all memory and clear data structures.
      void Clear();
      void SetScale(const double &scale){ _scale = scale;};

      void SetTolerances(const double &rtol, const double &atol, const double &divtol,
                         const unsigned &maxits, const unsigned &restart);

      void Init(Mat& Amat, Mat &Pmat);

      void Solve(const vector <unsigned>& variable_to_be_solved, const bool &ksp_clean);

      void MGInit(const MgSmootherType & mg_smoother_type, const unsigned &levelMax, const char* outer_ksp_solver = KSPGMRES);

      void MGSetLevel(LinearEquationSolver *LinSolver, const unsigned &maxlevel,
                      const vector <unsigned> &variable_to_be_solved,
                      SparseMatrix* PP, SparseMatrix* RR,
                      const unsigned &npre, const unsigned &npost);

      void SetSamePreconditioner(){
        _samePreconditioner = true;
      }
      bool UseSamePreconditioner(){
        return _samePreconditioner * _msh->GetIfHomogeneous();
      }

      void RemoveNullSpace();
      void GetNullSpaceBase( std::vector < Vec > &nullspBase);
      void ZerosBoundaryResiduals();
      void SetPenalty();

      virtual void BuildBdcIndex(const vector <unsigned> &variable_to_be_solved);
      virtual void SetPreconditioner(KSP& subksp, PC& subpc);

      void MGSolve(const bool ksp_clean);

      inline void MGClear() {
        KSPDestroy(&_ksp);
      }

      inline KSP* GetKSP() {
        return &_ksp;
      };

      ///  Set the user-specified solver stored in \p _solver_type
      void SetPetscSolverType(KSP &ksp);

      /** @deprecated, remove soon */
      std::pair<unsigned int, double> solve(SparseMatrix&  matrix_in,
                                            SparseMatrix&  precond_in,  NumericVector& solution_in,  NumericVector& rhs_in,
                                            const double tol,   const unsigned int m_its);

      /** @deprecated, remove soon */
      void init(SparseMatrix* matrix);

    protected:

      // member data
      KSP _ksp;    ///< Krylov subspace context
      PC  _pc;      ///< Preconditioner context

      PetscReal _rtol;
      PetscReal _abstol;
      PetscReal _dtol;
      PetscInt  _maxits;
      PetscInt  _restart;

      vector <PetscInt> _bdcIndex;
      vector <PetscInt> _hangingNodesIndex;
      bool _bdcIndexIsInitialized;

      Mat _pmat;


      bool _pmatIsInitialized;
      bool _samePreconditioner;

  };

  // =============================================

  inline GmresPetscLinearEquationSolver::GmresPetscLinearEquationSolver(const unsigned &igrid, Solution *other_solution)
    : LinearEquationSolver(igrid, other_solution) {
    
    if(igrid == 0) {
      this->_preconditioner_type = MLU_PRECOND;
      this->_solver_type         = PREONLY;
    }
    else {
      this->_preconditioner_type = ILU_PRECOND;
      this->_solver_type         = GMRES;
    }

    _scale = 1;
    _rtol   = 1.e-5;
    _abstol = 1.e-50;
    _dtol   = 1.e+5;
    _maxits = 1000;
    _restart = 30;

    _bdcIndexIsInitialized = 0;
    _pmatIsInitialized = false;

    _printSolverInfo = false;

    _samePreconditioner = false;

  }

  // =============================================

  inline GmresPetscLinearEquationSolver::~GmresPetscLinearEquationSolver() {
    this->Clear();
  }

  // ================================================

  inline void GmresPetscLinearEquationSolver::Clear() {

    if(_pmatIsInitialized) {
      _pmatIsInitialized = false;
      MatDestroy(&_pmat);
    }

    if(this->initialized()) {
      this->_is_initialized = false;
      KSPDestroy(&_ksp);
    }


  }

} //end namespace femus


#endif
#endif
