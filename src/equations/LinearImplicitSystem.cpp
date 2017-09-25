/*=========================================================================

 Program: FEMUS
 Module: LinearImplicitSystem
 Authors: Simone Bnà

 Copyright (c) FEMTTU
 All rights reserved.

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "LinearImplicitSystem.hpp"
#include "LinearEquationSolver.hpp"
#include "SparseMatrix.hpp"
#include "NumericVector.hpp"
#include "ElemType.hpp"
#include <iomanip>

namespace femus {

  // ********************************************

  LinearImplicitSystem::LinearImplicitSystem( MultiLevelProblem& ml_probl,
      const std::string& name_in,
      const unsigned int number_in, const MgSmoother& smoother_type ) :
    ImplicitSystem( ml_probl, name_in, number_in, smoother_type ),
    _n_max_linear_iterations( 3 ),
    _final_linear_residual( 1.e20 ),
    _linearAbsoluteConvergenceTolerance( 1.e-08 ),
    _mg_type( F_CYCLE ),
    _npre( 1 ),
    _npost( 1 ),
    _AMRtest( 0 ),
    _maxAMRlevels( 0 ),
    _AMRnorm( 0 ),
    _AMRthreshold( 0.01 ),
    _SmootherType( smoother_type ),
    _MGmatrixFineReuse( false ),
    _MGmatrixCoarseReuse( false ),
    _printSolverInfo( false ),
    _assembleMatrix( true ),
    _factorTest( false ),
    _scale( 1.0 )
  {
    _SparsityPattern.resize( 0 );
    _outer_ksp_solver = "gmres";
  }

  // ********************************************

  LinearImplicitSystem::~LinearImplicitSystem() {
    this->clear();
  }

  // ********************************************

  void LinearImplicitSystem::clear() {
    for( unsigned ig = 0; ig < _LinSolver.size(); ig++ ) {
      _LinSolver[ig]->DeletePde();
      delete _LinSolver[ig];

      if( _PP[ig] ) delete _PP[ig];

      if( _RR[ig] ) delete _RR[ig];
    }

    _NSchurVar_test = 0;
    _numblock_test = 0;
    _numblock_all_test = 0;
  }

  // ********************************************

  void LinearImplicitSystem::SetSparsityPattern( vector< bool > other_sparcity_pattern ) {
    unsigned SolPdeSize2 = _SolSystemPdeIndex.size() * _SolSystemPdeIndex.size();

    if( other_sparcity_pattern.size() != SolPdeSize2 ) {
      std::cout << "Error! Sparsity Pattern size ( " << other_sparcity_pattern.size() << " ) does not match system PDE size" << std::endl;
      exit( 0 );
    }

    _SparsityPattern.resize( SolPdeSize2 );

    for( int i = 0; i < SolPdeSize2; i++ ) _SparsityPattern[i] = other_sparcity_pattern[i];
  }

  // ********************************************

  void LinearImplicitSystem::init() {

    _LinSolver.resize( _gridn );

    _LinSolver[0] = LinearEquationSolver::build( 0, _solution[0], GMRES_SMOOTHER ).release();

    for( unsigned i = 1; i < _gridn; i++ ) {
      _LinSolver[i] = LinearEquationSolver::build( i, _solution[i], _SmootherType ).release();
    }

    for( unsigned i = 0; i < _gridn; i++ ) {
      _LinSolver[i]->InitPde( _SolSystemPdeIndex, _ml_sol->GetSolType(),
                              _ml_sol->GetSolName(), &_solution[i]->_Bdc, _gridr, _gridn, _SparsityPattern );
    }

    _PP.resize( _gridn );
    _RR.resize( _gridn );

    for( unsigned i = 0; i < _gridn; i++ ) {
      _PP[i] = NULL;
      _RR[i] = NULL;
    }

    for( unsigned ig = 1; ig < _gridn; ig++ ) {
      BuildProlongatorMatrix( ig );
    }

    _NSchurVar_test = 0;
    _numblock_test = 0;
    _numblock_all_test = 0;
    // By default we solve for all the PDE variables
    ClearVariablesToBeSolved();
    AddVariableToBeSolved( "All" );
  }

  // ********************************************

  void LinearImplicitSystem::solve( const MgSmootherType& mgSmootherType ) {

    clock_t start_mg_time = clock();

    unsigned grid0;

    if( _mg_type == F_CYCLE ) {
      std::cout << std::endl << " *** Start " << _solverType << " Linear Full-Cycle ***" << std::endl;
      grid0 = 1;
    }
    else if( _mg_type == V_CYCLE ) {
      std::cout << std::endl << " *** Start " << _solverType << " Linear V-Cycle ***" << std::endl;
      grid0 = _gridn;
    }
    else if( _mg_type == M_CYCLE ) {
      std::cout << std::endl << " *** Start " << _solverType << " Linear Mixed-Cycle ***" << std::endl;
      grid0 = _gridr;
    }
    else {
      std::cout << "wrong " << _solverType << " type for this solver " << std::endl;
      abort();
    }
    
    std::cout << "COARSE GRID = "<<grid0<<std::endl;
    std::cout << "FINE GRID = "<<_gridn<<std::endl;

    unsigned AMRCounter = 0;

    for( unsigned igridn = grid0; igridn <= _gridn; igridn++ ) {   //_igridn
      std::cout << std::endl << " ****** Start Level Max " << igridn << " ******" << std::endl;


      bool ThisIsAMR = ( _mg_type == F_CYCLE && _AMRtest &&  AMRCounter < _maxAMRlevels && igridn == _gridn ) ? 1 : 0;

      if( ThisIsAMR ) _solution[igridn - 1]->InitAMREps();

      clock_t start_assembly_time = clock();

      _levelToAssemble = igridn - 1u; //Be carefull!!!! this is needed in the _assemble_function
      _LinSolver[igridn - 1u]->SetResZero();
      _assembleMatrix = true;
      _assemble_system_function( _equation_systems );

      _MGmatrixFineReuse = false;
      _MGmatrixCoarseReuse = ( igridn - grid0 > 0 ) ?  true : _MGmatrixFineReuse;
      for( unsigned i = igridn - 1u; i > 0; i-- ) {
        if( _RR[i] ) {
          if( i == igridn - 1u )
            _LinSolver[i - 1u]->_KK->matrix_ABC( *_RR[i], *_LinSolver[i]->_KK, *_PP[i], _MGmatrixFineReuse );
          else
            _LinSolver[i - 1u]->_KK->matrix_ABC( *_RR[i], *_LinSolver[i]->_KK, *_PP[i], _MGmatrixCoarseReuse );
        }
        else {
          if( i == igridn - 1u )
            _LinSolver[i - 1u]->_KK->matrix_PtAP( *_PP[i], *_LinSolver[i]->_KK, _MGmatrixFineReuse );
          else
            _LinSolver[i - 1u]->_KK->matrix_PtAP( *_PP[i], *_LinSolver[i]->_KK, _MGmatrixCoarseReuse );
        }
      }

      std::cout << std::endl << " ****** Level Max " << igridn << " ASSEMBLY TIME:\t" << static_cast<double>( ( clock() - start_assembly_time ) ) / CLOCKS_PER_SEC << std::endl;

      if( _MGsolver ) {
        _LinSolver[igridn - 1u]->MGInit( mgSmootherType, igridn, _outer_ksp_solver.c_str() );

        for( unsigned i = 0; i < igridn; i++ ) {
          if( _RR[i] )
            _LinSolver[i]->MGSetLevel( _LinSolver[igridn - 1u], igridn - 1u, _VariablesToBeSolvedIndex, _PP[i], _RR[i], _npre, _npost );
          else
            _LinSolver[i]->MGSetLevel( _LinSolver[igridn - 1u], igridn - 1u, _VariablesToBeSolvedIndex, _PP[i], _PP[i], _npre, _npost );
        }

        MGVcycle( igridn, mgSmootherType );

        _LinSolver[igridn - 1u]->MGClear();
      }
      else MLVcycle( igridn );

      if( ThisIsAMR ) AddAMRLevel( AMRCounter );

      if( igridn < _gridn ) ProlongatorSol( igridn );

      std::cout << std::endl << " ****** End Level Max " << igridn << " ******" << std::endl;
    }

    std::cout << std::endl << " *** Linear " << _solverType << " TIME: " << std::setw( 11 ) << std::setprecision( 6 ) << std::fixed
              << static_cast<double>( ( clock() - start_mg_time ) ) / CLOCKS_PER_SEC << std::endl;
  }

  // ********************************************

  bool LinearImplicitSystem::IsLinearConverged( const unsigned igridn ) {

    bool conv = true;
    double L2normRes;
//     std::cout << std::endl;

    for( unsigned k = 0; k < _SolSystemPdeIndex.size(); k++ ) {
      unsigned indexSol = _SolSystemPdeIndex[k];
      L2normRes       = _solution[igridn]->_Res[indexSol]->l2_norm();
      std::cout << "       *************** Level Max " << igridn + 1 << "  Linear Res  L2norm " << std::scientific << _ml_sol->GetSolutionName( indexSol ) << " = " << L2normRes << std::endl;

      if( L2normRes < _linearAbsoluteConvergenceTolerance && conv == true ) {
        conv = true;
      }
      else {
        conv = false;
      }
    }

    return conv;

  }

  // *******************************************************

  void LinearImplicitSystem::ProlongatorSol( unsigned gridf ) {

    for( unsigned k = 0; k < _SolSystemPdeIndex.size(); k++ ) {

      unsigned SolIndex = _SolSystemPdeIndex[k];
      unsigned solType = _ml_sol->GetSolutionType( SolIndex );

      _solution[gridf]->_Sol[SolIndex]->matrix_mult( *_solution[gridf - 1]->_Sol[SolIndex],
          *_msh[gridf]->GetCoarseToFineProjection( solType ) );
      _solution[gridf]->_Sol[SolIndex]->close();
    }
  }

  // ********************************************

  bool LinearImplicitSystem::MGVcycle( const unsigned& gridn, const MgSmootherType& mgSmootherType ) {

    clock_t start_mg_time = clock();

    _LinSolver[gridn - 1u]->SetEpsZero();

    bool linearIsConverged;

    for( unsigned linearIterator = 0; linearIterator < _n_max_linear_iterations; linearIterator++ ) { //linear cycle

      std::cout << "       *************** Linear iteration " << linearIterator + 1 << " ***********" << std::endl;
      bool ksp_clean = !linearIterator * _assembleMatrix;
      _LinSolver[gridn - 1u]->MGSolve( ksp_clean );
      _solution[gridn - 1u]->UpdateRes( _SolSystemPdeIndex, _LinSolver[gridn - 1u]->_RES, _LinSolver[gridn - 1u]->KKoffset );
      linearIsConverged = IsLinearConverged( gridn - 1u );

      if( linearIsConverged )  break;
    }

    _solution[gridn - 1u]->UpdateSol( _SolSystemPdeIndex, _LinSolver[gridn - 1u]->_EPS, _LinSolver[gridn - 1u]->KKoffset );

    std::cout << "       *************** Linear-Cycle TIME:\t" << std::setw( 11 ) << std::setprecision( 6 ) << std::fixed
              << static_cast<double>( ( clock() - start_mg_time ) ) / CLOCKS_PER_SEC << std::endl;
    return linearIsConverged;
  }

  // ********************************************

  bool LinearImplicitSystem::MLVcycle( const unsigned& gridn ) {

    clock_t start_mg_time = clock();

    
    _LinSolver[gridn - 1u]->SetEpsZero();

    bool linearIsConverged;

    for( unsigned linearIterator = 0; linearIterator < _n_max_linear_iterations; linearIterator++ ) { //linear cycle

      std::cout << std::endl << " *************** Linear iteration " << linearIterator + 1 << " ***********" << std::endl;

      bool ksp_clean = !linearIterator * _assembleMatrix;

      for( unsigned ig = gridn - 1u; ig > 0; ig-- ) {
	
	
	unsigned factor = 1;
	//BEGIN da commentare
	if(_factorTest){
	  if(_gridr == 1){
	    if(ig >= _gridr ){
	      factor = (1 + ig - _gridr );
	    }
	  }
	  else{
	    if (ig >= _gridr ){
	      factor = (2 + ig - _gridr); //da controllare qnd _gridr lo mettiamo >1
	    }
	  }
	  factor *= factor;
	  factor = factor + 1;
	}
	//END da commentare
	//std::cout<< "ig = "<< ig << " factor =  " <<factor<<std::endl;
	
	std::cout << "pre-smoothing level = " << ig << " 1 + level^2 = " << factor <<std::endl;
	
        // ============== Presmoothing ==============
        for( unsigned k = 0; k < _npre * factor; k++ ) {
	  //_LinSolver[ig]->Solve( _VariablesToBeSolvedIndex, ksp_clean * ( !k ) );
	  Solve(ig + 1, ksp_clean * ( !k ), 1, 1);
        }

        // ============== Restriction ==============
        Restrictor( ig );
      }

      // ============== Direct Solver ==============
      _LinSolver[0]->Solve( _VariablesToBeSolvedIndex, ksp_clean );

      for( unsigned ig = 1; ig < gridn; ig++ ) {
        // ============== Standard Prolongation ==============
        Prolongator( ig );

	unsigned factor = 1;
	//BEGIN da commentare
	if(_factorTest){
	  if(_gridr == 1){
	    if(ig >= _gridr ){
	      factor = (1 + ig - _gridr );
	    }
	  }
	  else{
	    if (ig >= _gridr ){
	      factor = (2 + ig - _gridr);
	    }
	  }
	  factor *= factor;
	  factor = factor + 1;
	}
	//END da commentare
    	//std::cout<< "ig = "<< ig << " factor =  " <<factor<<std::endl;
	
	std::cout << "post-smoothing level = " << ig << " 1 + level^2 = " << factor <<std::endl;
        // ============== PostSmoothing ==============
        for( unsigned k = 0; k < _npost * factor; k++ ) {
          //_LinSolver[ig]->Solve( _VariablesToBeSolvedIndex, ksp_clean * ( !_npre ) * ( !k ) );
	  Solve(ig + 1, ksp_clean * ( !(_npre * factor) ) * ( !k ), 1, 1);
        }
      }

      // ============== Update AMR Solution and Residual ( _gridr-1 <= ig <= gridn-2 ) ==============
      for( unsigned ig = _gridr - 1; ig < gridn - 1; ig++ ) { // _gridr
        _solution[ig]->UpdateSolAndRes( _SolSystemPdeIndex, _LinSolver[ig]->_EPS, _LinSolver[ig]->_RES, _LinSolver[ig]->KKoffset );
      }

      // ============== Update Fine Residual ==============
      _solution[gridn - 1]->UpdateRes( _SolSystemPdeIndex, _LinSolver[gridn - 1]->_RES, _LinSolver[gridn - 1]->KKoffset );
      linearIsConverged = IsLinearConverged( gridn - 1 );

      if( linearIsConverged ) break;

    }

    // ============== Update Fine Solution ==============
    _solution[gridn - 1]->UpdateSol( _SolSystemPdeIndex, _LinSolver[gridn - 1]->_EPS, _LinSolver[gridn - 1]->KKoffset );


    std::cout << "\n ************ Linear-Cycle TIME:\t" << std::setw( 11 ) << std::setprecision( 6 ) << std::fixed
              << static_cast<double>( ( clock() - start_mg_time ) ) / CLOCKS_PER_SEC << std::endl;

    return linearIsConverged;
  }
  
  
  void LinearImplicitSystem::Solve( const unsigned& gridn, const bool &kspClean, const int &npre, const int &npost ) {
    
    unsigned grid0 = ( gridn <= _gridr ) ? gridn : _gridr;
    
    for( unsigned ig = gridn - 1u; ig > grid0 - 1u; ig-- ) {
      
      if(_factorTest) 
	_LinSolver[ig]->SetScale(_scale/(gridn-1u));
      else
	_LinSolver[ig]->SetScale(_scale);
      
      for( unsigned k = 0; k < npre; k++ ) {
	_LinSolver[ig]->Solve( _VariablesToBeSolvedIndex, kspClean );
      }
      Restrictor( ig );
    }
      
    _LinSolver[grid0 - 1u]->Solve( _VariablesToBeSolvedIndex, kspClean );

    for( unsigned ig = grid0; ig < gridn; ig++ ) {
      Prolongator( ig );
      for( unsigned k = 0; k < npost; k++ ) {
	_LinSolver[ig]->Solve( _VariablesToBeSolvedIndex, kspClean );
      }
    }
  }
  // ********************************************

  void LinearImplicitSystem::Restrictor( const unsigned& gridf ) {

    _LinSolver[gridf - 1u]->SetEpsZero();
    _LinSolver[gridf - 1u]->SetResZero();

    if( _RR[gridf] ) {
      _LinSolver[gridf - 1u]->_RESC->matrix_mult( *_LinSolver[gridf]->_RES, *_RR[gridf] );
    }
    else {
      _LinSolver[gridf - 1u]->_RESC->matrix_mult_transpose( *_LinSolver[gridf]->_RES, *_PP[gridf] );
    }

    *_LinSolver[gridf - 1u]->_RES += *_LinSolver[gridf - 1u]->_RESC;

  }

  // *******************************************************

  void LinearImplicitSystem::Prolongator( const unsigned& gridf ) {

    _LinSolver[gridf]->_EPSC->matrix_mult( *_LinSolver[gridf - 1]->_EPS, *_PP[gridf] );
    _LinSolver[gridf]->UpdateResidual();
    _LinSolver[gridf]->SumEpsCToEps();

  }

  // ********************************************

  void LinearImplicitSystem::AddAMRLevel( unsigned& AMRCounter ) {
    bool conv_test = 0;

    conv_test = _solution[_gridn - 1]->FlagAMRRegionBasedOnErroNormAdaptive( _SolSystemPdeIndex, _AMRthreshold, _AMRnorm );
    
    //     if( _AMRnorm == 0 ) {
//       conv_test = _solution[_gridn - 1]->FlagAMRRegionBasedOnl2( _SolSystemPdeIndex, _AMRthreshold );
//     }
//     else if( _AMRnorm == 1 ) {
//       conv_test = _solution[_gridn - 1]->FlagAMRRegionBasedOnSemiNorm( _SolSystemPdeIndex, _AMRthreshold );
//     }

    if( conv_test == 0 ) {
      _ml_msh->AddAMRMeshLevel();
      _ml_sol->AddSolutionLevel();
      AddSystemLevel();
      AMRCounter++;
    }
    else {
      _maxAMRlevels = AMRCounter;
      std::cout << "The AMR solver has converged after " << AMRCounter << " refinements\n";
    }
  }

  // ********************************************

  void LinearImplicitSystem::AddSystemLevel() {

    _equation_systems.AddLevel();

    _msh.resize( _gridn + 1 );
    _solution.resize( _gridn + 1 );
    _msh[_gridn] = _equation_systems._ml_msh->GetLevel( _gridn );
    _solution[_gridn] = _ml_sol->GetSolutionLevel( _gridn );

    for( int i = 0; i < _gridn; i++ ) {
      _LinSolver[i]->AddLevel();
    }

    _LinSolver.resize( _gridn + 1 );

    _LinSolver[_gridn] = LinearEquationSolver::build( _gridn, _solution[_gridn], _SmootherType ).release();

    _LinSolver[_gridn]->InitPde( _SolSystemPdeIndex, _ml_sol->GetSolType(),
                                 _ml_sol->GetSolName(), &_solution[_gridn]->_Bdc, _gridr, _gridn + 1, _SparsityPattern );

    _PP.resize( _gridn + 1 );
    _RR.resize( _gridn + 1 );
    BuildProlongatorMatrix( _gridn );

    _LinSolver[_gridn]->set_solver_type( _finegridsolvertype );
    _LinSolver[_gridn]->SetTolerances( _rtol, _atol, _divtol, _maxits, _restart );
    _LinSolver[_gridn]->set_preconditioner_type( _finegridpreconditioner );
    _LinSolver[_gridn]->PrintSolverInfo( _printSolverInfo );

    if( _numblock_test ) {
      unsigned num_block2 = std::min( _num_block, _msh[_gridn]->GetNumberOfElements() );
      _LinSolver[_gridn]->SetElementBlockNumber( num_block2 );
    }
    else if( _numblock_all_test ) {
      _LinSolver[_gridn]->SetElementBlockNumber( "All", _overlap );
    }

    if( _NSchurVar_test ) {
      _LinSolver[_gridn]->SetNumberOfSchurVariables( _NSchurVar );
    }

    _gridn++;
  }

  // ********************************************


//---------------------------------------------------------------------------------------------
// This is function sets the AMR options
//---------------------------------------------------------------------------------------------

  void LinearImplicitSystem::SetAMRSetOptions( const std::string& AMR, const unsigned& AMRlevels,
      const std::string& AMRnorm, const double& AMRthreshold,
      bool ( * SetRefinementFlag )( const std::vector < double >& x,
                                    const int& ElemGroupNumber, const int& level ) ) {
    if( !strcmp( "yes", AMR.c_str() ) || !strcmp( "YES", AMR.c_str() ) || !strcmp( "Yes", AMR.c_str() ) ) {
      _AMRtest = 1;
    }

    _maxAMRlevels = AMRlevels;
    _AMRthreshold.resize(1);
    _AMRthreshold[0] = AMRthreshold;

    if( !strcmp( "l2", AMRnorm.c_str() )  || !strcmp( "h0", AMRnorm.c_str() ) || !strcmp( "H0", AMRnorm.c_str() )) {
      _AMRnorm = 0;
    }
    else if( !strcmp( "H1", AMRnorm.c_str() ) || !strcmp( "h1", AMRnorm.c_str() ) ) {
      _AMRnorm = 1;
    }
    else {
      std::cout << AMRnorm << " invalid AMRnorm type \n set to default H1 norm" << std::endl;
      _AMRnorm = 1;
    }

    if( SetRefinementFlag == NULL ) {
    }
    else {
      _msh[0]->Mesh::_SetRefinementFlag = SetRefinementFlag;
      _msh[0]->Mesh::_IsUserRefinementFunctionDefined = true;
    }
  }


//---------------------------------------------------------------------------------------------------
// This routine generates the matrix for the projection of the FE matrix to finer grids.
// This is a virtual function overloaded in the class MonolithicFSINonLinearImplicitSystem.
//---------------------------------------------------------------------------------------------------

  void LinearImplicitSystem::BuildProlongatorMatrix( unsigned gridf ) {

    if( gridf < 1 ) {
      std::cout << "Error! In function \"BuildProlongatorMatrix\" argument less then 1" << std::endl;
      exit( 0 );
    }

    int iproc;
    MPI_Comm_rank( MPI_COMM_WORLD, &iproc );

    LinearEquationSolver* LinSolf = _LinSolver[gridf];
    LinearEquationSolver* LinSolc = _LinSolver[gridf - 1];
    Mesh* mshc = _msh[gridf - 1];
    int nf = LinSolf->KKIndex[LinSolf->KKIndex.size() - 1u];
    int nc = LinSolc->KKIndex[LinSolc->KKIndex.size() - 1u];
    int nf_loc = LinSolf->KKoffset[LinSolf->KKIndex.size() - 1][iproc] - LinSolf->KKoffset[0][iproc];
    int nc_loc = LinSolc->KKoffset[LinSolc->KKIndex.size() - 1][iproc] - LinSolc->KKoffset[0][iproc];

    NumericVector* NNZ_d = NumericVector::build().release();
    NNZ_d->init( *LinSolf->_EPS );
    NNZ_d->zero();

    NumericVector* NNZ_o = NumericVector::build().release();
    NNZ_o->init( *LinSolf->_EPS );
    NNZ_o->zero();

    for( unsigned k = 0; k < _SolSystemPdeIndex.size(); k++ ) {
      unsigned SolIndex = _SolSystemPdeIndex[k];
      unsigned  SolType = _ml_sol->GetSolutionType( SolIndex );

      // loop on the coarse grid
      for( int isdom = iproc; isdom < iproc + 1; isdom++ ) {
        for( int iel = mshc->_elementOffset[isdom]; iel < mshc->_elementOffset[isdom + 1]; iel++ ) {
          short unsigned ielt = mshc->GetElementType( iel );
          mshc->_finiteElement[ielt][SolType]->GetSparsityPatternSize( *LinSolf, *LinSolc, iel, NNZ_d, NNZ_o, SolIndex, k );
        }
      }
    }

    NNZ_d->close();
    NNZ_o->close();

    unsigned offset = LinSolf->KKoffset[0][iproc];
    vector <int> nnz_d( nf_loc );
    vector <int> nnz_o( nf_loc );

    for( int i = 0; i < nf_loc; i++ ) {
      nnz_d[i] = static_cast <int>( ( *NNZ_d )( offset + i ) );
      nnz_o[i] = static_cast <int>( ( *NNZ_o )( offset + i ) );
    }

    delete NNZ_d;
    delete NNZ_o;

    _PP[gridf] = SparseMatrix::build().release();
    _PP[gridf]->init( nf, nc, nf_loc, nc_loc, nnz_d, nnz_o );

    for( unsigned k = 0; k < _SolSystemPdeIndex.size(); k++ ) {
      unsigned SolIndex = _SolSystemPdeIndex[k];
      unsigned  SolType = _ml_sol->GetSolutionType( SolIndex );

      // loop on the coarse grid
      for( int isdom = iproc; isdom < iproc + 1; isdom++ ) {
        for( int iel = mshc->_elementOffset[isdom]; iel < mshc->_elementOffset[isdom + 1]; iel++ ) {
          short unsigned ielt = mshc->GetElementType( iel );
          mshc->_finiteElement[ielt][SolType]->BuildProlongation( *LinSolf, *LinSolc, iel, _PP[gridf], SolIndex, k );
        }
      }
    }

    _PP[gridf]->close();
  }

  // ********************************************

  void LinearImplicitSystem::SetDirichletBCsHandling( const DirichletBCType DirichletMode ) {

    if( DirichletMode == PENALTY ) {
      _DirichletBCsHandlingMode = 0;
    }
    else { // elimination
      _DirichletBCsHandlingMode = 1;
    }
  }

  // ********************************************

  void LinearImplicitSystem::SetSamePreconditioner(){
    for( unsigned i = 0; i < _gridn; i++ ) {
      _LinSolver[i]->SetSamePreconditioner();
    }
  }

  // ********************************************

  void LinearImplicitSystem::AddVariableToBeSolved( const char solname[] ) {

    if( !strcmp( solname, "All" ) || !strcmp( solname, "ALL" ) || !strcmp( solname, "all" ) ) {
      _VariablesToBeSolvedIndex.resize( _SolSystemPdeIndex.size() );

      for( unsigned i = 0; i < _SolSystemPdeIndex.size(); i++ ) {
        _VariablesToBeSolvedIndex[i] = i;
      }
    }
    else {
      unsigned n = _VariablesToBeSolvedIndex.size();
      _VariablesToBeSolvedIndex.resize( n + 1u );
      unsigned varind = _ml_sol->GetIndex( solname );

      for( unsigned i = 0; i < _SolSystemPdeIndex.size(); i++ ) {
        if( _SolSystemPdeIndex[i] == varind ) {
          _VariablesToBeSolvedIndex[n] = i;
          break;
        }

        if( _SolSystemPdeIndex.size() - 1u == i ) {
          std::cout << "Error! The variable " << solname << " cannot be added to AddVariableToBeSolved"
                    << " Index because it is not included in the solution variable set." << std::endl;
          std::exit( 0 );
        }
      }
    }
  }

  // ********************************************

  void LinearImplicitSystem::ClearVariablesToBeSolved() {
    _VariablesToBeSolvedIndex.clear();
  }

  // ********************************************

  void LinearImplicitSystem::SetMgSmoother( const MgSmoother mgsmoother ) {
    _SmootherType = mgsmoother;
  }


  void LinearImplicitSystem::PrintSolverInfo(const bool & printInfo){

    _printSolverInfo = printInfo;

    for( unsigned i = 0; i < _gridn; i++ ) {
      _LinSolver[i]->PrintSolverInfo( _printSolverInfo );
    }
  }


  // ********************************************

  void LinearImplicitSystem::SetElementBlockNumber( unsigned const& dim_block ) {
    _numblock_test = 1;
    const unsigned dim = _msh[0]->GetDimension();
    const unsigned base = pow( 2, dim );
    _num_block = pow( base, dim_block );

    for( unsigned i = 1; i < _gridn; i++ ) {
      unsigned num_block2 = std::min( _num_block, _msh[i]->GetNumberOfElements() );
      _LinSolver[i]->SetElementBlockNumber( num_block2 );
    }
  }

  // ********************************************

  void LinearImplicitSystem::SetElementBlockNumber( const char all[], const unsigned& overlap ) {
    _numblock_all_test = 1;
    _overlap = overlap;

    for( unsigned i = 1; i < _gridn; i++ ) {
      _LinSolver[i]->SetElementBlockNumber( all, overlap );
    }
  }

  // ********************************************

  void LinearImplicitSystem::SetSolverFineGrids( const SolverType finegridsolvertype ) {
    _finegridsolvertype = finegridsolvertype;

    for( unsigned i = 1; i < _gridn; i++ ) {
      _LinSolver[i]->set_solver_type( _finegridsolvertype );
    }
  }

  // ********************************************

  void LinearImplicitSystem::SetPreconditionerFineGrids( const PreconditionerType finegridpreconditioner ) {
    _finegridpreconditioner = finegridpreconditioner;

    for( unsigned i = 1; i < _gridn; i++ ) {
      _LinSolver[i]->set_preconditioner_type( _finegridpreconditioner );
    }
  }

  // ********************************************

  void LinearImplicitSystem::SetTolerances( const double &rtol, const double &atol,
      const double &divtol, const unsigned &maxits, const unsigned &restart ) {
    _rtol = rtol;
    _atol = atol;
    _divtol = divtol;
    _maxits = maxits;
    _restart = restart;

    for( unsigned i = 0; i < _gridn; i++ ) {
      _LinSolver[i]->SetTolerances( _rtol, _atol, _divtol, _maxits, _restart );
    }
  }

  // ********************************************

  void LinearImplicitSystem::SetNumberOfSchurVariables( const unsigned short& NSchurVar ) {
    _NSchurVar_test = 1;
    _NSchurVar = NSchurVar;

    for( unsigned i = 1; i < _gridn; i++ ) {
      _LinSolver[i]->SetNumberOfSchurVariables( _NSchurVar );
    }
  }

  // ********************************************

  void LinearImplicitSystem::SetFieldSplitTree( FieldSplitTree *fieldSplitTree ) {
    for( unsigned i = 1; i < _gridn; i++ ) {
      _LinSolver[i]->SetFieldSplitTree( fieldSplitTree );
    }
  };


  /// @deprecated
// this function is like init but it doesn't call InitPDE
  void LinearImplicitSystem::init_two() {

    _LinSolver.resize( _gridn );

    _LinSolver[0] = LinearEquationSolver::build( 0, _solution[0], GMRES_SMOOTHER ).release();

    for( unsigned i = 1; i < _gridn; i++ ) {
      _LinSolver[i] = LinearEquationSolver::build( i, _solution[i], _SmootherType ).release();
    }

//     for (unsigned i=0; i<_gridn; i++) {
//       _LinSolver[i]->InitPde(_SolSystemPdeIndex,_ml_sol->GetSolType(),
//                           _ml_sol->GetSolName(),&_solution[i]->_Bdc,_gridr,_gridn,_SparsityPattern);
//     }


    _PP.resize( _gridn );
    _RR.resize( _gridn );

    for( unsigned i = 0; i < _gridn; i++ ) {
      _PP[i] = NULL;
      _RR[i] = NULL;
    }

//
//     for (unsigned ig=1; ig<_gridn; ig++) {
//       BuildProlongatorMatrix(ig);
//     }

    _NSchurVar_test = 0;
    _numblock_test = 0;
    _numblock_all_test = 0;
    // By default we solved for all the PDE variables
    ClearVariablesToBeSolved();
    AddVariableToBeSolved( "All" );
  }







// =================================================================
//   std::vector<NumericVector *> _b;   //// LinearEquation (each level)   _RESC
//   std::vector<NumericVector *> _res; //// LinearEquation (each level)   _RES
//   std::vector<NumericVector *> _x;   //// LinearEquation (each level)   _EPS
//   std::vector<NumericVector *> _x_old; //// LinearEquation (each level)  _EPSC

//if the residual norm is small enough,exit the cycle, and so also the MGSolve
//this is also the check for the single level solver
//what is the meaning of having multiple cycles for single-grid solver?
//they are all like just a single linear solver loop, where convergence has already been reached,
// and you do another check after the previous one in the linear solver loop
//the big question is:
///@todo why dont you do "res_fine < Eps1"
// instead of  "res_fine < Eps1*(1.+ bNorm_fine)" ???
//because one is for the absolute error and another one is for the relative error
/// This function solves the discrete problem with multigrid solver
  void LinearImplicitSystem::MGSolve( double Eps1,         // tolerance for the linear solver
                                      int MaxIter,           // n iterations
                                      const uint Gamma,     // Control V W cycle
                                      const uint Nc_pre,    // n pre-smoothing cycles
                                      const uint Nc_coarse, // n coarse cycles
                                      const uint Nc_post    // n post-smoothing cycles
                                    ) {

#ifdef DEFAULT_PRINT_INFO
    std::cout << "######### BEGIN MG SOLVE ########" << std::endl;
#endif
    double res_fine;

    _LinSolver[GetGridn() - 1]->_RESC->close();
    double bNorm_fine =     _LinSolver[GetGridn() - 1]->_RESC->l2_norm();
    _LinSolver[GetGridn() - 1]->_EPSC->close();
    double x_old_fine = _LinSolver[GetGridn() - 1]->_EPSC->l2_norm();

#ifdef DEFAULT_PRINT_INFO
    std::cout << " bNorm_fine l2 "     <<  bNorm_fine                     << std::endl;
    std::cout << " bNorm_fine linfty " << _LinSolver[GetGridn() - 1]->_RESC->linfty_norm()  << std::endl;
    std::cout << " xold_fine l2 "      <<  x_old_fine                     << std::endl;
#endif

    // FAS Multigrid (Nested) ---------
    bool NestedMG = false;

    if( NestedMG ) {
      _LinSolver[0]->_EPS->zero();
      MGStep( 0, 1.e-20, MaxIter, Gamma, Nc_pre, Nc_coarse, Nc_post );

      //smooth on the coarse level WITH PHYSICAL b !
      //and compute the residual

      for( uint Level = 1; Level < GetGridn(); Level++ ) {

        _LinSolver[Level]->_EPS->matrix_mult( *_LinSolver[Level - 1]->_EPS, *_PP[Level] ); //**** project the solution

        res_fine = MGStep( Level, Eps1, MaxIter, Gamma, Nc_pre, Nc_coarse, Nc_post );

      }
    } // NestedMG

    // V or W cycle
    int cycle = 0;
    bool exit_mg = false;

    while( !exit_mg && cycle < MaxIter ) {

///std::cout << "@@@@@@@@@@ BEGIN MG CYCLE @@@@@@@@"<< std::endl;

///std::cout << "@@@@@@@@@@ start on the finest level @@@@@@@@"<< std::endl;

      res_fine = MGStep( GetGridn() - 1, Eps1, MaxIter, Gamma, Nc_pre, Nc_coarse, Nc_post );

///std::cout << "@@@@@@@@@@ back to the finest level @@@@@@@@"<< std::endl;

///std::cout << "@@@@@@@@@@ END MG CYCLE @@@@@@@@"<< std::endl;

      std::cout << "@@@@@@@@@@ CHECK THE RESIDUAL NORM OF THE FINEST LEVEL @@@@@@@@" << std::endl;

      std::cout << "res_fine: " << res_fine << std::endl;
      std::cout << "bNorm_fine: " << bNorm_fine << std::endl;

      if( res_fine < Eps1 * ( 1. + bNorm_fine ) ) exit_mg = true;

      cycle++;

#ifdef DEFAULT_PRINT_INFO
      std::cout << " cycle= " << cycle   << " residual= " << res_fine << " \n";
#endif

    }

#ifdef DEFAULT_PRINT_INFO
    std::cout << "######### END MG SOLVE #######" << std::endl;
#endif
    return;
  }

// ====================================================================
/// This function does one multigrid step
// solve Ax=b
// compute the residual res=b- Ax
// restrict the residual R*res
//notice that the A and b for the POST-smoothing are the same
//as for the pre-smoothing


  double LinearImplicitSystem::MGStep( int Level,           // Level
                                       double Eps1,          // Tolerance
                                       int MaxIter,          // n iterations - number of mg cycles
                                       const uint Gamma,     // Control V W cycle
                                       const uint Nc_pre,    // n pre-smoothing smoother iterations
                                       const uint Nc_coarse, // n coarse smoother iterations
                                       const uint Nc_post    // n post-smoothing smoother iterations
                                     ) {


    std::pair<uint, double> rest;

    if( Level == 0 ) {
///  std::cout << "************ REACHED THE BOTTOM *****************"<< std::endl;

#ifdef DEFAULT_PRINT_CONV
      _LinSolver[Level]->_EPS->close();
      double xNorm0 = _LinSolver[Level]->_EPS->linfty_norm();
      _LinSolver[Level]->_RESC->close();
      double bNorm0 = _LinSolver[Level]->_RESC->linfty_norm();
      _LinSolver[Level]->_KK->close();
      double ANorm0 = _LinSolver[Level]->_KK->l1_norm();
      std::cout << "Level " << Level << " ANorm l1 " << ANorm0 << " bNorm linfty " << bNorm0  << " xNormINITIAL linfty " << xNorm0 << std::endl;
#endif

      rest = _LinSolver[Level]->solve( *_LinSolver[Level]->_KK, *_LinSolver[Level]->_KK, *_LinSolver[Level]->_EPS, *_LinSolver[Level]->_RESC, DEFAULT_EPS_LSOLV_C, Nc_coarse ); //****** smooth on the coarsest level

#ifdef DEFAULT_PRINT_CONV
      std::cout << " Coarse sol : res-norm: " << rest.second << " n-its: " << rest.first << std::endl;
      _LinSolver[Level]->_EPS->close();
      std::cout << " Norm of x after the coarse solution " << _LinSolver[Level]->_EPS->linfty_norm() << std::endl;
#endif

      _LinSolver[Level]->_RES->resid( *_LinSolver[Level]->_RESC, *_LinSolver[Level]->_EPS, *_LinSolver[Level]->_KK );  //************ compute the coarse residual

      _LinSolver[Level]->_RES->close();
      std::cout << "COARSE Level " << Level << " res linfty " << _LinSolver[Level]->_RES->linfty_norm() << " res l2 " << _LinSolver[Level]->_RES->l2_norm() << std::endl;

    }

    else {

///  std::cout << "************ BEGIN ONE PRE-SMOOTHING *****************"<< std::endl;
#ifdef DEFAULT_PRINT_TIME
      std::clock_t start_time = std::clock();
#endif
#ifdef DEFAULT_PRINT_CONV
      _LinSolver[Level]->_EPS->close();
      double xNormpre = _LinSolver[Level]->_EPS->linfty_norm();
      _LinSolver[Level]->_RESC->close();
      double bNormpre = _LinSolver[Level]->_RESC->linfty_norm();
      _LinSolver[Level]->_KK->close();
      double ANormpre = _LinSolver[Level]->_KK->l1_norm();
      std::cout << "Level " << Level << " ANorm l1 " << ANormpre << " bNorm linfty " << bNormpre  << " xNormINITIAL linfty " << xNormpre << std::endl;
#endif

      rest = _LinSolver[Level]->solve( *_LinSolver[Level]->_KK, *_LinSolver[Level]->_KK, *_LinSolver[Level]->_EPS, *_LinSolver[Level]->_RESC, DEFAULT_EPS_PREPOST, Nc_pre ); //****** smooth on the finer level

#ifdef DEFAULT_PRINT_CONV
      std::cout << " Pre Lev: " << Level << ", res-norm: " << rest.second << " n-its: " << rest.first << std::endl;
#endif
#ifdef DEFAULT_PRINT_TIME
      std::clock_t end_time = std::clock();
      std::cout << " time =" << double( end_time - start_time ) / CLOCKS_PER_SEC << std::endl;
#endif

      _LinSolver[Level]->_RES->resid( *_LinSolver[Level]->_RESC, *_LinSolver[Level]->_EPS, *_LinSolver[Level]->_KK ); //********** compute the residual

///    std::cout << "************ END ONE PRE-SMOOTHING *****************"<< std::endl;

///    std::cout << ">>>>>>>> BEGIN ONE DESCENT >>>>>>>>>>"<< std::endl;

      _LinSolver[Level - 1]->_RESC->matrix_mult( *_LinSolver[Level]->_RES, *_RR[Level - 1] ); //****** restrict the residual from the finer grid ( new rhs )
      _LinSolver[Level - 1]->_EPS->close();                                //initial value of x for the presmoothing iterations
      _LinSolver[Level - 1]->_EPS->zero();                                //initial value of x for the presmoothing iterations

      for( uint g = 1; g <= Gamma; g++ )
        MGStep( Level - 1, Eps1, MaxIter, Gamma, Nc_pre, Nc_coarse, Nc_post ); //***** call MGStep for another possible descent

//at this point you have certainly reached the COARSE level

///      std::cout << ">>>>>>>> BEGIN ONE ASCENT >>>>>>>>"<< std::endl;
#ifdef DEFAULT_PRINT_CONV
      _LinSolver[Level - 1]->_RES->close();
      std::cout << "BEFORE PROL Level " << Level << " res linfty " << _LinSolver[Level - 1]->_RES->linfty_norm() << " res l2 " << _LinSolver[Level - 1]->_RES->l2_norm() << std::endl;
#endif

      _LinSolver[Level]->_RES->matrix_mult( *_LinSolver[Level - 1]->_EPS, *_PP[Level] ); //******** project the dx from the coarser grid
#ifdef DEFAULT_PRINT_CONV
      _LinSolver[Level]->_RES->close();
      //here, the _res contains the prolongation of dx, so the new x is x_old + P dx
      std::cout << "AFTER PROL Level " << Level << " res linfty " << _LinSolver[Level]->_RES->linfty_norm() << " res l2 " << _LinSolver[Level]->_RES->l2_norm() << std::endl;
#endif
      _LinSolver[Level]->_EPS->add( *_LinSolver[Level]->_RES ); // adding the coarser residual to x
      //initial value of x for the post-smoothing iterations
      //_b is the same as before
///   std::cout << "************ BEGIN ONE POST-SMOOTHING *****************"<< std::endl;
      // postsmooting (Nc_post)
#ifdef DEFAULT_PRINT_TIME
      start_time = std::clock();
#endif
#ifdef DEFAULT_PRINT_CONV
      _LinSolver[Level]->_EPS->close();
      double xNormpost = _LinSolver[Level]->_EPS->linfty_norm();
      _LinSolver[Level]->_RESC->close();
      double bNormpost = _LinSolver[Level]->_RESC->linfty_norm();
      _LinSolver[Level]->_KK->close();
      double ANormpost = _LinSolver[Level]->_KK->l1_norm();
      std::cout << "Level " << Level << " ANorm l1 " << ANormpost << " bNorm linfty " << bNormpost << " xNormINITIAL linfty " << xNormpost << std::endl;
#endif

      rest = _LinSolver[Level]->solve( *_LinSolver[Level]->_KK, *_LinSolver[Level]->_KK, *_LinSolver[Level]->_EPS, *_LinSolver[Level]->_RESC, DEFAULT_EPS_PREPOST, Nc_post ); //***** smooth on the coarser level

#ifdef DEFAULT_PRINT_CONV
      std::cout << " Post Lev: " << Level << ", res-norm: " << rest.second << " n-its: " << rest.first << std::endl;
#endif
#ifdef DEFAULT_PRINT_TIME
      end_time = std::clock();
      std::cout << " time =" << double( end_time - start_time ) / CLOCKS_PER_SEC << std::endl;
#endif

      _LinSolver[Level]->_RES->resid( *_LinSolver[Level]->_RESC, *_LinSolver[Level]->_EPS, *_LinSolver[Level]->_KK ); //*******  compute the residual

///    std::cout << "************ END ONE POST-SMOOTHING *****************"<< std::endl;

    }

    _LinSolver[Level]->_RES->close();

    return  rest.second;  //it returns the residual norm of whatever level you are in
    // if this is the l2_norm then also the nonlinear solver is computed in the l2 norm
    //WHAT NORM is THIS?!? l2, but PRECONDITIONED!!!

  }




} //end namespace femus





