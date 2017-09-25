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

//----------------------------------------------------------------------------
// includes :
//----------------------------------------------------------------------------
#include "FemusConfig.hpp"

#ifdef HAVE_PETSC

// Local Includes
#include "PetscMacro.hpp"
#include "GmresPetscLinearEquationSolver.hpp"
#include "PetscPreconditioner.hpp"
#include "PetscVector.hpp"
#include "PetscMatrix.hpp"
#include <iomanip>
#include <sstream>

namespace femus {

  using namespace std;

    
// ==============================================
// ----------------------- functions ------
// ==============================================

  void GmresPetscLinearEquationSolver::SetTolerances(const double& rtol, const double& atol, const double& divtol,
      const unsigned& maxits, const unsigned& restart) {
    _rtol    = static_cast<PetscReal>(rtol);
    _abstol  = static_cast<PetscReal>(atol);
    _dtol    = static_cast<PetscReal>(divtol);
    _maxits  = static_cast<PetscInt>(maxits);
    _restart = static_cast<PetscInt>(restart);
  }

  // ================================================

  void GmresPetscLinearEquationSolver::BuildBdcIndex(const vector <unsigned>& variable_to_be_solved) {

    _bdcIndexIsInitialized = 1;

    unsigned BDCIndexSize = KKoffset[KKIndex.size() - 1][processor_id()] - KKoffset[0][processor_id()];
    _bdcIndex.resize(BDCIndexSize);
    _hangingNodesIndex.resize(BDCIndexSize);

    vector <bool> ThisSolutionIsIncluded(_SolPdeIndex.size(), false);

    for(unsigned iind = 0; iind < variable_to_be_solved.size(); iind++) {
      unsigned PdeIndexSol = variable_to_be_solved[iind];
      ThisSolutionIsIncluded[PdeIndexSol] = true;
    }

    unsigned count0 = 0;
    unsigned count1 = 0;

    for(int k = 0; k < _SolPdeIndex.size(); k++) {
      unsigned indexSol = _SolPdeIndex[k];
      unsigned soltype = _SolType[indexSol];

      for(unsigned inode_mts = _msh->_dofOffset[soltype][processor_id()];
          inode_mts < _msh->_dofOffset[soltype][processor_id() + 1]; inode_mts++) {
        int local_mts = inode_mts - _msh->_dofOffset[soltype][processor_id()];
        int idof_kk = KKoffset[k][processor_id()] + local_mts;

        if(!ThisSolutionIsIncluded[k] || (* (*_Bdc) [indexSol])(inode_mts) < 0.9) {
          _bdcIndex[count0] = idof_kk;
          count0++;
        }
        else if(!ThisSolutionIsIncluded[k] || (* (*_Bdc) [indexSol])(inode_mts) < 1.9) {
          _hangingNodesIndex[count1] = idof_kk;
          count1++;
        }
      }
    }

    _bdcIndex.resize(count0);
    std::vector < PetscInt >(_bdcIndex).swap(_bdcIndex);
    std::sort(_bdcIndex.begin(), _bdcIndex.end());

    _hangingNodesIndex.resize(count1);
    std::vector < PetscInt >(_hangingNodesIndex).swap(_hangingNodesIndex);
    std::sort(_hangingNodesIndex.begin(), _hangingNodesIndex.end());


    return;
  }

  // ================================================

  void GmresPetscLinearEquationSolver::Solve(const vector <unsigned>& variable_to_be_solved, const bool& ksp_clean) {

    PetscLogDouble t1;
    PetscTime(&t1);

    if(_bdcIndexIsInitialized == 0) BuildBdcIndex(variable_to_be_solved);

    //BEGIN ASSEMBLE matrix with Dirichlet penalty BCs by penalty
    Mat KK = (static_cast<PetscMatrix*>(_KK))->mat();
    if(ksp_clean) {
      this->Clear();
      SetPenalty();
      RemoveNullSpace();
      if( UseSamePreconditioner() ) {
        this->Init(KK, KK);
      }
      else{
        this->Init(KK, _pmat);
      }
    }
    //END ASSEMBLE

    //BEGIN SOLVE and UPDATE
    ZerosBoundaryResiduals();
    KSPSolve(_ksp, (static_cast< PetscVector* >(_RES))->vec(), (static_cast< PetscVector* >(_EPSC))->vec());
    *_EPS += *_EPSC;
    _RESC->matrix_mult(*_EPSC, *_KK);
    *_RES -= *_RESC;
    //END SOLVE and UPDATE

    //BEGIN PRINT Computational info
    if(_printSolverInfo) {
      int its;
      KSPGetIterationNumber(_ksp, &its);

      KSPConvergedReason reason;
      KSPGetConvergedReason(_ksp, &reason);

      PetscReal rnorm;
      KSPGetResidualNorm(_ksp, &rnorm);

      PetscLogDouble t2;
      PetscTime(&t2);

      PetscPrintf(PETSC_COMM_WORLD, "        *************** ML linear solver time: %e \n", t2 - t1);
      PetscPrintf(PETSC_COMM_WORLD, "        *************** Number of outer ksp solver iterations = %i \n", its);
      PetscPrintf(PETSC_COMM_WORLD, "        *************** Convergence reason = %i \n", reason);
      PetscPrintf(PETSC_COMM_WORLD, "        *************** Residual norm = %10.8g \n", rnorm);
    }

    //END PRINT
  }

  // ================================================

  void GmresPetscLinearEquationSolver::Init(Mat& Amat, Mat& Pmat) {

    if(!this->initialized())    {
      this->_is_initialized = true;

      KSPCreate(MPI_COMM_WORLD, &_ksp);
      KSPGetPC(_ksp, &_pc);

      this->SetPetscSolverType(_ksp);

      KSPSetOperators(_ksp, Amat, Pmat);
      KSPSetTolerances(_ksp, _rtol, _abstol, _dtol, _maxits);

      if(_solver_type != PREONLY) {
        //KSPSetInitialGuessKnoll(_ksp, PETSC_TRUE);
        KSPSetNormType(_ksp, KSP_NORM_NONE);
      }

      KSPSetFromOptions(_ksp);
      KSPGMRESSetRestart(_ksp, _restart);

      SetPreconditioner(_ksp, _pc);
    }
  }

  // ================================================

  void GmresPetscLinearEquationSolver::MGInit(const MgSmootherType & mg_smoother_type, const unsigned &levelMax, const char* outer_ksp_solver) {

    KSPCreate(PETSC_COMM_WORLD, &_ksp);

    KSPSetType(_ksp, outer_ksp_solver);

    KSPGetPC(_ksp, &_pc);
    PCSetType(_pc, PCMG);
    PCMGSetLevels(_pc, levelMax, NULL);

    if(mg_smoother_type == FULL) {
      PCMGSetType(_pc, PC_MG_FULL);
    }
    else if(mg_smoother_type == MULTIPLICATIVE) {
      PCMGSetType(_pc, PC_MG_MULTIPLICATIVE);
    }
    else if(mg_smoother_type == ADDITIVE) {
      PCMGSetType(_pc, PC_MG_ADDITIVE);
    }
    else if(mg_smoother_type == KASKADE) {
      PCMGSetType(_pc, PC_MG_KASKADE);
    }
    else {
      std::cout << "Wrong mg_type for PETSCsolve()" << std::endl;
      abort();
    }
  };

  // ================================================

  void GmresPetscLinearEquationSolver::MGSetLevel(
    LinearEquationSolver* LinSolver, const unsigned& levelMax,
    const vector <unsigned>& variable_to_be_solved, SparseMatrix* PP, SparseMatrix* RR,
    const unsigned& npre, const unsigned& npost) {

    unsigned level = _msh->GetLevel();

    // ***************** NODE/ELEMENT SEARCH *******************
    if(_bdcIndexIsInitialized == 0) BuildBdcIndex(variable_to_be_solved);
    // ***************** END NODE/ELEMENT SEARCH *******************

    KSP* kspMG = LinSolver->GetKSP();
    PC pcMG;
    KSPGetPC(*kspMG, &pcMG);

    KSP subksp;

    if(level == 0) {
      PCMGGetCoarseSolve(pcMG, &subksp);
    }
    else {
      PCMGGetSmoother(pcMG, level , &subksp);
      KSPSetTolerances(subksp, PETSC_DEFAULT, PETSC_DEFAULT, PETSC_DEFAULT, npre);
    }

    this->SetPetscSolverType(subksp);
    std::ostringstream levelName;
    levelName << "level-" << level;
    KSPSetOptionsPrefix(subksp, levelName.str().c_str());
    KSPSetFromOptions(subksp);

    ZerosBoundaryResiduals();

    SetPenalty();
    RemoveNullSpace();

    Mat KK = (static_cast< PetscMatrix* >(_KK))->mat();
    if( UseSamePreconditioner() ) {
      KSPSetOperators(subksp, KK, KK);
    }
    else{
      KSPSetOperators(subksp, KK, _pmat);
    }

    PC subpc;
    KSPGetPC(subksp, &subpc);
    SetPreconditioner(subksp, subpc);

    if(level < levelMax) {
      PCMGSetX(pcMG, level, (static_cast< PetscVector* >(_EPS))->vec());
      PCMGSetRhs(pcMG, level, (static_cast< PetscVector* >(_RES))->vec());
    }

    if(level > 0) {
      PCMGSetR(pcMG, level, (static_cast<PetscVector*>(_RESC))->vec());
      PCMGSetInterpolation(pcMG, level, (static_cast< PetscMatrix* >(PP))->mat());
      PCMGSetRestriction(pcMG, level, (static_cast< PetscMatrix* >(RR))->mat());

      if(npre != npost) {
        KSP subkspUp;
        PCMGGetSmootherUp(pcMG, level , &subkspUp);
        KSPSetTolerances(subkspUp, PETSC_DEFAULT, PETSC_DEFAULT, PETSC_DEFAULT, npost);
        this->SetPetscSolverType(subkspUp);
        KSPSetPC(subkspUp, subpc);
        PC subpcUp;
        KSPGetPC(subkspUp, &subpcUp);
        KSPSetUp(subkspUp);
      }
    }
  }

  // ================================================

  void GmresPetscLinearEquationSolver::MGSolve(const bool ksp_clean) {

    PetscLogDouble t1;
    PetscLogDouble t2;
    PetscTime(&t1);

    if(ksp_clean) {
      Mat KK = (static_cast< PetscMatrix* >(_KK))->mat();

      if( UseSamePreconditioner() ) {
        KSPSetOperators(_ksp, KK, KK);
      }
      else {
        KSPSetOperators(_ksp, KK, _pmat);
      }

      KSPSetTolerances(_ksp, _rtol, _abstol, _dtol, _maxits);

      if(_solver_type != PREONLY) {
        KSPSetInitialGuessKnoll(_ksp, PETSC_TRUE);
      }

      KSPSetFromOptions(_ksp);
      KSPGMRESSetRestart(_ksp, _restart);
      KSPSetUp(_ksp);

    }

    ZerosBoundaryResiduals();


    KSPSolve(_ksp, (static_cast< PetscVector* >(_RES))->vec(), (static_cast< PetscVector* >(_EPSC))->vec());

    _RESC->matrix_mult(*_EPSC, *_KK);
    *_RES -= *_RESC;
    *_EPS += *_EPSC;

    if(_printSolverInfo) {
      int its;
      KSPGetIterationNumber(_ksp, &its);

      KSPConvergedReason reason;
      KSPGetConvergedReason(_ksp, &reason);

      PetscReal rnorm;
      KSPGetResidualNorm(_ksp, &rnorm);

      PetscTime(&t2);
      PetscPrintf(PETSC_COMM_WORLD, "       *************** MG linear solver time: %e \n", t2 - t1);
      PetscPrintf(PETSC_COMM_WORLD, "       *************** Number of outer ksp solver iterations = %i \n", its);
      PetscPrintf(PETSC_COMM_WORLD, "       *************** Convergence reason = %i \n", reason);
      PetscPrintf(PETSC_COMM_WORLD, "       *************** Residual norm = %10.8g \n", rnorm);
    }
  }

  // ================================================

  void GmresPetscLinearEquationSolver::RemoveNullSpace() {

    if( _msh->GetLevel() != 0) {
      std::vector < Vec > nullspBase;
      GetNullSpaceBase(nullspBase);
      if(nullspBase.size() != 0) {
        MatNullSpace   nullsp;
        MatNullSpaceCreate(PETSC_COMM_WORLD, PETSC_FALSE, nullspBase.size(), &nullspBase[0], &nullsp);

        PetscBool  isNull;
        MatNullSpaceTest(nullsp, (static_cast< PetscMatrix* >(_KK))->mat(), &isNull);
        if (!isNull) std::cout<<"The null space created for KK is not correct!"<<std::endl;

        MatSetNullSpace( (static_cast< PetscMatrix* >(_KK))->mat(), nullsp);
        MatSetTransposeNullSpace( (static_cast< PetscMatrix* >(_KK))->mat(), nullsp);
        if( !UseSamePreconditioner() ) {
          MatNullSpaceTest(nullsp, _pmat, &isNull);
          if (!isNull) std::cout<<"The null space created for _pmat is not correct!"<<std::endl;
          MatSetNullSpace( _pmat, nullsp);
          MatSetTransposeNullSpace( _pmat, nullsp);
        }
        MatNullSpaceDestroy(&nullsp);

        for(unsigned i = 0; i < nullspBase.size(); i++) {
          VecDestroy(&nullspBase[i]);
        }
      }
    }

  }

  // ================================================

  void GmresPetscLinearEquationSolver::GetNullSpaceBase(std::vector < Vec > &nullspBase) {
    for(int k = 0; k < _SolPdeIndex.size(); k++) {
      unsigned indexSol = _SolPdeIndex[k];

      if(_solution->GetIfRemoveNullSpace(indexSol)) {
        Vec EPS = (static_cast< PetscVector* >(_EPS))->vec();
        unsigned nullspSize = nullspBase.size();
        nullspBase.resize(nullspSize + 1);
        VecDuplicate(EPS, &nullspBase[nullspSize]);
        unsigned soltype = _SolType[indexSol];
        unsigned owndofs = _msh->_dofOffset[soltype][processor_id() + 1] - _msh->_dofOffset[soltype][processor_id()];
        if ( soltype == 4 ) owndofs /= ( _msh->GetDimension() + 1 );
        for(unsigned i = 0; i < owndofs; i++) {
          int idof_kk = KKoffset[k][processor_id()] + i;
          VecSetValue(nullspBase[nullspSize], idof_kk, 1., INSERT_VALUES);
        }

        VecAssemblyBegin(nullspBase[nullspSize]);
        VecAssemblyEnd(nullspBase[nullspSize]);
        VecNormalize(nullspBase[nullspSize], NULL);

        nullspSize++;
      }
    }
  }
  // =================================================

  void GmresPetscLinearEquationSolver::ZerosBoundaryResiduals() {
    std::vector< PetscScalar > value(_bdcIndex.size(), 0.);
    Vec RES = (static_cast< PetscVector* >(_RES))->vec();
    VecSetValues(RES, _bdcIndex.size(), &_bdcIndex[0], &value[0],  INSERT_VALUES);
    VecAssemblyBegin(RES);
    VecAssemblyEnd(RES);
  }

  // =================================================

  void GmresPetscLinearEquationSolver::SetPenalty() {

    Mat KK = (static_cast< PetscMatrix* >(_KK))->mat();

    MatSetOption(KK, MAT_NO_OFF_PROC_ZERO_ROWS, PETSC_TRUE);
    MatSetOption(KK, MAT_KEEP_NONZERO_PATTERN, PETSC_TRUE);
    MatZeroRows(KK, _bdcIndex.size(), &_bdcIndex[0], 1.e100, 0, 0);

    if( !UseSamePreconditioner() ) {
      if(_pmatIsInitialized) MatDestroy(&_pmat);
      MatDuplicate(KK, MAT_COPY_VALUES, &_pmat);
      if( _hangingNodesIndex.size() != 0){
        MatSetOption(_pmat, MAT_NO_OFF_PROC_ZERO_ROWS, PETSC_TRUE);
        MatZeroRows(_pmat, _hangingNodesIndex.size(), &_hangingNodesIndex[0], 1.e100, 0, 0);
      }
      _pmatIsInitialized = true;
    }
  }

  // =================================================

  void GmresPetscLinearEquationSolver::SetPreconditioner(KSP& subksp, PC& subpc) {
    PetscPreconditioner::set_petsc_preconditioner_type(this->_preconditioner_type, subpc);
    PetscReal zero = 1.e-16;
    PCFactorSetZeroPivot(subpc, zero);
    PCFactorSetShiftType(subpc, MAT_SHIFT_NONZERO);
  }

  // ================================================

  void GmresPetscLinearEquationSolver::SetPetscSolverType(KSP& ksp) {
    int ierr = 0;

    unsigned level;
    double scale;
    unsigned numberOfUniformLevels;
    
    switch(this->_solver_type) {
      case CG:
        ierr = KSPSetType(ksp, (char*) KSPCG);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case CR:
        ierr = KSPSetType(ksp, (char*) KSPCR);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case CGS:
        ierr = KSPSetType(ksp, (char*) KSPCGS);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case BICG:
        ierr = KSPSetType(ksp, (char*) KSPBICG);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case TCQMR:
        ierr = KSPSetType(ksp, (char*) KSPTCQMR);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case TFQMR:
        ierr = KSPSetType(ksp, (char*) KSPTFQMR);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case LSQR:
        ierr = KSPSetType(ksp, (char*) KSPLSQR);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case BICGSTAB:
        ierr = KSPSetType(ksp, (char*) KSPBCGS);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case MINRES:
        ierr = KSPSetType(ksp, (char*) KSPMINRES);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case GMRES:
        ierr = KSPSetType(ksp, (char*) KSPGMRES);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case FGMRES:
        ierr = KSPSetType(ksp, (char*) KSPFGMRES);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case RICHARDSON:
        KSPSetType(ksp, (char*) KSPRICHARDSON);
	
 	
	
	//scale = 1.2;
	
	//BEGIN da commentare
// 	level = _msh->GetLevel();
// 	numberOfUniformLevels = 1; // deve corrispondere a numberOfUniformLevels nel main
// 	
// 	if(numberOfUniformLevels == 1){
// 	  if(level > numberOfUniformLevels ){
// 	    scale /= (1 + level- numberOfUniformLevels);
// 	  }
// 	}
// 	else{
// 	  if (level >= numberOfUniformLevels ){
// 	    scale /= (2 + level - numberOfUniformLevels); //da controllare qnd numberOfUniformLevels lo mettiamo >1
// 	  }
// 	}
	//END da commentare
	
	//std::cout << "level = " << level << "scale = "<< scale << std::endl;
	
	//KSPRichardsonSetScale(ksp, scale);
        KSPRichardsonSetSelfScale(ksp, PETSC_TRUE);
        return;

      case CHEBYSHEV:
        ierr = KSPSetType(ksp, (char*) KSPCHEBYSHEV);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      case PREONLY:
        ierr = KSPSetType(ksp, (char*) KSPPREONLY);
        CHKERRABORT(MPI_COMM_WORLD, ierr);
        return;

      default:
        std::cerr << "ERROR:  Unsupported PETSC Solver: "
                  << this->_solver_type               << std::endl
                  << "Continuing with PETSC defaults" << std::endl;
    }
  }


  // ========================================================

  /** @deprecated, remove soon */

  std::pair<unsigned int, double> GmresPetscLinearEquationSolver::solve(SparseMatrix&  matrix_in,
      SparseMatrix&  precond_in,  NumericVector& solution_in,  NumericVector& rhs_in,
      const double tol,   const unsigned int m_its) {

//   START_LOG("solve()", "PetscLinearSolverM");
    // Make sure the data passed in are really of Petsc types
    PetscMatrix* matrix   = libmeshM_cast_ptr<PetscMatrix*> (&matrix_in);
    PetscMatrix* precond  = libmeshM_cast_ptr<PetscMatrix*> (&precond_in);
    PetscVector* solution = libmeshM_cast_ptr<PetscVector*> (&solution_in);
    PetscVector* rhs      = libmeshM_cast_ptr<PetscVector*> (&rhs_in);
    this->init(matrix);

    int ierr = 0;
    int its = 0, max_its = static_cast<int>(m_its);
    PetscReal final_resid = 0.;
    // Close the matrices and vectors in case this wasn't already done.
    matrix->close();
    precond->close();
    solution->close();
    rhs->close();

//   // If matrix != precond, then this means we have specified a
//   // special preconditioner, so reset preconditioner type to PCMAT.
//   if (matrix != precond)
//     {
//       this->_preconditioner_type = USER_PRECOND;
//       this->set_petsc_preconditioner_type ();
//     }
    if(this->_preconditioner) this->_preconditioner->set_matrix(matrix_in);

    // 2.2.1 & newer style
    // Set operators. The input matrix works as the preconditioning matrix
    if(!this->same_preconditioner)  {
      //ierr = KSPSetOperators(_ksp, matrix->mat(), precond->mat(),SAME_NONZERO_PATTERN);
      ierr = KSPSetOperators(_ksp, matrix->mat(), precond->mat());    //PETSC3p5
      CHKERRABORT(MPI_COMM_WORLD, ierr);
    } else  {
      //ierr = KSPSetOperators(_ksp, matrix->mat(), precond->mat(),SAME_PRECONDITIONER);
      ierr = KSPSetOperators(_ksp, matrix->mat(), precond->mat());    //PETSC3p5
      CHKERRABORT(MPI_COMM_WORLD, ierr);
    }

    // Set the tolerances for the iterative solver.  Use the user-supplied
    // tolerance for the relative residual & leave the others at default values.
    ierr = KSPSetTolerances(_ksp, tol, PETSC_DEFAULT, PETSC_DEFAULT, max_its);
    CHKERRABORT(MPI_COMM_WORLD, ierr);
    // Solve the linear system

//        PetscLogEvent USER_EVENT;
//      PetscLogDouble user_event_flops;
//      PetscLogEventRegister("User event",0,&USER_EVENT);
//      PetscLogEventBegin(USER_EVENT,0,0,0,0);


    ierr = KSPSolve(_ksp, rhs->vec(), solution->vec());
    CHKERRABORT(MPI_COMM_WORLD, ierr);
//         PetscLogFlops(user_event_flops);
//      PetscLogEventEnd(USER_EVENT,0,0,0,0);

    // Get the number of iterations required for convergence
    ierr = KSPGetIterationNumber(_ksp, &its);
    CHKERRABORT(MPI_COMM_WORLD, ierr);
    // Get the norm of the final residual to return to the user.
    ierr = KSPGetResidualNorm(_ksp, &final_resid);
    CHKERRABORT(MPI_COMM_WORLD, ierr);

//   STOP_LOG("solve()", "PetscLinearSolverM");
    return std::make_pair(its, final_resid);
  }


// @deprecated ========================================================
  PetscErrorCode __libmesh_petsc_preconditioner_setup(PC pc) {
    void* ctx;
    PetscErrorCode ierr = PCShellGetContext(pc, &ctx);
    CHKERRQ(ierr);
    Preconditioner* preconditioner = static_cast<Preconditioner*>(ctx);
    preconditioner->init();
    return 0;
  }


// @deprecated ========================================================
  PetscErrorCode __libmesh_petsc_preconditioner_apply(PC pc, Vec x, Vec y) {
    void* ctx;
    PetscErrorCode ierr = PCShellGetContext(pc, &ctx);
    CHKERRQ(ierr);
    Preconditioner* preconditioner = static_cast<Preconditioner*>(ctx);
    PetscVector x_vec(x);
    PetscVector y_vec(y);
    preconditioner->apply(x_vec, y_vec);
    return 0;
  }


// @deprecated ========================================================
  void GmresPetscLinearEquationSolver::init(SparseMatrix* matrix) {

    PetscMatrix* matrix_two   = libmeshM_cast_ptr<PetscMatrix*> (matrix);

    // Initialize the data structures if not done so already.
    if(!this->initialized())    {
      this->_is_initialized = true;
      int ierr = 0;
// #if PETSC_VERSION_LESS_THAN(2,2,0)  // 2.1.x & earlier style
//     // Create the linear solver context
//     ierr = SLESCreate(MPI_COMM_WORLD, &_sles);
//     CHKERRABORT(MPI_COMM_WORLD,ierr);
//     // Create the Krylov subspace & preconditioner contexts
//     ierr = SLESGetKSP(_sles, &_ksp);
//     CHKERRABORT(MPI_COMM_WORLD,ierr);
//     ierr = SLESGetPC(_sles, &_pc);
//     CHKERRABORT(MPI_COMM_WORLD,ierr);
//     // Have the Krylov subspace method use our good initial guess rather than 0
//     ierr = KSPSetInitialGuessNonzero(_ksp, PETSC_TRUE);
//     CHKERRABORT(MPI_COMM_WORLD,ierr);
//     // Set user-specified  solver and preconditioner types
//     this->set_petsc_solver_type();
//     // Set the options from user-input
//     // Set runtime options, e.g.,
//     //      -ksp_type <type> -pc_type <type> -ksp_monitor -ksp_rtol <rtol>
//     //  These options will override those specified above as long as
//     //  SLESSetFromOptions() is called _after_ any other customization
//     //  routines.
//     ierr = SLESSetFromOptions(_sles);
//     CHKERRABORT(MPI_COMM_WORLD,ierr);
//
// #else // 2.2.0 & newer style
      // Create the linear solver context
      ierr = KSPCreate(MPI_COMM_WORLD, &_ksp);
      CHKERRABORT(MPI_COMM_WORLD, ierr);
      //ierr = PCCreate (MPI_COMM_WORLD, &_pc); CHKERRABORT(MPI_COMM_WORLD,ierr);
      // Create the preconditioner context
      ierr = KSPGetPC(_ksp, &_pc);
      CHKERRABORT(MPI_COMM_WORLD, ierr);
      // Set operators. The input matrix works as the preconditioning matrix
      //ierr = KSPSetOperators(_ksp, matrix->mat(), matrix->mat(),SAME_NONZERO_PATTERN);
      ierr = KSPSetOperators(_ksp, matrix_two->mat(), matrix_two->mat());    //PETSC3p5
      CHKERRABORT(MPI_COMM_WORLD, ierr);
      // Have the Krylov subspace method use our good initial guess rather than 0
      ierr = KSPSetInitialGuessNonzero(_ksp, PETSC_TRUE);
      CHKERRABORT(MPI_COMM_WORLD, ierr);
      // Set user-specified  solver and preconditioner types
      this->SetPetscSolverType(_ksp);
      // Set the options from user-input
      // Set runtime options, e.g., -ksp_type <type> -pc_type <type> -ksp_monitor -ksp_rtol <rtol>
      //  These options will override those specified above as long as
      //  KSPSetFromOptions() is called _after_ any other customization  routines.
      ierr = KSPSetFromOptions(_ksp);
      KSPGMRESSetRestart(_ksp, _restart);
      CHKERRABORT(MPI_COMM_WORLD, ierr);
      // Not sure if this is necessary, or if it is already handled by KSPSetFromOptions?
      //ierr = PCSetFromOptions (_pc);CHKERRABORT(MPI_COMM_WORLD,ierr);

// #endif

      // Notify PETSc of location to store residual history.
      // This needs to be called before any solves, since
      // it sets the residual history length to zero.  The default
      // behavior is for PETSc to allocate (internally) an array
      // of size 1000 to hold the residual norm history.
      ierr = KSPSetResidualHistory(_ksp,
                                   PETSC_NULL,   // pointer to the array which holds the history
                                   PETSC_DECIDE, // size of the array holding the history
                                   PETSC_TRUE);  // Whether or not to reset the history for each solve.
      CHKERRABORT(MPI_COMM_WORLD, ierr);

      PetscPreconditioner::set_petsc_preconditioner_type(this->_preconditioner_type, _pc);

      if(this->_preconditioner) {
        this->_preconditioner->set_matrix(*matrix);
        PCShellSetContext(_pc, (void*) this->_preconditioner);
        PCShellSetSetUp(_pc, __libmesh_petsc_preconditioner_setup);
        PCShellSetApply(_pc, __libmesh_petsc_preconditioner_apply);
      }
    }
  }



} //end namespace femus


#endif

