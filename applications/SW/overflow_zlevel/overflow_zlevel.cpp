/** tutorial/Ex1
 * This example shows how to:
 * initialize a femus application;
 * define the multilevel-mesh object mlMsh;
 * read from the file ./input/square.neu the coarse-level mesh and associate it to mlMsh;
 * add in mlMsh uniform refined level-meshes;
 * define the multilevel-solution object mlSol associated to mlMsh;
 * add in mlSol different types of finite element solution variables;
 * initialize the solution varables;
 * define vtk and gmv writer objects associated to mlSol;
 * print vtk and gmv binary-format files in ./output directory.
 **/

#include "FemusInit.hpp"
#include "MultiLevelSolution.hpp"
#include "MultiLevelProblem.hpp"
#include "VTKWriter.hpp"
#include "GMVWriter.hpp"
#include "PetscMatrix.hpp"

#include "LinearImplicitSystem.hpp"

#include "slepceps.h"
#include <slepcmfn.h>

using namespace femus;

//double rho1[10]={1000,1000,1000,1000,1000,1000,1000,1000,1000,1000}; 
double rho1[20]={1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000};

double ni_h = 1000; // 0.1, 1, 10, 100, 200

double ni_v = 0.0001;

double dt = 20.; //60 for ni=100, 120 for ni=1

const unsigned NumberOfLayers = 20; 

//const double hRest[10]={50,50,50,50,50,50,50,50,50,50};
const double hRest[20]={100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100};

double InitalValueV(const std::vector < double >& x)
{
  return 0;
}


double InitalValueH(const std::vector < double >& x)
{
  return hRest[0];
}


double InitalValueT(const std::vector < double >& x)
{
  if (x[0]<-80000) return 10; 
  else return 20;
}


double InitalValueB(const std::vector < double >& x)
{
  double d1 = 500;
  double d2 = 2000;
  double x0 = 40000; 
  return (d1 + 0.5*(d2-d1)*(1 + tanh((x[0]-x0)/7000)));
}


bool SetBoundaryCondition(const std::vector < double >& x, const char SolName[], double& value, const int facename, const double time)
{
  bool dirichlet = false; //dirichlet
  if( facename == 1 || facename == 2) dirichlet = true;
  value = 0.;
  return dirichlet;
}


void ETD(MultiLevelProblem& ml_prob);


int main(int argc, char** args)
{

  SlepcInitialize(&argc, &args, PETSC_NULL, PETSC_NULL);

  // init Petsc-MPI communicator
  FemusInit mpinit(argc, args, MPI_COMM_WORLD);

  // define multilevel mesh
  MultiLevelMesh mlMsh;
  double scalingFactor = 1.;

  unsigned numberOfUniformLevels = 1;
  unsigned numberOfSelectiveLevels = 0;

  unsigned nx = static_cast<unsigned>(floor(pow(2.,8) + 0.5)); //Grid cell size = 0.5km 
  nx -= 56;
  
  double length = 200000; //2 * 1465700.; 

  mlMsh.GenerateCoarseBoxMesh(nx, 0, 0, -length / 2, length / 2, 0., 0., 0., 0., EDGE3, "seventh");

  //mlMsh.RefineMesh(numberOfUniformLevels + numberOfSelectiveLevels, numberOfUniformLevels , NULL);
  mlMsh.PrintInfo();

  // define the multilevel solution and attach the mlMsh object to it
  MultiLevelSolution mlSol(&mlMsh);

  for(unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf(name, "h%d", i);
    mlSol.AddSolution(name, DISCONTINUOUS_POLYNOMIAL, ZERO);
    sprintf(name, "v%d", i);
    mlSol.AddSolution(name, LAGRANGE, FIRST);
    sprintf(name, "T%d", i);
    mlSol.AddSolution(name, DISCONTINUOUS_POLYNOMIAL, ZERO);
    sprintf(name, "HT%d", i);
    mlSol.AddSolution(name, DISCONTINUOUS_POLYNOMIAL, ZERO);
  }
  
  mlSol.AddSolution("b", DISCONTINUOUS_POLYNOMIAL, ZERO, 1, false);

  mlSol.AddSolution("eta", DISCONTINUOUS_POLYNOMIAL, ZERO, 1, false);

  mlSol.Initialize("All");
 
  
  for(unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf(name, "v%d", i);
    mlSol.Initialize(name, InitalValueV);
  }
  
  for(unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf(name, "h%d", i);
    mlSol.Initialize(name, InitalValueH);
  }
  
  for(unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf(name, "T%d", i);
    mlSol.Initialize(name, InitalValueT);
  }

  mlSol.Initialize("b", InitalValueB);
  
  mlSol.AttachSetBoundaryConditionFunction(SetBoundaryCondition);
  mlSol.GenerateBdc("All");

  MultiLevelProblem ml_prob(&mlSol);

  // ******* Add FEM system to the MultiLevel problem *******
  LinearImplicitSystem& system = ml_prob.add_system < LinearImplicitSystem > ("SW");
  for(unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf(name, "h%d", i);
    system.AddSolutionToSystemPDE(name);
    sprintf(name, "v%d", i);
    system.AddSolutionToSystemPDE(name);
    sprintf(name, "HT%d", i);
    system.AddSolutionToSystemPDE(name);
  }
  system.init();

  mlSol.SetWriter(VTK);
  std::vector<std::string> print_vars;
  print_vars.push_back("All");
  //mlSol.GetWriter()->SetDebugOutput(true);
  mlSol.GetWriter()->Write(DEFAULT_OUTPUTDIR, "linear", print_vars, 0);

  unsigned numberOfTimeSteps = 7200; //40h with dt=20s
  for(unsigned i = 0; i < numberOfTimeSteps; i++) {
    ETD(ml_prob);
    mlSol.GetWriter()->Write(DEFAULT_OUTPUTDIR, "linear", print_vars, (i + 1)/1);
  }
  return 0;
}


void ETD(MultiLevelProblem& ml_prob)
{

  const unsigned& NLayers = NumberOfLayers;

  adept::Stack& s = FemusInit::_adeptStack;

  LinearImplicitSystem* mlPdeSys  = &ml_prob.get_system<LinearImplicitSystem> ("SW");   // pointer to the linear implicit system named "Poisson"

  unsigned level = ml_prob._ml_msh->GetNumberOfLevels() - 1u;

  Mesh* msh = ml_prob._ml_msh->GetLevel(level);    // pointer to the mesh (level) object
  elem* el = msh->el;  // pointer to the elem object in msh (level)

  MultiLevelSolution* mlSol = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution* sol = ml_prob._ml_sol->GetSolutionLevel(level);    // pointer to the solution (level) object

  LinearEquationSolver* pdeSys = mlPdeSys->_LinSolver[level]; // pointer to the equation (level) object

  SparseMatrix* KK = pdeSys->_KK;  // pointer to the global stifness matrix object in pdeSys (level)
  NumericVector* RES = pdeSys->_RES; // pointer to the global residual vector object in pdeSys (level)
  NumericVector* EPS = pdeSys->_EPS; // pointer to the global residual vector object in pdeSys (level)

  const unsigned  dim = msh->GetDimension(); // get the domain dimension of the problem

  unsigned    iproc = msh->processor_id(); // get the process_id (for parallel computation)
  unsigned    nprocs = msh->n_processors(); // get the process_id (for parallel computation)


  //solution variable
  std::vector < unsigned > solIndexh(NLayers);
  std::vector < unsigned > solPdeIndexh(NLayers);

  std::vector < unsigned > solIndexv(NLayers);
  std::vector < unsigned > solPdeIndexv(NLayers);
  
  std::vector < unsigned > solIndexHT(NLayers);
  std::vector < unsigned > solPdeIndexHT(NLayers);
  
  
   std::vector < unsigned > solIndexT(NLayers);

  vector< int > l2GMapRow; // local to global mapping
  vector< int > l2GMapColumn; // local to global mapping
  
  for(unsigned i = 0; i < NLayers; i++) {
    char name[10];
    sprintf(name, "h%d", i);
    solIndexh[i] = mlSol->GetIndex(name); // get the position of "hi" in the sol object
    solPdeIndexh[i] = mlPdeSys->GetSolPdeIndex(name); // get the position of "hi" in the pdeSys object

    sprintf(name, "v%d", i);
    solIndexv[i] = mlSol->GetIndex(name); // get the position of "vi" in the sol object
    solPdeIndexv[i] = mlPdeSys->GetSolPdeIndex(name); // get the position of "vi" in the pdeSys object
    
    sprintf(name, "HT%d", i);
    solIndexHT[i] = mlSol->GetIndex(name); // get the position of "Ti" in the sol object
    solPdeIndexHT[i] = mlPdeSys->GetSolPdeIndex(name); // get the position of "Ti" in the pdeSys object
    
    sprintf(name, "T%d", i);
    solIndexT[i] = mlSol->GetIndex(name); // get the position of "Ti" in the sol object
    
  }

  unsigned solTypeh = mlSol->GetSolutionType(solIndexh[0]);    // get the finite element type for "hi"
  unsigned solTypev = mlSol->GetSolutionType(solIndexv[0]);    // get the finite element type for "vi"
  unsigned solTypeHT = mlSol->GetSolutionType(solIndexHT[0]);    // get the finite element type for "Ti"
  
  KK->zero();
  RES->zero();

  MatSetOption((static_cast<PetscMatrix*>(KK))->mat(), MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_FALSE);
  
  for(unsigned k=0; k<NumberOfLayers; k++){
    for(unsigned i =  msh->_dofOffset[solTypeHT][iproc]; i <  msh->_dofOffset[solTypeHT][iproc + 1]; i++){
      double valueT = (*sol->_Sol[solIndexT[k]])(i);
      double valueH = (*sol->_Sol[solIndexh[k]])(i);
            
      double valueHT = valueT * valueH;
    
      sol->_Sol[solIndexHT[k]]->set(i, valueHT);
    }
    sol->_Sol[solIndexHT[k]]->close();
  }
  
  //BEGIN 
  double d1 = 500.;
  double d2 = 2000.;
  double x0 = 40000.;
  vector < double > z(NLayers);
  vector < double > x_intersec(NLayers);
  for(unsigned k = 0; k < NLayers; k++){
    z[k] = hRest[0] * k;
    double argument = - 1 + 2.*(z[k]-d1)/(d2-d1);
    if(argument<1 && argument>-1){
      x_intersec[k] = (atanh(- 1 + 2.*(z[k]-d1)/(d2-d1)))*7000+x0;   
      x_intersec[k] -= 100000;
    }
    else x_intersec[k] = -200000.;
    //std::cout<<"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA "<< x_intersec[k] <<std::endl;
  }
  //END  
    
  unsigned start = msh->_dofOffset[solTypeHT][iproc];
  unsigned end = msh->_dofOffset[solTypeHT][iproc + 1];
  for(unsigned i =  start; i <  end; i++){
     
    vector < adept::adouble > solhm(NLayers); 
    vector < adept::adouble > solh(NLayers);    // local coordinates
    vector < adept::adouble > solhp(NLayers); 
    vector < adept::adouble > solvm(NLayers);    // local coordinates
    vector < adept::adouble > solvp(NLayers);    // local coordinates
    vector < adept::adouble > solHTm(NLayers);    // local coordinates
    vector < adept::adouble > solHT(NLayers);    // local coordinates
    vector < adept::adouble > solHTp(NLayers);    // local coordinates
    
    vector< adept::adouble > aResh(NLayers);
    vector< adept::adouble > aResv(NLayers);
    vector< adept::adouble > aResHT(NLayers);
   
    unsigned bc1 = (i == start)? 0 : 1;
    unsigned bc2 = (i == end-1)? 0 : 1;
    
    l2GMapRow.resize(2 * NLayers);
    l2GMapColumn.resize( 2 * ( 2 + bc1 + bc2) * NLayers);
    
    std::fill(aResh.begin(), aResh.end(), 0);    //set aRes to zero
    std::fill(aResHT.begin(), aResHT.end(), 0);  //set aRes to zero

        
    for(unsigned j = 0; j < NLayers; j++) {
      
      solh[j] = (*sol->_Sol[solIndexh[j]])(i);      
      solHT[j] = (*sol->_Sol[solIndexHT[j]])(i);  
      l2GMapRow[j] = pdeSys->GetSystemDof(solIndexh[j], solPdeIndexh[j], 0, i);   
      l2GMapRow[NLayers + j] = pdeSys->GetSystemDof(solIndexHT[j], solPdeIndexHT[j], 0, i);   
      
      l2GMapColumn[j] = pdeSys->GetSystemDof(solIndexh[j], solPdeIndexh[j], 0, i);   
      l2GMapColumn[NLayers + j] = pdeSys->GetSystemDof(solIndexHT[j], solPdeIndexHT[j], 0, i);   
      
      solvm[j] = (*sol->_Sol[solIndexv[j]])(i); 
      solvp[j] = (*sol->_Sol[solIndexv[j]])(i+1); 

      l2GMapColumn[2 * NLayers + j] = pdeSys->GetSystemDof(solIndexv[j], solPdeIndexv[j], 0, i);   
      l2GMapColumn[3 * NLayers + j] = pdeSys->GetSystemDof(solIndexv[j], solPdeIndexv[j], 1, i);   
            
      if(i > start){ 
	solhm[j] = (*sol->_Sol[solIndexh[j]])(i-1);      
	solHTm[j] = (*sol->_Sol[solIndexHT[j]])(i-1); 
	
	l2GMapColumn[4 * NLayers + j] = pdeSys->GetSystemDof(solIndexh[j], solPdeIndexh[j], 0, i-1);   
	l2GMapColumn[5 * NLayers + j] = pdeSys->GetSystemDof(solIndexHT[j], solPdeIndexHT[j], 0, i-1);   
	
      }      
      
      if( i < end - 1){ 
	solhp[j] = (*sol->_Sol[solIndexh[j]])(i+1);      
	solHTp[j] = (*sol->_Sol[solIndexHT[j]])(i+1);      
	
	l2GMapColumn[(4 + 2 * bc1) * NLayers + j] = pdeSys->GetSystemDof(solIndexh[j], solPdeIndexh[j], 0, i+1);   
	l2GMapColumn[(5 + 2 * bc1) * NLayers + j] = pdeSys->GetSystemDof(solIndexHT[j], solPdeIndexHT[j], 0, i+1);   
      }
    }
    s.new_recording();
    
    vector < double > x(2);    // local coordinates
    for(unsigned j = 0; j < 2; j++) {
      unsigned xDof  = msh->GetSolutionDof(j, i, 2);    // global to global mapping between coordinates node and coordinate dof
      x[j] = (*msh->_topology->_Sol[0])(xDof);      // global extraction and local storage for the element coordinates
    }
    double dx = x[1] - x[0];
    
    double xmid = 0.5*(x[1] + x[0]);
    double d1 = 500;
    double d2 = 2000;
    double x0 = 40000;
    double b = d1 + 0.5*(d2-d1)*(1 + tanh((xmid-x0)/7000));
        
    double hTot = 0.;
    for(unsigned k = 0; k < NLayers; k++) {
      hTot += solh[k].value();
    }
    
    std::vector < double > hALE(NLayers, 0.); 
      
    hALE[0] = hRest[0] + (hTot - b);
    for(unsigned k = 1; k < NLayers; k++){
      hALE[k] = hRest[k];
    }
       
    std::vector < double > w(NLayers+1, 0.);
	  
    for(unsigned k = NLayers; k>1; k--){
      w[k-1] = w[k] - (  0.5 * ( solh[k-1].value() + solhp[k-1].value() ) * solvp[k-1].value() 
		       - 0.5 * ( solh[k-1].value() + solhm[k-1].value() ) * solvm[k-1].value() )/dx 
		    - ( hALE[k-1] - solh[k-1].value()) / dt;//TODO		    
		    //std::cout<<"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"<<w[k-1]<<std::endl;
    }
      
      
    for(unsigned k = 0; k < NLayers; k++) {
      if(xmid>x_intersec[k]){//TODO
      
        if( i > start ){
	  aResh[k] += 0.5 * (solhm[k] + solh[k]) * solvm[k] / dx; 
        }
        if(i < end - 1){
	  aResh[k] -= 0.5 * (solh[k] + solhp[k]) * solvp[k] / dx; 
        }
        aResh[k] += w[k+1] - w[k];	    
      
	
        if( i > start ){
          aResHT[k] += 0.5 * (solHTm[k] + solHT[k]) * solvm[k]  / dx; 
        }
        if(i < end - 1){
	  aResHT[k] -= 0.5 * (solHT[k] + solHTp[k]) * solvp[k]  / dx; 
        }
	
        if(k<NLayers-1){
	  aResHT[k] += w[k+1] * 0.5 * (solHT[k]/solh[k] + solHT[k+1]/solh[k+1]);
        }
        if( k > 0){
	  aResHT[k] -= w[k] * 0.5 * (solHT[k-1]/solh[k-1] + solHT[k]/solh[k] );
        }
      }
    }
      
    vector< double > Res(NLayers * 2); // local redidual vector
    for(unsigned k = 0; k < NLayers; k++) {
      Res[k] =  aResh[k].value();
      Res[NLayers + k] =  aResHT[k].value();
      //std::cout<< "Res["<<k<<"] = " << Res[k] <<std::endl;
      //std::cout<< "Res["<<NLayers+k<<"] = " << Res[NLayers+k] <<std::endl;
    }

    RES->add_vector_blocked(Res, l2GMapRow);
  
    s.dependent(&aResh[0], NLayers);
    s.dependent(&aResHT[0], NLayers);

    // define the independent variables
    s.independent(&solh[0], NLayers);
    s.independent(&solHT[0], NLayers);
    s.independent(&solvm[0], NLayers);
    s.independent(&solvp[0], NLayers);
    if(i > start){
      s.independent(&solhm[0], NLayers);
      s.independent(&solHTm[0], NLayers);
    }
    if(i < end - 1){
      s.independent(&solhp[0], NLayers);
      s.independent(&solHTp[0], NLayers);
    }
    
    // get the jacobian matrix (ordered by row major )
    vector < double > Jac(NLayers * 2 * NLayers * 2 * (2 + bc1 + bc2) ); 
    s.jacobian(&Jac[0], true);
    
    //store K in the global matrix KK
    KK->add_matrix_blocked(Jac, l2GMapRow, l2GMapColumn);

    s.clear_independents();
    s.clear_dependents();   
         
  }
  
  
  
  start = msh->_dofOffset[solTypev][iproc] + 1;
  end = msh->_dofOffset[solTypev][iproc + 1] - 1;
  for(unsigned i =  start; i <  end; i++){
    
    vector < adept::adouble > solhm(NLayers); 
    vector < adept::adouble > solhp(NLayers); 
    vector < adept::adouble > solvm(NLayers);    // local coordinates
    vector < adept::adouble > solv(NLayers);    // local coordinates
    vector < adept::adouble > solvp(NLayers);    // local coordinates
    vector < adept::adouble > solHTm(NLayers);    // local coordinates
    vector < adept::adouble > solHTp(NLayers);    // local coordinates
      
    vector< adept::adouble > aResh(NLayers);
    vector< adept::adouble > aResv(NLayers);
    vector< adept::adouble > aResHT(NLayers);
   
        
    l2GMapRow.resize(NLayers);
    l2GMapColumn.resize( 7 * NLayers);
    
    std::fill(aResv.begin(), aResv.end(), 0);    //set aRes to zero
        
    for(unsigned j = 0; j < NLayers; j++) {
      
      solv[j] = (*sol->_Sol[solIndexv[j]])(i); 
      l2GMapRow[j] = pdeSys->GetSystemDof(solIndexv[j], solPdeIndexv[j], 0, i);  
      l2GMapColumn[j] = pdeSys->GetSystemDof(solIndexv[j], solPdeIndexv[j], 0, i);   
      
      solvm[j] = (*sol->_Sol[solIndexv[j]])(i-1); 
      solvp[j] = (*sol->_Sol[solIndexv[j]])(i+1); 
      l2GMapColumn[NLayers + j] = pdeSys->GetSystemDof(solIndexv[j], solPdeIndexv[j], 0, i-1);   
      l2GMapColumn[2 * NLayers + j] = pdeSys->GetSystemDof(solIndexv[j], solPdeIndexv[j], 1, i);   
      
               
      solhm[j] = (*sol->_Sol[solIndexh[j]])(i-1);      
      solHTm[j] = (*sol->_Sol[solIndexHT[j]])(i-1); 
	
      l2GMapColumn[3 * NLayers + j] = pdeSys->GetSystemDof(solIndexh[j], solPdeIndexh[j], 0, i-1);   
      l2GMapColumn[4 * NLayers + j] = pdeSys->GetSystemDof(solIndexHT[j], solPdeIndexHT[j], 0, i-1);   
	
      solhp[j]  = (*sol->_Sol[solIndexh[j]])(i);      
      solHTp[j] = (*sol->_Sol[solIndexHT[j]])(i);      
	
      l2GMapColumn[5 * NLayers + j] = pdeSys->GetSystemDof(solIndexh[j], solPdeIndexh[j], 0, i);   
      l2GMapColumn[6 * NLayers + j] = pdeSys->GetSystemDof(solIndexHT[j], solPdeIndexHT[j], 0, i);   
      
    }
    s.new_recording();
    
    std::vector <double> xm(2);
    std::vector <double> xp(2);
    for(unsigned j = 0; j < 2; j++) {
      unsigned xDofm  = msh->GetSolutionDof(j, i-1, 2);    // global to global mapping between coordinates node and coordinate dof
      xm[j] = (*msh->_topology->_Sol[0])(xDofm);      // global extraction and local storage for the element coordinates
      
      unsigned xDofp  = msh->GetSolutionDof(j, i, 2);    // global to global mapping between coordinates node and coordinate dof
      xp[j] = (*msh->_topology->_Sol[0])(xDofp);      // global extraction and local storage for the element coordinates
    }
    double dxm = xm[1] - xm[0];
    double dxp = xp[1] - xp[0];
    
    double xmidm = 0.5 * (xm[0] + xm[1]);
    double xmidp = 0.5 * (xp[0] + xp[1]);
    
    double x_check = 0.5 * (xmidm+xmidp);
    
    double d1 = 500;
    double d2 = 2000;
    double x0 = 40000;
    double bm = d1 + 0.5*(d2-d1)*(1 + tanh((xmidm-x0)/7000));
    double bp = d1 + 0.5*(d2-d1)*(1 + tanh((xmidp-x0)/7000));
        
    double hTotm = 0.;
    double hTotp = 0.;    
    
    double beta = 0.2;
    double TRef = 5.;
    
    std::vector < adept::adouble > Pm(NLayers);
    std::vector < adept::adouble > Pp(NLayers);
    std::vector < adept::adouble > zMidm(NLayers);
    std::vector < adept::adouble > zMidp(NLayers);
    for(unsigned k = 0; k < NLayers; k++) {
      hTotm += solhm[k].value();
      hTotp += solhp[k].value();

      adept::adouble rhokm = rho1[k] - beta * (solHTm[k]/solhm[k] - TRef);
      adept::adouble rhokp = rho1[k] - beta * (solHTp[k]/solhp[k] - TRef);
      
      Pm[k] = 0.5 * 9.81 * rhokm * solhm[k];
      Pp[k] = 0.5 * 9.81 * rhokp * solhp[k]; 
      
      zMidm[k] = -bm + solhm[k]/2;
      zMidp[k] = -bp + solhp[k]/2;
      for(unsigned i = k+1; i < NLayers; i++) {
        zMidm[k] += solhm[i];
	zMidp[k] += solhp[i];
      }
      
      Pm[k] += rhokm * 9.81 * zMidm[k];
      Pp[k] += rhokm * 9.81 * zMidp[k]; 
      //Pm[k] = - rhokm * 9.81 * bm; // bottom topography
      //Pp[k] = - rhokp * 9.81 * bp; // bottom topography
      for( unsigned j = 0; j < k; j++){
	 adept::adouble rhojm = rho1[j] - beta * (solHTm[j]/solhm[j] - TRef);
	 adept::adouble rhojp = rho1[j] - beta * (solHTp[j]/solhp[j] - TRef);
      //for( unsigned j = 0; j < NLayers; j++){
	 //BEGIN Isopycnal Pressure
	 //adept::adouble rhojm = (j <= k) ? (rho1[j] - beta * (solHTm[j]/solhm[j] - TRef)) : rhokm;
 	 //adept::adouble rhojp = (j <= k) ? (rho1[j] - beta * (solHTp[j]/solhp[j] - TRef)) : rhokp;
	 //END
	 //BEGIN Modified Isopycnal Pressure
// 	 adept::adouble rhojm;
// 	 adept::adouble rhojp;
// 	 if (j<k){
// 	   rhojm = (rho1[j] - beta * solHTm[j]/solhm[j] - TRef) * rhokm/1024;
// 	   rhojp = (rho1[j] - beta * solHTp[j]/solhp[j] - TRef) * rhokp/1024;
// 	 }
// 	 else if (j==k){
// 	   rhojm = 0.5 * rhokm/1024 * (rhokm + (rhokm+rhokp)/2);  
// 	   rhojp = 0.5 * rhokp/1024 * (rhokp + (rhokm+rhokp)/2);
// 	 }
// 	 else{
// 	   rhojm = (rho1[j] - beta * solHTm[j]/solhm[j] - TRef) * rhokm/1024 * (rhokm+rhokp)/2;
// 	   rhojp = (rho1[j] - beta * solHTp[j]/solhp[j] - TRef) * rhokp/1024 * (rhokm+rhokp)/2;   
// 	 }
	 //END
	 Pm[k] += rhojm * 9.81 * solhm[j];
	 Pp[k] += rhojp * 9.81 * solhp[j];
	 //std::cout<<"HHHHHHHHHHHHHHHHHHH "<<j<<std::endl;
	 //std::cout<<"AAAAAAAAAAAAAAAAAAA "<<rhojm<<" , "<<rhojp<<std::endl;
      }
      Pm[k] /= rho1[k];
      Pp[k] /= rho1[k];
      //std::cout<<"BBBBBBBBBBBBBBBBBBB "<<rhokm<<" , "<<rhokp<<std::endl;
      //std::cout<<"CCCCCCCCCCCCCCCCCCC "<<Pm[k]<<" , "<<Pp[k]<<std::endl;
    }
     
    std::vector < double > hALEm(NLayers, 0.); 
    std::vector < double > hALEp(NLayers, 0.); 
      
    hALEm[0] = hRest[0] + (hTotm - bm);
    hALEp[0] = hRest[0] + (hTotp - bp);
    for(unsigned k = 1; k < NLayers; k++){
      hALEm[k] = hRest[k];
      hALEp[k] = hRest[k];      
    }
       
//     std::vector < double > wm(NLayers+1, 0.);
//     std::vector < double > wp(NLayers+1, 0.);
// 	  
//     for(unsigned k = NLayers; k>1; k--){
//       wm[k-1] = wm[k] -  solhm[k-1].value() * (solv[k-1].value() - solvm[k-1].value() )/dxm - ( hALEm[k-1] - solhm[k-1].value()) / dt;
//       wp[k-1] = wp[k] -  solhp[k-1].value() * (solvp[k-1].value() - solv[k-1].value() )/dxp - ( hALEp[k-1] - solhp[k-1].value()) / dt;
//     }
    
    std::vector < double > w(NLayers+1, 0.);
   
    double dx = 0.5 * (dxm + dxp);
    for(unsigned k = NLayers; k>1; k--){
      w[k-1] = w[k] -  ( solhp[k-1].value() * (0.5 * ( solv[k-1].value() + solvp[k-1].value() ) )
		       - solhm[k-1].value() * (0.5 * ( solv[k-1].value() + solvm[k-1].value() ) ) ) / dx
		       - ( 0.5 * ( hALEm[k-1] + hALEp[k-1] )  - 0.5 * ( solhm[k-1].value() + solhp[k-1].value() ) ) / dt;
		       //std::cout<<"w in u equation"<<w[k-1]<<std::endl;
      
    }  
      
    for(unsigned k = 0; k < NLayers; k++) {
      if(x_check>x_intersec[k]){
        adept::adouble vMidm = 0.5 * (solvm[k] + solv[k]);
        adept::adouble fvm = 0.5 * vMidm * vMidm + Pm[k];
        aResv[k] +=  fvm / dxm;
      
        adept::adouble vMidp = 0.5 * (solv[k] + solvp[k]);
        adept::adouble fvp = 0.5 * vMidp * vMidp + Pp[k];      
        aResv[k] -=  fvp / dxp;
      
        adept::adouble deltaZt_k;
        adept::adouble deltaZt_kplus1;
        if ( k > 0 ){
	  deltaZt_k = 4. * (solv[k-1]-solv[k]) / (solhm[k-1]+solhm[k]+solhp[k-1]+solhp[k]);
	  aResv[k] -= 0.5 * w[k] * deltaZt_k;
        }
        if (k < NLayers - 1) {
	  deltaZt_kplus1 = 4. * (solv[k]-solv[k+1]) / (solhm[k]+solhm[k+1]+solhp[k]+solhp[k+1] );
	  aResv[k] -= 0.5 * w[k+1] * deltaZt_kplus1; 
        }
      
        aResv[k] += ni_h*(solvm[k] - solv[k])/(dxm*dxm); // horizontal diffusion
        aResv[k] += ni_h*(solvp[k] - solv[k])/(dxp*dxp); // horizontal diffusion    
      
        aResv[k] += 2. * ni_v * (deltaZt_k - deltaZt_kplus1) / (solhm[k] + solhp[k]); // vertical diffusion
      
        //aResv[k] += 0.01*sqrt(solv[k]*solv[k])*solv[k];//drag force
	}
    }
      
    vector< double > Res(NLayers); // local redidual vector
    for(unsigned k = 0; k < NLayers; k++) {
      Res[k] =  aResv[k].value();
      //std::cout<< "Res["<<k<<"] = " << Res[k] <<std::endl;
    }

    RES->add_vector_blocked(Res, l2GMapRow);
  
    s.dependent(&aResv[0], NLayers);
    
    // define the independent variables
    s.independent(&solv[0], NLayers);
    s.independent(&solvm[0], NLayers);
    s.independent(&solvp[0], NLayers);
    s.independent(&solhm[0], NLayers);
    s.independent(&solHTm[0], NLayers);
    s.independent(&solhp[0], NLayers);
    s.independent(&solHTp[0], NLayers);
        
    // get the jacobian matrix (ordered by row major )
    vector < double > Jac(NLayers * NLayers * 7 );
    s.jacobian(&Jac[0], true);
    
    //store K in the global matrix KK
    KK->add_matrix_blocked(Jac, l2GMapRow, l2GMapColumn);

    s.clear_independents();
    s.clear_dependents();   
         
  }
  
  
  RES->close();
  KK->close();

  
  
//   PetscViewer    viewer;
//   PetscViewerDrawOpen(PETSC_COMM_WORLD,NULL,NULL,0,0,900,900,&viewer);
//   PetscObjectSetName((PetscObject)viewer,"FSI matrix");
//   PetscViewerPushFormat(viewer,PETSC_VIEWER_DRAW_LG);
//   MatView((static_cast<PetscMatrix*>(KK))->mat(),viewer);
//   double a;
//   std::cin>>a;
//
  
//  abort();
  MFN mfn;
  Mat A = (static_cast<PetscMatrix*>(KK))->mat();
  FN f, f1, f2, f3 , f4;
  
  //std::cout << "dt = " << dt << " dx = "<< dx << " maxWaveSpeed = "<<maxWaveSpeed << std::endl;
  std::cout << "dt = " << dt << std::endl;
  
  //dt = 100.;

  Vec v = (static_cast< PetscVector* >(RES))->vec();
  Vec y = (static_cast< PetscVector* >(EPS))->vec();

  MFNCreate( PETSC_COMM_WORLD, &mfn );

  MFNSetOperator( mfn, A );
  MFNGetFN( mfn, &f );


//   FNCreate(PETSC_COMM_WORLD, &f1);
//   FNCreate(PETSC_COMM_WORLD, &f2);
//   FNCreate(PETSC_COMM_WORLD, &f3);
//   FNCreate(PETSC_COMM_WORLD, &f4);
// 
//   FNSetType(f1, FNEXP);
// 
//   FNSetType(f2, FNRATIONAL);
//   double coeff1[1] = { -1};
//   FNRationalSetNumerator(f2, 1, coeff1);
//   FNRationalSetDenominator(f2, 0, PETSC_NULL);
// 
//   FNSetType( f3, FNCOMBINE );
// 
//   FNCombineSetChildren(f3, FN_COMBINE_ADD, f1, f2);
// 
//   FNSetType(f4, FNRATIONAL);
//   double coeff2[2] = {1., 0.};
//   FNRationalSetNumerator(f4, 2, coeff2);
//   FNRationalSetDenominator(f4, 0, PETSC_NULL);
// 
//   FNSetType( f, FNCOMBINE );
// 
//   FNCombineSetChildren(f, FN_COMBINE_DIVIDE, f3, f4);

  FNPhiSetIndex(f,1);
  FNSetType( f, FNPHI );
// FNView(f,PETSC_VIEWER_STDOUT_WORLD);

  FNSetScale( f, dt, dt);
  MFNSetFromOptions( mfn );

  MFNSolve( mfn, v, y);
  MFNDestroy( &mfn );

//   FNDestroy(&f1);
//   FNDestroy(&f2);
//   FNDestroy(&f3);
//   FNDestroy(&f4);


  sol->UpdateSol(mlPdeSys->GetSolPdeIndex(), EPS, pdeSys->KKoffset); 

  unsigned solIndexeta = mlSol->GetIndex("eta");
  unsigned solIndexb = mlSol->GetIndex("b");
  sol->_Sol[solIndexeta]->zero();
  for(unsigned k=0;k<NumberOfLayers;k++){
    sol->_Sol[solIndexeta]->add(*sol->_Sol[solIndexh[k]]);
  }
  sol->_Sol[solIndexeta]->add(-1,*sol->_Sol[solIndexb]);

  for(unsigned k=0; k<NumberOfLayers; k++){
    for(unsigned i =  msh->_dofOffset[solTypeHT][iproc]; i <  msh->_dofOffset[solTypeHT][iproc + 1]; i++){
      double valueHT = (*sol->_Sol[solIndexHT[k]])(i);
      double valueH = (*sol->_Sol[solIndexh[k]])(i);
            
      double valueT = valueHT/valueH;
    
      sol->_Sol[solIndexT[k]]->set(i, valueT);
    }
    
    sol->_Sol[solIndexT[k]]->close();
    
  }
  
  
}






