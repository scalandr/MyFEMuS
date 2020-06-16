#include <boost/assign/std.hpp>
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
#include "MultiLevelProblem.hpp"
#include "VTKWriter.hpp"
#include "GMVWriter.hpp"
#include "PetscMatrix.hpp"

#include "TransientSystem.hpp"
#include "LinearImplicitSystem.hpp"

#include "slepceps.h"
#include <slepcmfn.h>
#include <slepcsvd.h>


using namespace femus;

double dt = 1.;

double k_v = (2.5) * (0.00001);

double pi = acos (-1.);
//double k_h = 1. / ( 10 * pi );
double k_h = 0.0001 ;
double lambda = 1./(17*365*24*60*60);

const unsigned NumberOfLayers = 100;

unsigned counter = 0;
unsigned counter2 = 0;

clock_t start_time = clock();

bool phi_once = false;
bool constant_jac = false;
bool twostage = true;
bool assembly = true; //assembly must be left always true
bool slepc = true;

std::vector < std::vector < std::vector < double > > > phi1A;

// const double hRest[4] = {2.5, 2.5, 2.5, 2.5};

// const double hRest[20] = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};

// const double hRest[40] = {0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25,
//                          0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25
//                         };

// const double hRest[80] = {0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125,
//                           0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125,
//                           0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125,
//                           0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125
//                          };

const double hRest[100] = {0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
                           0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
                           0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
                           0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1
                          };

double InitalValueVi (const std::vector < double >& x , const unsigned &i)
{
  double psi1 = 1. - (x[0] - 5.) * (x[0] - 5.) * (x[0] - 5.) * (x[0] - 5.) / (5.*5.*5.*5.); //10x10 box test
  //double psi1 = 1. - (pow ( (x[0] - 20.), 16) / pow (20., 16));  //40x10 rectangle test
  //double psi1 = 1. - ( pow(( x[0] - 5. ), 2) / 25. );
  double z = -10. + hRest[0] / 2. + hRest[0] * (NumberOfLayers - i);
  double d_psi2 = (- (2.*z + 10.)) / (5 * 5);
  return (psi1 * d_psi2);
}

//  double InitalValueV0 ( const std::vector < double >& x ) {
//   //double psi1 = ( 10. - x[0] ) * x[0] / ( 5.*5. );
//   double psi1 = 1. - ( x[0] - 5. ) * ( x[0] - 5. ) * ( x[0] - 5. ) * ( x[0] - 5. ) / ( 5.*5.*5.*5. );
//   double z = -10 + hRest[0] / 2 + hRest[0] * ( NumberOfLayers - 1 );
//   double d_psi2 = ( - ( 2.*z + 10. ) ) / ( 5 * 5 );
//   //return ( -psi1 * d_psi2 );
//   return ( psi1 * d_psi2 );
// }

double InitalValueV0 (const std::vector < double >& x)
{
  return InitalValueVi (x, 1);
}
double InitalValueV1 (const std::vector < double >& x)
{
  return InitalValueVi (x, 2);
}
double InitalValueV2 (const std::vector < double >& x)
{
  return InitalValueVi (x, 3);
}
double InitalValueV3 (const std::vector < double >& x)
{
  return InitalValueVi (x, 4);
}
double InitalValueV4 (const std::vector < double >& x)
{
  return InitalValueVi (x, 5);
}
double InitalValueV5 (const std::vector < double >& x)
{
  return InitalValueVi (x, 6);
}
double InitalValueV6 (const std::vector < double >& x)
{
  return InitalValueVi (x, 7);
}
double InitalValueV7 (const std::vector < double >& x)
{
  return InitalValueVi (x, 8);
}
double InitalValueV8 (const std::vector < double >& x)
{
  return InitalValueVi (x, 9);
}
double InitalValueV9 (const std::vector < double >& x)
{
  return InitalValueVi (x, 10);
}
double InitalValueV10 (const std::vector < double >& x)
{
  return InitalValueVi (x, 11);
}
double InitalValueV11 (const std::vector < double >& x)
{
  return InitalValueVi (x, 12);
}
double InitalValueV12 (const std::vector < double >& x)
{
  return InitalValueVi (x, 13);
}
double InitalValueV13 (const std::vector < double >& x)
{
  return InitalValueVi (x, 14);
}
double InitalValueV14 (const std::vector < double >& x)
{
  return InitalValueVi (x, 15);
}
double InitalValueV15 (const std::vector < double >& x)
{
  return InitalValueVi (x, 16);
}
double InitalValueV16 (const std::vector < double >& x)
{
  return InitalValueVi (x, 17);
}
double InitalValueV17 (const std::vector < double >& x)
{
  return InitalValueVi (x, 18);
}
double InitalValueV18 (const std::vector < double >& x)
{
  return InitalValueVi (x, 19);
}
double InitalValueV19 (const std::vector < double >& x)
{
  return InitalValueVi (x, 20);
}
double InitalValueV20 (const std::vector < double >& x)
{
  return InitalValueVi (x, 21);
}
double InitalValueV21 (const std::vector < double >& x)
{
  return InitalValueVi (x, 22);
}
double InitalValueV22 (const std::vector < double >& x)
{
  return InitalValueVi (x, 23);
}
double InitalValueV23 (const std::vector < double >& x)
{
  return InitalValueVi (x, 24);
}
double InitalValueV24 (const std::vector < double >& x)
{
  return InitalValueVi (x, 25);
}
double InitalValueV25 (const std::vector < double >& x)
{
  return InitalValueVi (x, 26);
}
double InitalValueV26 (const std::vector < double >& x)
{
  return InitalValueVi (x, 27);
}
double InitalValueV27 (const std::vector < double >& x)
{
  return InitalValueVi (x, 28);
}
double InitalValueV28 (const std::vector < double >& x)
{
  return InitalValueVi (x, 29);
}
double InitalValueV29 (const std::vector < double >& x)
{
  return InitalValueVi (x, 30);
}
double InitalValueV30 (const std::vector < double >& x)
{
  return InitalValueVi (x, 31);
}
double InitalValueV31 (const std::vector < double >& x)
{
  return InitalValueVi (x, 32);
}
double InitalValueV32 (const std::vector < double >& x)
{
  return InitalValueVi (x, 33);
}
double InitalValueV33 (const std::vector < double >& x)
{
  return InitalValueVi (x, 34);
}
double InitalValueV34 (const std::vector < double >& x)
{
  return InitalValueVi (x, 35);
}
double InitalValueV35 (const std::vector < double >& x)
{
  return InitalValueVi (x, 36);
}
double InitalValueV36 (const std::vector < double >& x)
{
  return InitalValueVi (x, 37);
}
double InitalValueV37 (const std::vector < double >& x)
{
  return InitalValueVi (x, 38);
}
double InitalValueV38 (const std::vector < double >& x)
{
  return InitalValueVi (x, 39);
}
double InitalValueV39 (const std::vector < double >& x)
{
  return InitalValueVi (x, 40);
}
double InitalValueV40 (const std::vector < double >& x)
{
  return InitalValueVi (x, 41);
}
double InitalValueV41 (const std::vector < double >& x)
{
  return InitalValueVi (x, 42);
}
double InitalValueV42 (const std::vector < double >& x)
{
  return InitalValueVi (x, 43);
}
double InitalValueV43 (const std::vector < double >& x)
{
  return InitalValueVi (x, 44);
}
double InitalValueV44 (const std::vector < double >& x)
{
  return InitalValueVi (x, 45);
}
double InitalValueV45 (const std::vector < double >& x)
{
  return InitalValueVi (x, 46);
}
double InitalValueV46 (const std::vector < double >& x)
{
  return InitalValueVi (x, 47);
}
double InitalValueV47 (const std::vector < double >& x)
{
  return InitalValueVi (x, 48);
}
double InitalValueV48 (const std::vector < double >& x)
{
  return InitalValueVi (x, 49);
}
double InitalValueV49 (const std::vector < double >& x)
{
  return InitalValueVi (x, 50);
}
double InitalValueV50 (const std::vector < double >& x)
{
  return InitalValueVi (x, 51);
}
double InitalValueV51 (const std::vector < double >& x)
{
  return InitalValueVi (x, 52);
}
double InitalValueV52 (const std::vector < double >& x)
{
  return InitalValueVi (x, 53);
}
double InitalValueV53 (const std::vector < double >& x)
{
  return InitalValueVi (x, 54);
}
double InitalValueV54 (const std::vector < double >& x)
{
  return InitalValueVi (x, 55);
}
double InitalValueV55 (const std::vector < double >& x)
{
  return InitalValueVi (x, 56);
}
double InitalValueV56 (const std::vector < double >& x)
{
  return InitalValueVi (x, 57);
}
double InitalValueV57 (const std::vector < double >& x)
{
  return InitalValueVi (x, 58);
}
double InitalValueV58 (const std::vector < double >& x)
{
  return InitalValueVi (x, 59);
}
double InitalValueV59 (const std::vector < double >& x)
{
  return InitalValueVi (x, 60);
}
double InitalValueV60 (const std::vector < double >& x)
{
  return InitalValueVi (x, 61);
}
double InitalValueV61 (const std::vector < double >& x)
{
  return InitalValueVi (x, 62);
}
double InitalValueV62 (const std::vector < double >& x)
{
  return InitalValueVi (x, 63);
}
double InitalValueV63 (const std::vector < double >& x)
{
  return InitalValueVi (x, 64);
}
double InitalValueV64 (const std::vector < double >& x)
{
  return InitalValueVi (x, 65);
}
double InitalValueV65 (const std::vector < double >& x)
{
  return InitalValueVi (x, 66);
}
double InitalValueV66 (const std::vector < double >& x)
{
  return InitalValueVi (x, 67);
}
double InitalValueV67 (const std::vector < double >& x)
{
  return InitalValueVi (x, 68);
}
double InitalValueV68 (const std::vector < double >& x)
{
  return InitalValueVi (x, 69);
}
double InitalValueV69 (const std::vector < double >& x)
{
  return InitalValueVi (x, 70);
}
double InitalValueV70 (const std::vector < double >& x)
{
  return InitalValueVi (x, 71);
}
double InitalValueV71 (const std::vector < double >& x)
{
  return InitalValueVi (x, 72);
}
double InitalValueV72 (const std::vector < double >& x)
{
  return InitalValueVi (x, 73);
}
double InitalValueV73 (const std::vector < double >& x)
{
  return InitalValueVi (x, 74);
}
double InitalValueV74 (const std::vector < double >& x)
{
  return InitalValueVi (x, 75);
}
double InitalValueV75 (const std::vector < double >& x)
{
  return InitalValueVi (x, 76);
}
double InitalValueV76 (const std::vector < double >& x)
{
  return InitalValueVi (x, 77);
}
double InitalValueV77 (const std::vector < double >& x)
{
  return InitalValueVi (x, 78);
}
double InitalValueV78 (const std::vector < double >& x)
{
  return InitalValueVi (x, 79);
}
double InitalValueV79 (const std::vector < double >& x)
{
  return InitalValueVi (x, 80);
}
double InitalValueV80 (const std::vector < double >& x)
{
  return InitalValueVi (x, 81);
}
double InitalValueV81 (const std::vector < double >& x)
{
  return InitalValueVi (x, 82);
}
double InitalValueV82 (const std::vector < double >& x)
{
  return InitalValueVi (x, 83);
}
double InitalValueV83 (const std::vector < double >& x)
{
  return InitalValueVi (x, 84);
}
double InitalValueV84 (const std::vector < double >& x)
{
  return InitalValueVi (x, 85);
}
double InitalValueV85 (const std::vector < double >& x)
{
  return InitalValueVi (x, 86);
}
double InitalValueV86 (const std::vector < double >& x)
{
  return InitalValueVi (x, 87);
}
double InitalValueV87 (const std::vector < double >& x)
{
  return InitalValueVi (x, 88);
}
double InitalValueV88 (const std::vector < double >& x)
{
  return InitalValueVi (x, 89);
}
double InitalValueV89 (const std::vector < double >& x)
{
  return InitalValueVi (x, 90);
}
double InitalValueV90 (const std::vector < double >& x)
{
  return InitalValueVi (x, 91);
}
double InitalValueV91 (const std::vector < double >& x)
{
  return InitalValueVi (x, 92);
}
double InitalValueV92 (const std::vector < double >& x)
{
  return InitalValueVi (x, 93);
}
double InitalValueV93 (const std::vector < double >& x)
{
  return InitalValueVi (x, 94);
}
double InitalValueV94 (const std::vector < double >& x)
{
  return InitalValueVi (x, 95);
}
double InitalValueV95 (const std::vector < double >& x)
{
  return InitalValueVi (x, 96);
}
double InitalValueV96 (const std::vector < double >& x)
{
  return InitalValueVi (x, 97);
}
double InitalValueV97 (const std::vector < double >& x)
{
  return InitalValueVi (x, 98);
}
double InitalValueV98 (const std::vector < double >& x)
{
  return InitalValueVi (x, 99);
}
double InitalValueV99 (const std::vector < double >& x)
{
  return InitalValueVi (x, 100);
}



double InitalValueH (const std::vector < double >& x)
{
  return hRest[0];
}

double InitalValueT (const std::vector < double >& x)
{

  if (x[0] < 5.) {
    return 5.;
  }
  else {
    return 15.;
  }
  
  //return fabs(sin(x[0]));

}

double InitalValueS (const std::vector < double >& x)
{

  if (x[0] < 5.) {
    return 5.;
  }

  else {
    return 15.;
  }

}

double InitalValueZ (const std::vector < double >& x)
{

  if (x[0] < 5.) {
    return 5.;
  }

  else {
    return 15.;
  }

}

double InitalValueB (const std::vector < double >& x)
{
  return 10.; //( H_shelf + H_0 / 2 * (1 + tanh(hh / phi)) );
}


bool SetBoundaryCondition (const std::vector < double >& x, const char SolName[], double& value, const int facename, const double time)
{
  bool dirichlet = false;

  if (!strcmp (SolName, "HT")) {
    if (facename == 1 || facename == 2) {
      dirichlet = true;
      value = 0.;
    }
  }

  if (!strcmp (SolName, "T")) {
    if (facename == 1 || facename == 2) {
      dirichlet = true;
      value = 0.;
    }
  }

  return dirichlet;
}


void ETD (MultiLevelProblem& ml_prob, const unsigned & numberOfTimeSteps, Mat &JacScaled, Mat &phi1APetsc, Mat &Temp, Mat &Temp2);

void RK4 (MultiLevelProblem& ml_prob, const bool & implicitEuler, const unsigned & numberOfTimeSteps);

void build_phi1Av (const unsigned &CFL_pow, const std::vector < double > &NodeJac, const std::vector< double > &Res, Vec &y, Mat &A, Mat &phi1APetsc, 
                   Mat &Temp, Mat &Temp2);

void create_phi1A (const unsigned &CFL_pow, const std::vector < double > &NodeJac, std::vector < std::vector < double > > &phi1A, Mat &A, Mat &phi1APetsc, 
                   Mat &Temp, Mat &Temp2);


int main (int argc, char** args)
{

  SlepcInitialize (&argc, &args, PETSC_NULL, PETSC_NULL);

  // init Petsc-MPI communicator
  FemusInit mpinit (argc, args, MPI_COMM_WORLD);

  // define multilevel mesh
  MultiLevelMesh mlMsh;
  double scalingFactor = 1.;

  unsigned numberOfUniformLevels = 1;
  unsigned numberOfSelectiveLevels = 0;

  //unsigned nx = static_cast<unsigned> (floor (pow (2., 2) + 0.5));
  unsigned nx = static_cast<unsigned> (floor (pow (2.,/*5*/3) + 0.5));       //Grid cell size = 3.90625 m
  nx += 4;
  //nx += 16;

  double length = 10.; //2 * 1465700.;

  //mlMsh.GenerateCoarseBoxMesh ( nx, 0, 0, -length / 2, length / 2, 0., 0., 0., 0., EDGE3, "seventh" );
  mlMsh.GenerateCoarseBoxMesh (nx, 0, 0, 0, length, 0., 0., 0., 0., EDGE3, "seventh");

  //mlMsh.RefineMesh(numberOfUniformLevels + numberOfSelectiveLevels, numberOfUniformLevels , NULL);
  mlMsh.PrintInfo();

  // define the multilevel solution and attach the mlMsh object to it
  MultiLevelSolution mlSol (&mlMsh);

  for (unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf (name, "h%d", i);
    mlSol.AddSolution (name, DISCONTINOUS_POLYNOMIAL, ZERO, 2);
    sprintf (name, "v%d", i);
    mlSol.AddSolution (name, LAGRANGE, FIRST, 2);
    sprintf (name, "T%d", i);
    mlSol.AddSolution (name, DISCONTINOUS_POLYNOMIAL, ZERO, 2);
    sprintf (name, "HT%d", i);
    mlSol.AddSolution (name, DISCONTINOUS_POLYNOMIAL, ZERO, 2);
    sprintf (name, "S%d", i);
    mlSol.AddSolution (name, DISCONTINOUS_POLYNOMIAL, ZERO, 2);
    sprintf (name, "HS%d", i);
    mlSol.AddSolution (name, DISCONTINOUS_POLYNOMIAL, ZERO, 2);
    sprintf (name, "Z%d", i);
    mlSol.AddSolution (name, DISCONTINOUS_POLYNOMIAL, ZERO, 2);
    sprintf (name, "HZ%d", i);
    mlSol.AddSolution (name, DISCONTINOUS_POLYNOMIAL, ZERO, 2);
  }

  mlSol.AddSolution ("b", DISCONTINOUS_POLYNOMIAL, ZERO, 1, false);

  mlSol.AddSolution ("eta", DISCONTINOUS_POLYNOMIAL, ZERO, 1, false);

  mlSol.Initialize ("All");

  mlSol.Initialize ("v0", InitalValueV0);
  mlSol.Initialize ("v1", InitalValueV1);
  mlSol.Initialize ("v2", InitalValueV2);
  mlSol.Initialize ("v3", InitalValueV3);
  mlSol.Initialize ("v4", InitalValueV4);
  mlSol.Initialize ("v5", InitalValueV5);
  mlSol.Initialize ("v6", InitalValueV6);
  mlSol.Initialize ("v7", InitalValueV7);
  mlSol.Initialize ("v8", InitalValueV8);
  mlSol.Initialize ("v9", InitalValueV9);
  mlSol.Initialize ("v10", InitalValueV10);
  mlSol.Initialize ("v11", InitalValueV11);
  mlSol.Initialize ("v12", InitalValueV12);
  mlSol.Initialize ("v13", InitalValueV13);
  mlSol.Initialize ("v14", InitalValueV14);
  mlSol.Initialize ("v15", InitalValueV15);
  mlSol.Initialize ("v16", InitalValueV16);
  mlSol.Initialize ("v17", InitalValueV17);
  mlSol.Initialize ("v18", InitalValueV18);
  mlSol.Initialize ("v19", InitalValueV19);
  if (NumberOfLayers > 39) {
    mlSol.Initialize ("v20", InitalValueV20);
    mlSol.Initialize ("v21", InitalValueV21);
    mlSol.Initialize ("v22", InitalValueV22);
    mlSol.Initialize ("v23", InitalValueV23);
    mlSol.Initialize ("v24", InitalValueV24);
    mlSol.Initialize ("v25", InitalValueV25);
    mlSol.Initialize ("v26", InitalValueV26);
    mlSol.Initialize ("v27", InitalValueV27);
    mlSol.Initialize ("v28", InitalValueV28);
    mlSol.Initialize ("v29", InitalValueV29);
    mlSol.Initialize ("v30", InitalValueV30);
    mlSol.Initialize ("v31", InitalValueV31);
    mlSol.Initialize ("v32", InitalValueV32);
    mlSol.Initialize ("v33", InitalValueV33);
    mlSol.Initialize ("v34", InitalValueV34);
    mlSol.Initialize ("v35", InitalValueV35);
    mlSol.Initialize ("v36", InitalValueV36);
    mlSol.Initialize ("v37", InitalValueV37);
    mlSol.Initialize ("v38", InitalValueV38);
    mlSol.Initialize ("v39", InitalValueV39);
    if (NumberOfLayers > 79) {
      mlSol.Initialize ("v40", InitalValueV40);
      mlSol.Initialize ("v41", InitalValueV41);
      mlSol.Initialize ("v42", InitalValueV42);
      mlSol.Initialize ("v43", InitalValueV43);
      mlSol.Initialize ("v44", InitalValueV44);
      mlSol.Initialize ("v45", InitalValueV45);
      mlSol.Initialize ("v46", InitalValueV46);
      mlSol.Initialize ("v47", InitalValueV47);
      mlSol.Initialize ("v48", InitalValueV48);
      mlSol.Initialize ("v49", InitalValueV49);
      mlSol.Initialize ("v50", InitalValueV50);
      mlSol.Initialize ("v51", InitalValueV51);
      mlSol.Initialize ("v52", InitalValueV52);
      mlSol.Initialize ("v53", InitalValueV53);
      mlSol.Initialize ("v54", InitalValueV54);
      mlSol.Initialize ("v55", InitalValueV55);
      mlSol.Initialize ("v56", InitalValueV56);
      mlSol.Initialize ("v57", InitalValueV57);
      mlSol.Initialize ("v58", InitalValueV58);
      mlSol.Initialize ("v59", InitalValueV59);
      mlSol.Initialize ("v60", InitalValueV60);
      mlSol.Initialize ("v61", InitalValueV61);
      mlSol.Initialize ("v62", InitalValueV62);
      mlSol.Initialize ("v63", InitalValueV63);
      mlSol.Initialize ("v64", InitalValueV64);
      mlSol.Initialize ("v65", InitalValueV65);
      mlSol.Initialize ("v66", InitalValueV66);
      mlSol.Initialize ("v67", InitalValueV67);
      mlSol.Initialize ("v68", InitalValueV68);
      mlSol.Initialize ("v69", InitalValueV69);
      mlSol.Initialize ("v70", InitalValueV70);
      mlSol.Initialize ("v71", InitalValueV71);
      mlSol.Initialize ("v72", InitalValueV72);
      mlSol.Initialize ("v73", InitalValueV73);
      mlSol.Initialize ("v74", InitalValueV74);
      mlSol.Initialize ("v75", InitalValueV75);
      mlSol.Initialize ("v76", InitalValueV76);
      mlSol.Initialize ("v77", InitalValueV77);
      mlSol.Initialize ("v78", InitalValueV78);
      mlSol.Initialize ("v79", InitalValueV79);
      if (NumberOfLayers > 99) {
        mlSol.Initialize ("v80", InitalValueV80);
        mlSol.Initialize ("v81", InitalValueV81);
        mlSol.Initialize ("v82", InitalValueV82);
        mlSol.Initialize ("v83", InitalValueV83);
        mlSol.Initialize ("v84", InitalValueV84);
        mlSol.Initialize ("v85", InitalValueV85);
        mlSol.Initialize ("v86", InitalValueV86);
        mlSol.Initialize ("v87", InitalValueV87);
        mlSol.Initialize ("v88", InitalValueV88);
        mlSol.Initialize ("v89", InitalValueV89);
        mlSol.Initialize ("v90", InitalValueV90);
        mlSol.Initialize ("v91", InitalValueV91);
        mlSol.Initialize ("v92", InitalValueV92);
        mlSol.Initialize ("v93", InitalValueV93);
        mlSol.Initialize ("v94", InitalValueV94);
        mlSol.Initialize ("v95", InitalValueV95);
        mlSol.Initialize ("v96", InitalValueV96);
        mlSol.Initialize ("v97", InitalValueV97);
        mlSol.Initialize ("v98", InitalValueV98);
        mlSol.Initialize ("v99", InitalValueV99);
      }
    }
  }

  for (unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf (name, "h%d", i);
    mlSol.Initialize (name, InitalValueH);
  }

  for (unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf (name, "T%d", i);
    mlSol.Initialize (name, InitalValueT);
  }

  for (unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf (name, "S%d", i);
    mlSol.Initialize (name, InitalValueS);
  }

  for (unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    sprintf (name, "Z%d", i);
    mlSol.Initialize (name, InitalValueZ);
  }

  mlSol.Initialize ("b", InitalValueB);

  mlSol.AttachSetBoundaryConditionFunction (SetBoundaryCondition);
  mlSol.GenerateBdc ("All");

  MultiLevelProblem ml_prob (&mlSol);

  // ******* Add FEM system to the MultiLevel problem *******
  TransientLinearImplicitSystem& system = ml_prob.add_system < TransientLinearImplicitSystem > ("SWt");

  for (unsigned i = 0; i < NumberOfLayers; i++) {
    char name[10];
    //tracer 1
    sprintf (name, "HT%d", i);
    system.AddSolutionToSystemPDE (name);
    //tracer 2
    sprintf (name, "HS%d", i);
    system.AddSolutionToSystemPDE (name);
    //tracer 3
    sprintf (name, "HZ%d", i);
    system.AddSolutionToSystemPDE (name);
  }

  system.init();

  mlSol.SetWriter (VTK);
  std::vector<std::string> print_vars;
  print_vars.push_back ("All");
  //mlSol.GetWriter()->SetDebugOutput(true);
  mlSol.GetWriter()->Write (DEFAULT_OUTPUTDIR, "linear", print_vars, 0);

  unsigned numberOfTimeSteps = 500; //17h=1020 with dt=60, 17h=10200 with dt=6
  dt = 3.;
  bool implicitEuler = true; 
  
  Mat JacScaled;
  Mat phi1APetsc; 
  Mat Temp;
  Mat Temp2;
  MatCreateSeqDense(PETSC_COMM_SELF, 3.*NumberOfLayers, 3.*NumberOfLayers, NULL, &JacScaled);
  MatCreateSeqDense(PETSC_COMM_SELF, 3.*NumberOfLayers, 3.*NumberOfLayers, NULL, &phi1APetsc);
  MatCreateSeqDense(PETSC_COMM_SELF, 3.*NumberOfLayers, 3.*NumberOfLayers, NULL, &Temp);
  MatCreateSeqDense(PETSC_COMM_SELF, 3.*NumberOfLayers, 3.*NumberOfLayers, NULL, &Temp2);

  for (unsigned i = 0; i < numberOfTimeSteps; i++) {
    if (constant_jac == true) {
      assembly = (i == 0) ? true : false;
    }
    counter = i;
    system.CopySolutionToOldSolution();
    ETD (ml_prob, numberOfTimeSteps, JacScaled, phi1APetsc, Temp, Temp2);
    //RK4 ( ml_prob, implicitEuler, numberOfTimeSteps );
    mlSol.GetWriter()->Write (DEFAULT_OUTPUTDIR, "linear", print_vars, (i + 1) / 1);
    counter2++;
  }
  
  MatDestroy (&JacScaled);
  MatDestroy (&phi1APetsc);
  MatDestroy (&Temp);
  MatDestroy (&Temp2);

  std::cout << " TOTAL TIME:\t" <<
            static_cast<double> (clock() - start_time) / CLOCKS_PER_SEC << std::endl;
  return 0;
}


void ETD (MultiLevelProblem& ml_prob, const unsigned & numberOfTimeSteps, Mat &JacScaled, Mat &phi1APetsc, Mat &Temp, Mat &Temp2)
{

  const unsigned& NLayers = NumberOfLayers;

  adept::Stack& s = FemusInit::_adeptStack;

  if (assembly) {
    s.continue_recording();
  }

  else {
    s.pause_recording();
  }

  TransientLinearImplicitSystem* mlPdeSys  = &ml_prob.get_system<TransientLinearImplicitSystem> ("SWt");   // pointer to the linear implicit system named "Poisson"

  unsigned level = ml_prob._ml_msh->GetNumberOfLevels() - 1u;

  Mesh* msh = ml_prob._ml_msh->GetLevel (level);   // pointer to the mesh (level) object
  elem* el = msh->el;  // pointer to the elem object in msh (level)

  MultiLevelSolution* mlSol = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution* sol = ml_prob._ml_sol->GetSolutionLevel (level);   // pointer to the solution (level) object

  LinearEquationSolver* pdeSys = mlPdeSys->_LinSolver[level]; // pointer to the equation (level) object

  NumericVector* EPS = pdeSys->_EPS; // pointer to the global residual vector object in pdeSys (level)

  const unsigned  dim = msh->GetDimension(); // get the domain dimension of the problem

  unsigned    iproc = msh->processor_id(); // get the process_id (for parallel computation)
  unsigned    nprocs = msh->n_processors(); // get the process_id (for parallel computation)

  //solution variable
  std::vector < unsigned > solIndexh (NLayers);
  
  std::vector < unsigned > solIndexv (NLayers);

  std::vector < unsigned > solIndexHT (NLayers);
  std::vector < unsigned > solPdeIndexHT (NLayers);
  std::vector < unsigned > solIndexT (NLayers);

  std::vector < unsigned > solIndexHS (NLayers);
  std::vector < unsigned > solPdeIndexHS (NLayers);
  std::vector < unsigned > solIndexS (NLayers);

  std::vector < unsigned > solIndexHZ (NLayers);
  std::vector < unsigned > solPdeIndexHZ (NLayers);
  std::vector < unsigned > solIndexZ (NLayers);

  vector< int > l2GMapRow; // local to global mapping
  vector< int > l2GMapRowHS; // local to global mapping
  vector< int > l2GMapRowHZ; // local to global mapping

  for (unsigned i = 0; i < NLayers; i++) {
    char name[10];
    sprintf (name, "h%d", i);
    solIndexh[i] = mlSol->GetIndex (name);   // get the position of "hi" in the sol object

    sprintf (name, "v%d", i);
    solIndexv[i] = mlSol->GetIndex (name);   // get the position of "vi" in the sol object

    sprintf (name, "HT%d", i);
    solIndexHT[i] = mlSol->GetIndex (name);   // get the position of "HTi" in the sol object
    solPdeIndexHT[i] = mlPdeSys->GetSolPdeIndex (name);   // get the position of "HTi" in the pdeSys object
    sprintf (name, "T%d", i);
    solIndexT[i] = mlSol->GetIndex (name);   // get the position of "Ti" in the sol object

    sprintf (name, "HS%d", i);
    solIndexHS[i] = mlSol->GetIndex (name);   // get the position of "HSi" in the sol object
    solPdeIndexHS[i] = mlPdeSys->GetSolPdeIndex (name);   // get the position of "HSi" in the pdeSys object
    sprintf (name, "S%d", i);
    solIndexS[i] = mlSol->GetIndex (name);   // get the position of "Si" in the sol object

    sprintf (name, "HZ%d", i);
    solIndexHZ[i] = mlSol->GetIndex (name);   // get the position of "HZi" in the sol object
    solPdeIndexHZ[i] = mlPdeSys->GetSolPdeIndex (name);   // get the position of "HZi" in the pdeSys object
    sprintf (name, "Z%d", i);
    solIndexZ[i] = mlSol->GetIndex (name);   // get the position of "Zi" in the sol object

  }

  unsigned solTypeh = mlSol->GetSolutionType (solIndexh[0]);   // get the finite element type for "hi"
  unsigned solTypev = mlSol->GetSolutionType (solIndexv[0]);   // get the finite element type for "vi"
  unsigned solTypeHT = mlSol->GetSolutionType (solIndexHT[0]);   // get the finite element type for "Ti"
  unsigned solTypeHS = mlSol->GetSolutionType (solIndexHS[0]);   // get the finite element type for "Si"
  unsigned solTypeHZ = mlSol->GetSolutionType (solIndexHZ[0]);   // get the finite element type for "Zi"

  for (unsigned k = 0; k < NumberOfLayers; k++) {
    for (unsigned i =  msh->_dofOffset[solTypeHT][iproc]; i <  msh->_dofOffset[solTypeHT][iproc + 1]; i++) {
      //double valueT = (*sol->_SolOld[solIndexT[k]]) (i);
      double valueT = (*sol->_Sol[solIndexT[k]]) (i);
      double valueS = (*sol->_Sol[solIndexS[k]]) (i);
      double valueZ = (*sol->_Sol[solIndexZ[k]]) (i);
      double valueH = (*sol->_Sol[solIndexh[k]]) (i);

      double valueHT = valueT * valueH;
      double valueHS = valueS * valueH;
      double valueHZ = valueZ * valueH;

      sol->_Sol[solIndexHT[k]]->set (i, valueHT);
      sol->_Sol[solIndexHS[k]]->set (i, valueHS);
      sol->_Sol[solIndexHZ[k]]->set (i, valueHZ);
    }

    sol->_Sol[solIndexHT[k]]->close();
    sol->_Sol[solIndexHS[k]]->close();
    sol->_Sol[solIndexHZ[k]]->close();
  }

  std::vector < double > maxW (NLayers, -1.e6);
  maxW[0] = 0.;

  EPS->zero();

  unsigned start = msh->_dofOffset[solTypeHT][iproc];
  unsigned end = msh->_dofOffset[solTypeHT][iproc + 1];

  std::vector < double > N (NLayers * (end - start), 0.);
  std::vector < double > N_S (NLayers * (end - start), 0.);
  std::vector < double > N_Z (NLayers * (end - start), 0.);

  std::vector < std::vector < double > > Jac (end - start);

  if (constant_jac) {
    if (counter == 0) phi1A.resize (end - start);
  }

  else if (phi_once) phi1A.resize (end - start);

//   std::vector <double> EPS_HS ( (end - start) * NLayers , 0.);
//   std::vector <double> EPS_HZ ( (end - start) * NLayers , 0.);
  
  for (unsigned i =  start; i <  end; i++) {

    vector < double > solhm (NLayers);
    vector < double > solh (NLayers);   // local coordinates
    vector < double > solhp (NLayers);
    vector < double > solvm (NLayers);   // local coordinates
    vector < double > solvp (NLayers);   // local coordinates
    vector < adept::adouble > solHTm (NLayers);   // local coordinates
    vector < adept::adouble > solHT (NLayers);   // local coordinates
    vector < adept::adouble > solHTp (NLayers);   // local coordinates
    vector < adept::adouble > solHSm (NLayers);   // local coordinates
    vector < adept::adouble > solHS (NLayers);   // local coordinates
    vector < adept::adouble > solHSp (NLayers);   // local coordinates
    vector < adept::adouble > solHZm (NLayers);   // local coordinates
    vector < adept::adouble > solHZ (NLayers);   // local coordinates
    vector < adept::adouble > solHZp (NLayers);   // local coordinates

    vector < adept::adouble > solHTmm (NLayers);   // local coordinates
    vector < adept::adouble > solHTpp (NLayers);   // local coordinates

    vector< adept::adouble > aResHT (NLayers);
    vector< adept::adouble > aResHS (NLayers, 0.);
    vector< adept::adouble > aResHZ (NLayers, 0.);
    vector< double > aResHTLili (NLayers, 0.);
    vector< double > aResHSLili (NLayers, 0.);
    vector< double > aResHZLili (NLayers, 0.);

    unsigned bc1 = (i == start) ? 0 : 1;
    unsigned bc2 = (i == end - 1) ? 0 : 1;

    unsigned bc3 = (i > start + 1) ? 1 : 0;
    unsigned bc4 = (i < end - 2) ? 1 : 0;

    l2GMapRow.resize (NLayers);
    l2GMapRowHS.resize (NLayers);
    l2GMapRowHZ.resize (NLayers);
    
    std::fill (aResHT.begin(), aResHT.end(), 0);   //set aResHT to zero
    std::fill (aResHS.begin(), aResHS.end(), 0);   //set aResHS to zero
    std::fill (aResHZ.begin(), aResHZ.end(), 0);   //set aResHZ to zero

    for (unsigned j = 0; j < NLayers; j++) {

      solh[j] = (*sol->_Sol[solIndexh[j]]) (i);
      solHT[j] = (*sol->_Sol[solIndexHT[j]]) (i);
      l2GMapRow[/*NLayers +*/ j] = pdeSys->GetSystemDof (solIndexHT[j], solPdeIndexHT[j], 0, i);
      solHS[j] = (*sol->_Sol[solIndexHS[j]]) (i);
      l2GMapRowHS[/*NLayers +*/ j] = pdeSys->GetSystemDof (solIndexHS[j], solPdeIndexHS[j], 0, i);
      solHZ[j] = (*sol->_Sol[solIndexHZ[j]]) (i);
      l2GMapRowHZ[/*NLayers +*/ j] = pdeSys->GetSystemDof (solIndexHZ[j], solPdeIndexHZ[j], 0, i);

      solvm[j] = (*sol->_Sol[solIndexv[j]]) (i);
      solvp[j] = (*sol->_Sol[solIndexv[j]]) (i + 1);

      if (i > start) {
        solhm[j] = (*sol->_Sol[solIndexh[j]]) (i - 1);
        solHTm[j] = (*sol->_Sol[solIndexHT[j]]) (i - 1);
        solHSm[j] = (*sol->_Sol[solIndexHS[j]]) (i - 1);
        solHZm[j] = (*sol->_Sol[solIndexHZ[j]]) (i - 1);
      }

      if (i < end - 1) {
        solhp[j] = (*sol->_Sol[solIndexh[j]]) (i + 1);
        solHTp[j] = (*sol->_Sol[solIndexHT[j]]) (i + 1);
        solHSp[j] = (*sol->_Sol[solIndexHS[j]]) (i + 1);
        solHZp[j] = (*sol->_Sol[solIndexHZ[j]]) (i + 1);
      }

//       if ( i > start + 1 ) {
//         solHTmm[j] = ( *sol->_Sol[solIndexHT[j]] ) ( i - 2 );
//       }
//
//       if ( i < end - 2 ) {
//         solHTpp[j] = ( *sol->_Sol[solIndexHT[j]] ) ( i + 2 );
//       }

    }

    if (assembly) {
      s.new_recording();
    }

    vector < double > x (2);   // local coordinates

    for (unsigned j = 0; j < 2; j++) {
      unsigned xDof  = msh->GetSolutionDof (j, i, 2);   // global to global mapping between coordinates node and coordinate dof
      x[j] = (*msh->_topology->_Sol[0]) (xDof);     // global extraction and local storage for the element coordinates
    }

    double dx = x[1] - x[0];

    double b = 10.; //( H_shelf + H_0 / 2 * (1 + tanh(hh / phi)) );

    std::vector < double > w (NLayers + 1, 0.);

    std::vector < double > zTop (NLayers);
    zTop[0] = 0;

    for (unsigned k = 1; k < NLayers; k++) {
      zTop[k] = zTop[k - 1] - solh[k];
    }

    std::vector < double > psi2 (NLayers);

    for (unsigned k = 0; k < NLayers; k++) {
      //psi2[k] = ( - ( zMid[k] + 10 ) * zMid[k] ) / 25;
      psi2[k] = 1. - (zTop[k] + 5.) * (zTop[k] + 5.) / (25.);
    }

    double xmid = 0.5 * (x[1] + x[0]);

    for (unsigned k = NLayers; k > 1; k--) {
      //w[k - 1] = - (- 16. / (pow (20., 16)) * pow ( (xmid - 20.), 15)) * psi2[k - 1]; //40x10 rectangle test
      //w[k - 1] = - ( - (2./25.) * (xmid - 5.) ) * psi2[k - 1];
      w[k - 1] = - (- 4. / 625.* (xmid - 5) * (xmid - 5) * (xmid - 5)) * psi2[k - 1]; //10x10 box test
      //std::cout << w[k-1] << " ";

      //w[k - 1] = ( ( 10. - 2. * xmid ) / 25. ) * psi2[k - 1];
      if (maxW[k - 1] < w[k - 1]) {
        maxW[k - 1] = w[k - 1];
      }
    }

    //std::cout << std::endl;

    for (unsigned k = 0; k < NLayers; k++) {

      //BEGIN FIRST ORDER HORIZONTAL ADVECTION
      if (i > start) {
        if (solvm[k] > 0) {
          //tracer 1
          aResHT[k] += solHTm[k].value() * solvm[k] / dx;
          aResHTLili[k] += solHTm[k].value() * solvm[k] / dx;
          //tracer 2
          aResHS[k] += solHSm[k].value() * solvm[k] / dx;
          aResHSLili[k] += solHSm[k].value() * solvm[k] / dx;
          //tracer 3
          aResHZ[k] += solHZm[k].value() * solvm[k] / dx;
          aResHZLili[k] += solHZm[k].value() * solvm[k] / dx;
        }

        else {
          //tracer 1
          aResHT[k] += solHT[k].value() * solvm[k] / dx;
          aResHTLili[k] += solHT[k].value() * solvm[k] / dx;
          //tracer 2
          aResHS[k] += solHS[k].value() * solvm[k] / dx;
          aResHSLili[k] += solHS[k].value() * solvm[k] / dx;
          //tracer 3
          aResHZ[k] += solHZ[k].value() * solvm[k] / dx;
          aResHZLili[k] += solHZ[k].value() * solvm[k] / dx;
        }
      }

      if (i < end - 1) {
        if (solvp[k] > 0) {
          //tracer 1
          aResHT[k] -= solHT[k].value() * solvp[k] / dx;
          aResHTLili[k] -= solHT[k].value() * solvp[k] / dx;
          //tracer 2
          aResHS[k] -= solHS[k].value() * solvp[k] / dx;
          aResHSLili[k] -= solHS[k].value() * solvp[k] / dx;
          //tracer 3
          aResHZ[k] -= solHZ[k].value() * solvp[k] / dx;
          aResHZLili[k] -= solHZ[k].value() * solvp[k] / dx;
        }

        else {
          //tracer 1
          aResHT[k] -= solHTp[k].value() * solvp[k] / dx;
          aResHTLili[k] -= solHTp[k].value() * solvp[k] / dx;
          //tracer 2
          aResHS[k] -= solHSp[k].value() * solvp[k] / dx;
          aResHSLili[k] -= solHSp[k].value() * solvp[k] / dx;
          //tracer 3
          aResHZ[k] -= solHZp[k].value() * solvp[k] / dx;
          aResHZLili[k] -= solHZp[k].value() * solvp[k] / dx;
        }
      }

      //END FIRST ORDER HORIZONTAL ADVECTION

      //BEGIN THIRD ORDER HORIZONTAL ADVECTION
//       if ( i > start ) {
//         aResHT[k] += 0.5 * ( solHTm[k].value() + solHT[k].value() ) * solvm[k] / dx;
//         if ( solvm[k] > 0 ) {
//           if ( i > start + 1 ) {
//             aResHT[k] += - 1. / 6. * ( solHT[k].value() - 2.*solHTm[k].value() + solHTmm[k].value() ) * solvm[k]  / dx;
//           }
//         }
//         else {
//           if ( i < end - 1 ) {
//             aResHT[k] += - 1. / 6. * ( solHTp[k].value() - 2.*solHT[k].value() + solHTm[k].value() ) * solvm[k]  / dx;
//           }
//         }
//       }
//       if ( i < end - 1 ) {
//         aResHT[k] -= 0.5 * ( solHTp[k].value() + solHT[k].value() ) * solvp[k] / dx;
//         if ( solvp[k] > 0 ) {
//           if (i > start) {
//             aResHT[k] -= - 1. / 6. * ( solHTp[k].value() - 2.*solHT[k].value() + solHTm[k].value() ) * solvp[k]  / dx;
//           }
//         }
//         else {
//           if ( i < end - 2 ) {
//             aResHT[k] -= - 1. / 6. * ( solHTpp[k].value() - 2.*solHTp[k].value() + solHT[k].value() ) * solvp[k]  / dx;
//           }
//         }
//       }
      //END THIRD ORDER HORIZONTAL ADVECTION

      //BEGIN VERTICAL ADVECTION
      if (k < NLayers - 1) {  //bottom
        //aResHT[k] += w[k + 1] * 0.5 * ( solHT[k] / solh[k] + solHT[k + 1] / solh[k + 1] ); //central difference
        if (w[k + 1] > 0) {
          aResHT[k] += w[k + 1] * (solHT[k + 1] / solh[k + 1]); //tracer 1
          aResHS[k] += w[k + 1] * (solHS[k + 1] / solh[k + 1]); //tracer 2
          aResHZ[k] += w[k + 1] * (solHZ[k + 1] / solh[k + 1]); //tracer 3
        }

        else {
          aResHT[k] += w[k + 1] * (solHT[k] / solh[k]); //tracer 1
          aResHS[k] += w[k + 1] * (solHS[k] / solh[k]); //tracer 2
          aResHZ[k] += w[k + 1] * (solHZ[k] / solh[k]); //tracer 3
        }
      }

      if (k > 0) {  //top
        //aResHT[k] -= w[k] * 0.5 * ( solHT[k - 1] / solh[k - 1] + solHT[k] / solh[k] ); //central difference
        if (w[k] > 0) {
          aResHT[k] -= w[k] * (solHT[k] / solh[k]); //tracer 1
          aResHS[k] -= w[k] * (solHS[k] / solh[k]); //tracer 2
          aResHZ[k] -= w[k] * (solHZ[k] / solh[k]); //tracer 3
        }

        else {
          aResHT[k] -= w[k] * (solHT[k - 1] / solh[k - 1]); //tracer 1
          aResHS[k] -= w[k] * (solHS[k - 1] / solh[k - 1]); //tracer 2
          aResHZ[k] -= w[k] * (solHZ[k - 1] / solh[k - 1]); //tracer 3
        }
      }

      //END VERTICAL ADVECTION

      adept::adouble deltaZt_T = 0.;
      adept::adouble deltaZb_T = 0.;
      adept::adouble deltaZt_S = 0.;
      adept::adouble deltaZb_S = 0.;
      adept::adouble deltaZt_Z = 0.;
      adept::adouble deltaZb_Z = 0.;
      adept::adouble ht = 0.;
      adept::adouble hb = 0.;

      if (k > 0) {
        //ht = ( solhm[k - 1] + solhm[k] + solhp[k - 1] + solhp[k] ) / 4.;
        ht = (solh[k - 1] + solh[k]) / 2.;
        deltaZt_T = (solHT[k - 1] / solh[k - 1] - solHT[k] / solh[k]) / ht; //tracer 1
        deltaZt_S = (solHS[k - 1] / solh[k - 1] - solHS[k] / solh[k]) / ht; //tracer 2
        deltaZt_Z = (solHZ[k - 1] / solh[k - 1] - solHZ[k] / solh[k]) / ht; //tracer 3
      }

      else {
        //ht = 0.5 * ( solhm[k] + solhp[k] );
        ht = solh[k];
        deltaZt_T = 0.* (0. - solHT[k]) / ht;
        deltaZt_S = 0.* (0. - solHS[k]) / ht;
        deltaZt_Z = 0.* (0. - solHZ[k]) / ht;
      }

      if (k < NLayers - 1) {
        //hb = ( solhm[k] + solhm[k + 1] + solhp[k] + solhp[k + 1] ) / 4.;
        hb = (solh[k] + solh[k + 1]) / 2.;
        deltaZb_T = (solHT[k] / solh[k] - solHT[k + 1] / solh[k + 1]) / hb;
        deltaZb_S = (solHS[k] / solh[k] - solHS[k + 1] / solh[k + 1]) / hb;
        deltaZb_Z = (solHZ[k] / solh[k] - solHZ[k + 1] / solh[k + 1]) / hb;
      }

      else {
        //hb = 0.5 * ( solhm[k] + solhp[k] );
        hb = solh[k];
        deltaZb_T = 0.* (solHT[k] - 0.) / hb;
        deltaZb_S = 0.* (solHS[k] - 0.) / hb;
        deltaZb_Z = 0.* (solHZ[k] - 0.) / hb;
      }

      //std::cout<<"AAAAAAAAAAAAAAAAAAAAAAAAAA"<<deltaZt - deltaZb<<std::endl;

      aResHT[k] += solh[k] * k_v * (deltaZt_T - deltaZb_T) / ( (ht + hb) / 2.); // vertical diffusion tracer 1
      aResHS[k] += solh[k] * k_v * (deltaZt_S - deltaZb_S) / ( (ht + hb) / 2.); // vertical diffusion tracer 2
      aResHZ[k] += solh[k] * k_v * (deltaZt_Z - deltaZb_Z) / ( (ht + hb) / 2.); // vertical diffusion tracer 3

      //BEGIN HORIZONTAL DIFFUSION
      if (i > start) {
        //tracer 1
        aResHT[k] += k_h * (0.5 * (solhm[k] + solh[k])) * (solHTm[k].value() / solhm[k] - solHT[k].value() / solh[k]) / (dx * dx);
        aResHTLili[k] += k_h * (0.5 * (solhm[k] + solh[k])) * (solHTm[k].value() / solhm[k] - solHT[k].value() / solh[k]) / (dx * dx);
        //tracer 2
        aResHS[k] += k_h * (0.5 * (solhm[k] + solh[k])) * (solHSm[k].value() / solhm[k] - solHS[k].value() / solh[k]) / (dx * dx);
        aResHSLili[k] += k_h * (0.5 * (solhm[k] + solh[k])) * (solHSm[k].value() / solhm[k] - solHS[k].value() / solh[k]) / (dx * dx);
        //tracer 3
        aResHZ[k] += k_h * (0.5 * (solhm[k] + solh[k])) * (solHZm[k].value() / solhm[k] - solHZ[k].value() / solh[k]) / (dx * dx);
        aResHZLili[k] += k_h * (0.5 * (solhm[k] + solh[k])) * (solHZm[k].value() / solhm[k] - solHZ[k].value() / solh[k]) / (dx * dx);
      }

      if (i < end - 1) {
        //tracer 1
        aResHT[k] += k_h * (0.5 * (solhp[k] + solh[k])) * (solHTp[k].value() / solhp[k] - solHT[k].value() / solh[k]) / (dx * dx);
        aResHTLili[k] += k_h * (0.5 * (solhp[k] + solh[k])) * (solHTp[k].value() / solhp[k] - solHT[k].value() / solh[k]) / (dx * dx);
        //tracer 2
        aResHS[k] += k_h * (0.5 * (solhp[k] + solh[k])) * (solHSp[k].value() / solhp[k] - solHS[k].value() / solh[k]) / (dx * dx);
        aResHSLili[k] += k_h * (0.5 * (solhp[k] + solh[k])) * (solHSp[k].value() / solhp[k] - solHS[k].value() / solh[k]) / (dx * dx);
        //tracer 3
        aResHZ[k] += k_h * (0.5 * (solhp[k] + solh[k])) * (solHZp[k].value() / solhp[k] - solHZ[k].value() / solh[k]) / (dx * dx);
        aResHZLili[k] += k_h * (0.5 * (solhp[k] + solh[k])) * (solHZp[k].value() / solhp[k] - solHZ[k].value() / solh[k]) / (dx * dx);
      }

      //END HORIZONTAL DIFFUSION

      //BEGIN FORCING TERM
      
      //radioactive tracer 1
      //aResHT[k] += - solHT[k].value() * lambda;
      //aResHTLili[k] += - solHT[k].value() * lambda;
      
      double dimensione = 1./86400.; //second^{-1}
      //double dimensione = 1./1400.; //minute^{-1}
      double aa = 2.0 * dimensione;
      double bb = 0.5 * dimensione;
      double cc = 0.5; //TODO
      double rr = 0.1 * dimensione; //1. * dimensione;
      double kk = 0.2; //TODO
      double delta = 0.35 * dimensione; //0.35 * dimensione;
      double delta1 = 0.15 * dimensione; //0.15 * dimensione;
      double eps = 0.2 * dimensione;
      
      //SPLITTING 1
//       //tracer 1
//       aResHT[k] += aa * solHT[k] * (solHS[k].value() / (kk + solHS[k].value())) - bb * solHZ[k].value() 
//                  + bb * exp(- cc * solHT[k].value()) * solHZ[k].value() - rr * solHT[k];
//       aResHTLili[k] += - bb * solHZ[k].value() + bb * exp(- cc * solHT[k].value()) * solHZ[k].value();
//       //tracer 3
//       aResHZ[k] += solHZ[k] * (delta * (1. - exp(- cc * solHT[k].value())) - eps);
//       //tracer 2
//       aResHS[k] += - aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) + rr * solHT[k].value() + eps * solHZ[k].value()  
//                 + delta1 * (1. - exp(- cc * solHT[k].value())) * solHZ[k].value();
//       aResHSLili[k] += - aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) + rr * solHT[k].value() 
//                        + eps * solHZ[k].value() + delta1 * (1. - exp(- cc * solHT[k].value())) * solHZ[k].value();
                
      //SPLITTING 2     
      //tracer 1 
      aResHT[k] += aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) - bb * solHZ[k] 
                   + bb * exp(- cc * solHT[k].value()) * solHZ[k].value() - rr * solHT[k];
      aResHTLili[k] += aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) 
                       + bb * exp(- cc * solHT[k].value()) * solHZ[k].value();              
      //tracer 2
      aResHZ[k] += - solHZ[k] * eps + solHZ[k] * delta - solHZ[k].value() * delta * exp(- cc * solHT[k].value());
      aResHZLili[k] += - solHZ[k].value() * delta * exp(- cc * solHT[k].value());
      //tracer 3
      aResHS[k] += - aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) + rr * solHT[k] + eps * solHZ[k]  
                     + delta1 * solHZ[k] - delta1 * exp(- cc * solHT[k].value()) * solHZ[k].value();
      aResHSLili[k] += - aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) 
                       - delta1 * exp(- cc * solHT[k].value()) * solHZ[k].value();
                       
      //SPLITTING 3  
//       //tracer 1
//       aResHT[k] += aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) - bb * solHZ[k].value() 
//                    + bb * exp(- cc * solHT[k].value()) * solHZ[k].value() - rr * solHT[k];
//       aResHTLili[k] += aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) - bb * solHZ[k].value() 
//                        + bb * exp(- cc * solHT[k].value()) * solHZ[k].value();
//       //tracer 3
//       aResHZ[k] += solHZ[k] * delta - solHZ[k] * eps - solHZ[k].value() * delta * exp(- cc * solHT[k].value());
//       aResHZLili[k] += - solHZ[k].value() * delta * exp(- cc * solHT[k].value());
//       //tracer 2
//       aResHS[k] += - aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) + rr * solHT[k].value() + eps * solHZ[k].value()  
//                    + delta1 * (1. - exp(- cc * solHT[k].value())) * solHZ[k].value();
//       aResHSLili[k] += - aa * solHT[k].value() * (solHS[k].value() / (kk + solHS[k].value())) + rr * solHT[k].value() 
//                        + eps * solHZ[k].value() + delta1 * (1. - exp(- cc * solHT[k].value())) * solHZ[k].value();
      
      //SPLITTING 4     
//       //tracer 1 
//       aResHT[k] += aa * solHT[k] * (solHS[k].value() / (kk + solHS[k].value())) - bb * solHZ[k] 
//                    + bb * exp(- cc * solHT[k].value()) * solHZ[k] - rr * solHT[k];             
//       //tracer 2
//       aResHZ[k] += - solHZ[k] * eps + solHZ[k] * delta - solHZ[k] * delta * exp(- cc * solHT[k].value());
//       //tracer 3
//       aResHS[k] += - aa * solHT[k] * (solHS[k].value() / (kk + solHS[k].value())) + rr * solHT[k] + eps * solHZ[k]  
//                      + delta1 * solHZ[k] - delta1 * exp(- cc * solHT[k].value()) * solHZ[k];
      
      //END FORCING TERM
      
    }

    vector< double > Res (3*NLayers);   // local redidual vector

    for (unsigned k = 0; k < NLayers; k++) {
      Res[k] =  aResHT[k].value();
      Res[k + NLayers] =  aResHS[k].value();
      Res[k + 2*NLayers] =  aResHZ[k].value();

      if (twostage) {
        N[i * NLayers + k] = aResHTLili[k];
        N_S[i * NLayers + k] = aResHSLili[k];
        N_Z[i * NLayers + k] = aResHZLili[k];
      }
    }

    if (assembly) {
      s.dependent (&aResHT[0], NLayers);
      s.dependent (&aResHS[0], NLayers);
      s.dependent (&aResHZ[0], NLayers);

      // define the independent variables
      s.independent (&solHT[0], NLayers);
      s.independent (&solHS[0], NLayers);
      s.independent (&solHZ[0], NLayers);

      //Jac[i].resize (NLayers * NLayers);
      Jac[i].resize (3*NLayers * 3*NLayers);
      s.jacobian (&Jac[i][0], true);
      
      s.clear_independents();
      s.clear_dependents();
    }

    //building objects for computing ∆tφ1(∆tJn)F(Tn)
    Vec y1/*, y2, y3*/;

    //tracer 1
    VecCreate (PETSC_COMM_WORLD, &y1);
    VecSetSizes (y1, PETSC_DECIDE, 3*NLayers);
    VecSetFromOptions (y1);
//     //tracer 2
//     VecCreate (PETSC_COMM_WORLD, &y2);
//     VecSetSizes (y2, PETSC_DECIDE, NLayers);
//     VecSetFromOptions (y2);
//     //tracer 3
//     VecCreate (PETSC_COMM_WORLD, &y3);
//     VecSetSizes (y3, PETSC_DECIDE, NLayers);
//     VecSetFromOptions (y3);

    if (slepc) {
      Mat A;
      Vec v1/*, v2, v3*/;

      MatCreate (MPI_COMM_SELF, &A);
      MatSetSizes (A, 3*NLayers, 3*NLayers, 3*NLayers, 3*NLayers);
      MatSetType (A, MATSEQDENSE);
      MatSeqDenseSetPreallocation (A, &Jac[i][0]);
      MatAssemblyBegin (A, MAT_FINAL_ASSEMBLY);
      MatAssemblyEnd (A, MAT_FINAL_ASSEMBLY);
      MatTranspose (A, MAT_INPLACE_MATRIX, &A);
      //MatView ( A,PETSC_VIEWER_STDOUT_WORLD );

      //tracer 1
      VecCreateSeqWithArray (MPI_COMM_SELF, 1, 3*NLayers, &Res[0], &v1);
      VecAssemblyBegin (v1);
      VecAssemblyEnd (v1);
      //VecView(v1,PETSC_VIEWER_STDOUT_WORLD);
//       //tracer 2
//       VecCreateSeqWithArray (MPI_COMM_SELF, 1, NLayers, &ResS[0], &v2);
//       VecAssemblyBegin (v2);
//       VecAssemblyEnd (v2);
//       //VecView(v2,PETSC_VIEWER_STDOUT_WORLD);
//       //tracer 3
//       VecCreateSeqWithArray (MPI_COMM_SELF, 1, NLayers, &ResZ[0], &v3);
//       VecAssemblyBegin (v3);
//       VecAssemblyEnd (v3);
//       //VecView(v3,PETSC_VIEWER_STDOUT_WORLD);

      MFN mfn;
      FN f;
      MFNConvergedReason reason;

      //BEGIN condition number of A
//             SVD svd;
//             PetscReal sigma_1,sigma_n;
//             PetscInt nconv1,nconv2;
//
//             SVDCreate(PETSC_COMM_WORLD,&svd);
//             SVDSetOperator(svd,A);
//             SVDSetFromOptions(svd);
//             SVDSetDimensions(svd,1,PETSC_DEFAULT,PETSC_DEFAULT);
//             //First request a singular value from one end of the spectrum
//             SVDSetWhichSingularTriplets(svd,SVD_LARGEST);
//             SVDSolve(svd);
//             //Get number of converged singular values
//             SVDGetConverged(svd,&nconv1);
//             //Get converged singular values: largest singular value is stored in sigma_1.
//             if (nconv1 > 0) {
//               SVDGetSingularTriplet(svd,0,&sigma_1,NULL,NULL);
//             }
//             else {
//               PetscPrintf(PETSC_COMM_WORLD," Unable to compute large singular value!\n\n");
//             }
//             //Request a singular value from the other end of the spectrum
//             SVDSetWhichSingularTriplets(svd,SVD_SMALLEST);
//             SVDSolve(svd);
//             //Get number of converged singular triplets
//             SVDGetConverged(svd,&nconv2);
//             //Get converged singular values: smallest singular value is stored in sigma_n.
//             if (nconv2 > 0) {
//               SVDGetSingularTriplet(svd,0,&sigma_n,NULL,NULL);
//             }
//             else {
//               PetscPrintf(PETSC_COMM_WORLD," Unable to compute small singular value!\n\n");
//             }
//             //Display solution and clean up
//             if (nconv1 > 0 && nconv2 > 0) {
//               PetscPrintf(PETSC_COMM_WORLD," Computed singular values: sigma_1=%.4f, sigma_n=%.4f\n",(double)sigma_1,(double)sigma_n);
//               PetscPrintf(PETSC_COMM_WORLD," Estimated condition number: sigma_1/sigma_n=%.4f\n\n",(double)(sigma_1/sigma_n));
//             }
//             //Free work space
//             SVDDestroy(&svd);
      //END

      PetscReal tol;
      PetscInt ncv, maxit, its;

      MFNCreate (PETSC_COMM_WORLD, &mfn);

      MFNSetOperator (mfn, A);
      MFNGetFN (mfn, &f);

      FNPhiSetIndex (f, 1);
      FNSetType (f, FNPHI);
      //FNView(f,PETSC_VIEWER_STDOUT_WORLD);

      FNSetScale (f, dt, dt);

      if (NLayers==100){
        MFNSetDimensions (mfn, 10); 
      }
      else {
        MFNSetDimensions (mfn, 4);
      }

      if (i == start || i == start + 1 || i == start + 2 || i == end - 3 || i == end - 2 || i == end - 1) {
        if (NLayers==100){
         MFNSetDimensions (mfn, 20); 
        }
        else {
          MFNSetDimensions (mfn, 8);
        }
      }

      tol = 1e-6;
      MFNSetTolerances (mfn, tol, PETSC_DEFAULT);
      MFNSetFromOptions (mfn);

      clock_t etd_time;

      if (counter2 == 0) {
        etd_time = clock();
//                 std::cout<< "--------------------- v "<< i << " --------------------- " <<std::endl;
//                 for ( PetscInt kk=0; kk<NLayers; kk++ ) {
//                     PetscScalar valueHT = 0.;
//                     VecGetValues ( v, 1, &kk, &valueHT );
//                     std::cout<< valueHT << std::endl;
//                 }
      }

      //BEGIN phi1A print
//         PetscInt j;
//         for (j=0; j<NLayers; j++) {
//           Vec a, a1;
//           VecCreate(PETSC_COMM_WORLD,&a);
//           VecSetSizes(a,PETSC_DECIDE,NLayers);
//           VecSetFromOptions(a);
//           VecSet(a,0.0);
//           PetscScalar one=1.0;
//           VecSetValues(a,1,&j,&one,INSERT_VALUES);
//           VecAssemblyBegin(a);
//           VecAssemblyEnd(a);
//           VecView(a,PETSC_VIEWER_STDOUT_WORLD);
//           VecDuplicate (a, &a1);
//           MFNSolve (mfn, a, a1);
//           VecScale (a1, 1./dt);
//           VecView(a1,PETSC_VIEWER_STDOUT_WORLD);
//
//           VecDestroy (&a);
//           VecDestroy (&a1);
//         }
      //END

      MFNSolve (mfn, v1, y1);
      //MFNSolve (mfn, v2, y2);
      //MFNSolve (mfn, v3, y3);
      MFNGetConvergedReason (mfn, &reason);
      //VecView(y1,PETSC_VIEWER_STDOUT_WORLD);
      //VecView(y2,PETSC_VIEWER_STDOUT_WORLD);
      //VecView(y3,PETSC_VIEWER_STDOUT_WORLD);

      if (reason < 0) std::cout << "Solver did not converge" << std::endl;

      if (counter2 == 0) {
        std::cout << " ETD TIME :\t" << static_cast<double> (clock() - etd_time) / CLOCKS_PER_SEC << std::endl;
//                // std::cout<< "--------------------- y --------------------- " <<std::endl;
//                 for ( PetscInt kk=0; kk<NLayers; kk++ ) {
//                     PetscScalar valueHT = 0.;
//                     VecGetValues ( y, 1, &kk, &valueHT );
//                    std::cout<< valueHT << std::endl;
//                 }
//                 //std::cout<< "--------------------- END --------------------- " <<std::endl;
      }

      //BEGIN Get some information from the solver and display it
//         MFNGetIterationNumber(mfn,&its);
//         PetscPrintf(PETSC_COMM_WORLD," Number of iterations of the method: %D\n",its);
//         MFNGetDimensions(mfn,&ncv);
//         PetscPrintf(PETSC_COMM_WORLD," Subspace dimension: %D\n",ncv);
//         MFNGetTolerances(mfn,&tol,&maxit);
//         PetscPrintf(PETSC_COMM_WORLD," Stopping condition: tol=%.4g, maxit=%D\n",(double)tol,maxit);
      //END

      MFNDestroy (&mfn);
      MatDestroy (&A);
      VecDestroy (&v1);
      //VecDestroy (&v2);
      //VecDestroy (&v3);

    }

    else {
     //TODO PROVARE SOLO CON SLEPC PER ORA, MANCA DA IMPLEMENTARE BENE LE FUNZIONI create_phi1A e build_phi1Av
      unsigned CFL_pow = 4;
      
      //MatScale (A, dt / pow (2, CFL_pow));

      if (constant_jac) {
        if (counter == 0) create_phi1A (CFL_pow, Jac[i], phi1A[i], JacScaled, phi1APetsc, Temp, Temp2);

        for (int ii = 0; ii < 3*NumberOfLayers; ii++) {
          double value1 = 0.; //tracer 1 
          //double value2 = 0.; //tracer 2
          //double value3 = 0.; //tracer 3
          for (unsigned kk = 0; kk < 3*NumberOfLayers; kk++) {
            value1 += phi1A[i][ii][kk] * Res[kk];
            //value2 += phi1A[i][ii][kk] * aResHS[kk];
            //value3 += phi1A[i][ii][kk] * aResHZ[kk];
          }

          VecSetValues (y1, 1, &ii, &value1, INSERT_VALUES); //tracer 1
          //VecSetValues (y2, 1, &ii, &value2, INSERT_VALUES); //tracer 2
          //VecSetValues (y3, 1, &ii, &value3, INSERT_VALUES); //tracer 3
        }

        //tracer 1
        VecAssemblyBegin (y1);
        VecAssemblyEnd (y1);
        VecScale (y1, dt);
//         //tracer 2
//         VecAssemblyBegin (y2);
//         VecAssemblyEnd (y2);
//         VecScale (y2, dt);
//         //tracer 3
//         VecAssemblyBegin (y3);
//         VecAssemblyEnd (y3);
//         VecScale (y3, dt);
      }

      else if (phi_once) {
        clock_t phi1A_time = clock();
        create_phi1A (CFL_pow, Jac[i], phi1A[i], JacScaled, phi1APetsc, Temp, Temp2);
        //std::cout << " phi1A TIME :\t" << static_cast<double> (clock() - phi1A_time) / CLOCKS_PER_SEC << std::endl;

//         clock_t mult_time = clock();
        for (int ii = 0; ii < 3*NumberOfLayers; ii++) {
          double value1 = 0.; //tracer 1
          //double value2 = 0.; //tracer 2
          //double value3 = 0.; //tracer 3
          for (unsigned kk = 0; kk < 3*NumberOfLayers; kk++) {
            value1 += phi1A[i][ii][kk] * Res[kk];
            //value2 += phi1A[i][ii][kk] * aResHS[kk];
            //value3 += phi1A[i][ii][kk] * aResHZ[kk];
          }

          VecSetValues (y1, 1, &ii, &value1, INSERT_VALUES); //tracer 1
          //VecSetValues (y2, 1, &ii, &value2, INSERT_VALUES); //tracer 2
          //VecSetValues (y3, 1, &ii, &value3, INSERT_VALUES); //tracer 3
        }

        //tracer 1
        VecAssemblyBegin (y1);
        VecAssemblyEnd (y1);
        VecScale (y1, dt);
        //std::cout << " mat/vec multiplication TIME :\t" << static_cast<double> (clock() - mult_time) / CLOCKS_PER_SEC << std::endl;
//         //tracer 2
//         VecAssemblyBegin (y2);
//         VecAssemblyEnd (y2);
//         VecScale (y2, dt);
//         //tracer 3
//         VecAssemblyBegin (y3);
//         VecAssemblyEnd (y3);
//         VecScale (y3, dt);
      }

      else {
        //tracer 1
        build_phi1Av (CFL_pow, Jac[i], Res, y1, JacScaled, phi1APetsc, Temp, Temp2);
        VecScale (y1, dt);
//         //tracer 2
//         build_phi1Av (CFL_pow, Jac[i], aResHS, y2, JacScaled, phi1APetsc, Temp, Temp2);
//         VecScale (y2, dt);
//         //tracer 3
//         build_phi1Av (CFL_pow, Jac[i], aResHZ, y3, JacScaled, phi1APetsc, Temp, Temp2);
//         VecScale (y3, dt);
      }

//       std::cout<<" ------------- y1 ------------- " << std::endl;
//       VecView (y1, PETSC_VIEWER_STDOUT_WORLD);
//       std::cout<<" ------------- y2 ------------- " << std::endl;
//       VecView (y2, PETSC_VIEWER_STDOUT_WORLD);
//       std::cout<<" ------------- y3 ------------- " << std::endl;
//       VecView (y3, PETSC_VIEWER_STDOUT_WORLD);

    }

    std::vector <double> EPS_local (NLayers, 0.);
    std::vector <double> EPS_HS (NLayers, 0.);
    std::vector <double> EPS_HZ (NLayers, 0.);

    int kkk = 0.;
    for (int k = 0; k < NLayers; k++) {
      //tracer 1
      PetscScalar valueHT = 0.;
      VecGetValues (y1, 1, &k, &valueHT);
      EPS_local[k] = valueHT;
      //tracer 2
      PetscScalar valueHS = 0.;
      kkk = k + NLayers;
      VecGetValues (y1, 1, &kkk, &valueHS);
      EPS_HS[k] = valueHS;
      //tracer 3
      PetscScalar valueHZ = 0.;
      kkk = k + 2*NLayers;
      VecGetValues (y1, 1, &kkk, &valueHZ);
      EPS_HZ[k] = valueHZ;
    }

    EPS->add_vector_blocked (EPS_local, l2GMapRow); //tracer 1
    EPS->add_vector_blocked (EPS_HS, l2GMapRowHS); //tracer 2
    EPS->add_vector_blocked (EPS_HZ, l2GMapRowHZ); //tracer 3

    VecDestroy (&y1);
    //VecDestroy (&y2);
    //VecDestroy (&y3);

  }

  sol->UpdateSol (mlPdeSys->GetSolPdeIndex(), EPS, pdeSys->KKoffset);

//   for (unsigned k = 0; k < NumberOfLayers; k++) {
// 
//     for (unsigned i =  start; i <  end; i++) {
//       sol->_Sol[solIndexHS[k]]->add(i, EPS_HS[i*NLayers + k]);
//       sol->_Sol[solIndexHZ[k]]->add(i, EPS_HZ[i*NLayers + k]);
//     }
//     sol->_Sol[solIndexHS[k]]->close();
//     sol->_Sol[solIndexHZ[k]]->close();
// 
//   }

// printing of the max value of w in every layer
//   for ( unsigned k = 0; k < NLayers; k++ ) {
//     std::cout << "layer " << k << " " << maxW[k] << std::endl;
//   }

//   PetscViewer    viewer;
//   PetscViewerDrawOpen(PETSC_COMM_WORLD,NULL,NULL,0,0,900,900,&viewer);
//   PetscObjectSetName((PetscObject)viewer,"FSI matrix");
//   PetscViewerPushFormat(viewer,PETSC_VIEWER_DRAW_LG);
//   MatView((static_cast<PetscMatrix*>(KK))->mat(),viewer);
//   double a;
//   std::cin>>a;

//  abort();
//std::cout << "dt = " << dt << std::endl;
//std::cout << "first stage " << std::endl;

  if (twostage == true) {

    //std::cout << "second stage " << std::endl;

    EPS->zero();

    for (unsigned i =  start; i <  end; i++) {

      vector < double > solhm (NLayers);
      vector < double > solh (NLayers);   // local coordinates
      vector < double > solhp (NLayers);
      vector < double > solvm (NLayers);   // local coordinates
      vector < double > solvp (NLayers);   // local coordinates
      vector < double > solHTm (NLayers);   // local coordinates
      vector < double > solHT (NLayers);   // local coordinates
      vector < double > solHTp (NLayers);   // local coordinates
      vector < double > solHSm (NLayers);   // local coordinates
      vector < double > solHS (NLayers);   // local coordinates
      vector < double > solHSp (NLayers);   // local coordinates
      vector < double > solHZm (NLayers);   // local coordinates
      vector < double > solHZ (NLayers);   // local coordinates
      vector < double > solHZp (NLayers);   // local coordinates

      vector < double > solHTmm (NLayers);   // local coordinates
      vector < double > solHTpp (NLayers);   // local coordinates

      vector< double > aResHT (NLayers, 0.);
      vector< double > aResHS (NLayers, 0.);
      vector< double > aResHZ (NLayers, 0.);

      unsigned bc1 = (i == start) ? 0 : 1;
      unsigned bc2 = (i == end - 1) ? 0 : 1;

//       unsigned bc3 = (i > start + 1) ? 1 : 0;
//       unsigned bc4 = (i < end - 2) ? 1 : 0;

      for (unsigned j = 0; j < NLayers; j++) {

        solh[j] = (*sol->_Sol[solIndexh[j]]) (i);
        solHT[j] = (*sol->_Sol[solIndexHT[j]]) (i);
        l2GMapRow[/*NLayers +*/ j] = pdeSys->GetSystemDof (solIndexHT[j], solPdeIndexHT[j], 0, i);
        solHS[j] = (*sol->_Sol[solIndexHS[j]]) (i);
        l2GMapRowHS[/*NLayers +*/ j] = pdeSys->GetSystemDof (solIndexHS[j], solPdeIndexHS[j], 0, i);
        solHZ[j] = (*sol->_Sol[solIndexHZ[j]]) (i);
        l2GMapRowHZ[/*NLayers +*/ j] = pdeSys->GetSystemDof (solIndexHZ[j], solPdeIndexHZ[j], 0, i);

        solvm[j] = (*sol->_Sol[solIndexv[j]]) (i);
        solvp[j] = (*sol->_Sol[solIndexv[j]]) (i + 1);


        if (i > start) {
          solhm[j] = (*sol->_Sol[solIndexh[j]]) (i - 1);
          solHTm[j] = (*sol->_Sol[solIndexHT[j]]) (i - 1);
          solHSm[j] = (*sol->_Sol[solIndexHS[j]]) (i - 1);
          solHZm[j] = (*sol->_Sol[solIndexHZ[j]]) (i - 1);
        }

        if (i < end - 1) {
          solhp[j] = (*sol->_Sol[solIndexh[j]]) (i + 1);
          solHTp[j] = (*sol->_Sol[solIndexHT[j]]) (i + 1);
          solHSp[j] = (*sol->_Sol[solIndexHS[j]]) (i + 1);
          solHZp[j] = (*sol->_Sol[solIndexHZ[j]]) (i + 1);
        }

//       if ( i > start + 1 ) {
//         solHTmm[j] = ( *sol->_Sol[solIndexHT[j]] ) ( i - 2 );
//       }
//
//       if ( i < end - 2 ) {
//         solHTpp[j] = ( *sol->_Sol[solIndexHT[j]] ) ( i + 2 );
//       }

      }

      vector < double > x (2);   // local coordinates

      for (unsigned j = 0; j < 2; j++) {
        unsigned xDof  = msh->GetSolutionDof (j, i, 2);   // global to global mapping between coordinates node and coordinate dof
        x[j] = (*msh->_topology->_Sol[0]) (xDof);     // global extraction and local storage for the element coordinates
      }

      double dx = x[1] - x[0];

      for (unsigned k = 0; k < NLayers; k++) {

        //BEGIN FIRST ORDER HORIZONTAL ADVECTION
        if (i > start) {
          //aResHT[k] += 0.5 * (solHTm[k] + solHT[k]) * solvm[k]  / dx; //second order
          if (solvm[k] > 0) {
            aResHT[k] += solHTm[k] * solvm[k] / dx; //tracer 1
            aResHS[k] += solHSm[k] * solvm[k] / dx; //tracer 2
            aResHZ[k] += solHZm[k] * solvm[k] / dx; //tracer 3
          }

          else {
            aResHT[k] += solHT[k] * solvm[k] / dx;
            aResHS[k] += solHS[k] * solvm[k] / dx;
            aResHZ[k] += solHZ[k] * solvm[k] / dx;
          }
        }

        if (i < end - 1) {
          //aResHT[k] -= 0.5 * (solHT[k] + solHTp[k]) * solvp[k]  / dx; //second order
          if (solvp[k] > 0) {
            aResHT[k] -= solHT[k] * solvp[k] / dx;
            aResHS[k] -= solHS[k] * solvp[k] / dx;
            aResHZ[k] -= solHZ[k] * solvp[k] / dx;
          }

          else {
            aResHT[k] -= solHTp[k] * solvp[k] / dx;
            aResHS[k] -= solHSp[k] * solvp[k] / dx;
            aResHZ[k] -= solHZp[k] * solvp[k] / dx;
          }
        }

        //END FIRST ORDER HORIZONTAL ADVECTION

        //BEGIN THIRD ORDER HORIZONTAL ADVECTION
//       if ( i > start ) {
//         aResHT[k] += 0.5 * ( solHTm[k].value() + solHT[k].value() ) * solvm[k] / dx;
//         if ( solvm[k] > 0 ) {
//           if ( i > start + 1 ) {
//             aResHT[k] += - 1. / 6. * ( solHT[k].value() - 2.*solHTm[k].value() + solHTmm[k].value() ) * solvm[k]  / dx;
//           }
//         }
//         else {
//           if ( i < end - 1 ) {
//             aResHT[k] += - 1. / 6. * ( solHTp[k].value() - 2.*solHT[k].value() + solHTm[k].value() ) * solvm[k]  / dx;
//           }
//         }
//       }
//       if ( i < end - 1 ) {
//         aResHT[k] -= 0.5 * ( solHTp[k].value() + solHT[k].value() ) * solvp[k] / dx;
//         if ( solvp[k] > 0 ) {
//           if (i > start) {
//             aResHT[k] -= - 1. / 6. * ( solHTp[k].value() - 2.*solHT[k].value() + solHTm[k].value() ) * solvp[k]  / dx;
//           }
//         }
//         else {
//           if ( i < end - 2 ) {
//             aResHT[k] -= - 1. / 6. * ( solHTpp[k].value() - 2.*solHTp[k].value() + solHT[k].value() ) * solvp[k]  / dx;
//           }
//         }
//       }
        //END THIRD ORDER HORIZONTAL ADVECTION

        //BEGIN HORIZONTAL DIFFUSION
        if (i > start) {
          aResHT[k] += k_h * (0.5 * (solhm[k] + solh[k])) * (solHTm[k] / solhm[k] - solHT[k] / solh[k]) / (dx * dx); //tracer 1
          aResHS[k] += k_h * (0.5 * (solhm[k] + solh[k])) * (solHSm[k] / solhm[k] - solHS[k] / solh[k]) / (dx * dx); //tracer 2
          aResHZ[k] += k_h * (0.5 * (solhm[k] + solh[k])) * (solHZm[k] / solhm[k] - solHZ[k] / solh[k]) / (dx * dx); //tracer 3
        }

        if (i < end - 1) {
          aResHT[k] += k_h * (0.5 * (solhp[k] + solh[k])) * (solHTp[k] / solhp[k] - solHT[k] / solh[k]) / (dx * dx);
          aResHS[k] += k_h * (0.5 * (solhp[k] + solh[k])) * (solHSp[k] / solhp[k] - solHS[k] / solh[k]) / (dx * dx);
          aResHZ[k] += k_h * (0.5 * (solhp[k] + solh[k])) * (solHZp[k] / solhp[k] - solHZ[k] / solh[k]) / (dx * dx);
        }

        //END HORIZONTAL DIFFUSION
        
        //BEGIN FORCING TERM
        //TODO da togliere quando lo si da' al matrix exponential
      
        //radioactive tracer 1
        //aResHT[k] += - solHT[k] * lambda;
        
        double dimensione = 1./86400.; 
        //double dimensione = 1./1400.; //minute^{-1}
        double aa = 2.0 * dimensione;
        double bb = 0.5 * dimensione;
        double cc = 0.5; //TODO
        double rr = 0.1 * dimensione; //1. * dimensione;
        double kk = 0.2; //TODO
        double delta = 0.35 * dimensione; //0.35 * dimensione;
        double delta1 = 0.15 * dimensione; //0.15 * dimensione;
        double eps = 0.2 * dimensione;
        
        //SPITTING 1
//         //tracer 1 /
//         aResHT[k] += - bb * solHZ[k] + bb * exp(- cc * solHT[k]) * solHZ[k];
//         //tracer 2
//         aResHS[k] += - aa * solHT[k] * (solHS[k] / (kk + solHS[k])) + rr * solHT[k] + eps * solHZ[k]  
//                 + delta1 * (1. - exp(- cc * solHT[k])) * solHZ[k];
                
        //SPITTING 2     
        //tracer 1 
        aResHT[k] += aa * solHT[k] * (solHS[k] / (kk + solHS[k])) + bb * exp(- cc * solHT[k]) * solHZ[k];              
        //tracer 2
        aResHZ[k] += - solHZ[k] * delta * exp(- cc * solHT[k]);
        //tracer 3
        aResHS[k] += - aa * solHT[k] * (solHS[k] / (kk + solHS[k])) - delta1 * exp(- cc * solHT[k]) * solHZ[k];
                
        //SPITTING 3
//         //tracer 1 /
//         aResHT[k] += aa * solHT[k] * (solHS[k] / (kk + solHS[k])) - bb * solHZ[k] + bb * exp(- cc * solHT[k]) * solHZ[k]; 
//         //tracer 3
//         aResHZ[k] += - solHZ[k] * delta * exp(- cc * solHT[k]);
//         //tracer 2
//         aResHS[k] += - aa * solHT[k] * (solHS[k] / (kk + solHS[k])) + rr * solHT[k] + eps * solHZ[k]  
//                      + delta1 * (1. - exp(- cc * solHT[k])) * solHZ[k];
                     
        //SPLITTING 4: no terms in the second stage
        //END FORCING TERM

      }
      

      //BEGIN Nstar - N
      for (unsigned k = 0; k < NLayers; k++) {
        aResHT[k] -= N[i * NLayers + k];
        aResHS[k] -= N_S[i * NLayers + k];
        aResHZ[k] -= N_Z[i * NLayers + k];
      }

      //END
      
      vector< double > Res (3*NLayers);  // local redidual vector
      for (unsigned k = 0; k < NLayers; k++) {
        Res[k] =  aResHT[k];
        Res[k + NLayers] =  aResHS[k];
        Res[k + 2*NLayers] =  aResHZ[k];
      }

      Vec y1/*, y2, y3*/;

      //tracer 1
      VecCreate (PETSC_COMM_WORLD, &y1);
      VecSetSizes (y1, PETSC_DECIDE, 3 * NLayers);
      VecSetFromOptions (y1);
//       //tracer 2
//       VecCreate (PETSC_COMM_WORLD, &y2);
//       VecSetSizes (y2, PETSC_DECIDE, NLayers);
//       VecSetFromOptions (y2);
//       //tracer 3
//       VecCreate (PETSC_COMM_WORLD, &y3);
//       VecSetSizes (y3, PETSC_DECIDE, NLayers);
//       VecSetFromOptions (y3);

      if (slepc) {
        Mat A;
        Vec v1/*, v2, v3*/;

        MatCreate (MPI_COMM_SELF, &A);
        MatSetSizes (A, 3*NLayers, 3*NLayers, 3*NLayers, 3*NLayers);
        MatSetType (A, MATSEQDENSE);
        MatSeqDenseSetPreallocation (A, &Jac[i][0]);
        MatAssemblyBegin (A, MAT_FINAL_ASSEMBLY);
        MatAssemblyEnd (A, MAT_FINAL_ASSEMBLY);
        //MatTranspose (A, MAT_INPLACE_MATRIX, &A);
        //MatView ( A,PETSC_VIEWER_STDOUT_WORLD );

        //tracer 1
        VecCreateSeqWithArray (MPI_COMM_SELF, 1, 3*NLayers, &Res[0], &v1);
        VecAssemblyBegin (v1);
        VecAssemblyEnd (v1);
        //VecView(v1,PETSC_VIEWER_STDOUT_WORLD);
//         //tracer 2
//         VecCreateSeqWithArray (MPI_COMM_SELF, 1, NLayers, &aResHS[0], &v2);
//         VecAssemblyBegin (v2);
//         VecAssemblyEnd (v2);
//         //VecView(v2,PETSC_VIEWER_STDOUT_WORLD);
//         //tracer 3
//         VecCreateSeqWithArray (MPI_COMM_SELF, 1, NLayers, &aResHZ[0], &v3);
//         VecAssemblyBegin (v3);
//         VecAssemblyEnd (v3);
//         //VecView(v3,PETSC_VIEWER_STDOUT_WORLD);

        MFN mfn;
        FN f;
        MFNConvergedReason reason;

        //BEGIN condition number of A
//             SVD svd;
//             PetscReal sigma_1,sigma_n;
//             PetscInt nconv1,nconv2;
//
//             SVDCreate(PETSC_COMM_WORLD,&svd);
//             SVDSetOperator(svd,A);
//             SVDSetFromOptions(svd);
//             SVDSetDimensions(svd,1,PETSC_DEFAULT,PETSC_DEFAULT);
//             //First request a singular value from one end of the spectrum
//             SVDSetWhichSingularTriplets(svd,SVD_LARGEST);
//             SVDSolve(svd);
//             //Get number of converged singular values
//             SVDGetConverged(svd,&nconv1);
//             //Get converged singular values: largest singular value is stored in sigma_1.
//             if (nconv1 > 0) {
//               SVDGetSingularTriplet(svd,0,&sigma_1,NULL,NULL);
//             }
//             else {
//               PetscPrintf(PETSC_COMM_WORLD," Unable to compute large singular value!\n\n");
//             }
//             //Request a singular value from the other end of the spectrum
//             SVDSetWhichSingularTriplets(svd,SVD_SMALLEST);
//             SVDSolve(svd);
//             //Get number of converged singular triplets
//             SVDGetConverged(svd,&nconv2);
//             //Get converged singular values: smallest singular value is stored in sigma_n.
//             if (nconv2 > 0) {
//               SVDGetSingularTriplet(svd,0,&sigma_n,NULL,NULL);
//             }
//             else {
//               PetscPrintf(PETSC_COMM_WORLD," Unable to compute small singular value!\n\n");
//             }
//             //Display solution and clean up
//             if (nconv1 > 0 && nconv2 > 0) {
//               PetscPrintf(PETSC_COMM_WORLD," Computed singular values: sigma_1=%.4f, sigma_n=%.4f\n",(double)sigma_1,(double)sigma_n);
//               PetscPrintf(PETSC_COMM_WORLD," Estimated condition number: sigma_1/sigma_n=%.4f\n\n",(double)(sigma_1/sigma_n));
//             }
//             //Free work space
//             SVDDestroy(&svd);
        //END

        PetscReal tol;
        PetscInt ncv, maxit, its;

        MFNCreate (PETSC_COMM_WORLD, &mfn);

        MFNSetOperator (mfn, A);
        MFNGetFN (mfn, &f);

        FNPhiSetIndex (f, 1);
        FNSetType (f, FNPHI);
        // FNView(f,PETSC_VIEWER_STDOUT_WORLD);

        FNSetScale (f, dt, dt * 0.5);
        
        if (NLayers==100){
          MFNSetDimensions (mfn, 10); 
        }
        else {
          MFNSetDimensions (mfn, 4);
        }

        if (i == start || i == start + 1 || i == start + 2 || i == end - 3 || i == end - 2 || i == end - 1) {
          if (NLayers==100){
           MFNSetDimensions (mfn, 20);
          }
          else {
            MFNSetDimensions (mfn, 8);
          }
        }

        tol = 1e-6;
        MFNSetTolerances (mfn, tol, PETSC_DEFAULT);
        MFNSetFromOptions (mfn);

        clock_t etd_time;

        if (counter2 == 0) {
          etd_time = clock();
//                 std::cout<< "--------------------- v "<< i << " --------------------- " <<std::endl;
//                 for ( PetscInt kk=0; kk<NLayers; kk++ ) {
//                     PetscScalar valueHT = 0.;
//                     VecGetValues ( v, 1, &kk, &valueHT );
//                     std::cout<< valueHT << std::endl;
//                 }
        }

        //BEGIN phi1A print
//         PetscInt j;
//         for (j=0; j<NLayers; j++) {
//           Vec a, a1;
//           VecCreate(PETSC_COMM_WORLD,&a);
//           VecSetSizes(a,PETSC_DECIDE,NLayers);
//           VecSetFromOptions(a);
//           VecSet(a,0.0);
//           PetscScalar one=1.0;
//           VecSetValues(a,1,&j,&one,INSERT_VALUES);
//           VecAssemblyBegin(a);
//           VecAssemblyEnd(a);
//           VecView(a,PETSC_VIEWER_STDOUT_WORLD);
//           VecDuplicate (a, &a1);
//           MFNSolve (mfn, a, a1);
//           VecScale (a1, 2./dt);
//           VecView(a1,PETSC_VIEWER_STDOUT_WORLD);
//
//           VecDestroy (&a);
//           VecDestroy (&a1);
//         }
        //END

        MFNSolve (mfn, v1, y1); //tracer 1
        //MFNSolve (mfn, v2, y2); //tracer 2
        //MFNSolve (mfn, v3, y3); //tracer 3
        MFNGetConvergedReason (mfn, &reason);

        //VecView(y1,PETSC_VIEWER_STDOUT_WORLD);
        //VecView(y2,PETSC_VIEWER_STDOUT_WORLD);
        //VecView(y3,PETSC_VIEWER_STDOUT_WORLD);

        if (reason < 0) std::cout << "Solver did not converge" << std::endl;

        if (counter2 == 0) {
          std::cout << " ETD TIME :\t" << static_cast<double> (clock() - etd_time) / CLOCKS_PER_SEC << std::endl;
//                // std::cout<< "--------------------- y --------------------- " <<std::endl;
//                 for ( PetscInt kk=0; kk<NLayers; kk++ ) {
//                     PetscScalar valueHT = 0.;
//                     VecGetValues ( y, 1, &kk, &valueHT );
//                    std::cout<< valueHT << std::endl;
//                 }
//                 //std::cout<< "--------------------- END --------------------- " <<std::endl;
        }


        //BEGIN Get some information from the solver and display it
//         MFNGetIterationNumber(mfn,&its);
//         PetscPrintf(PETSC_COMM_WORLD," Number of iterations of the method: %D\n",its);
//         MFNGetDimensions(mfn,&ncv);
//         PetscPrintf(PETSC_COMM_WORLD," Subspace dimension: %D\n",ncv);
//         MFNGetTolerances(mfn,&tol,&maxit);
//         PetscPrintf(PETSC_COMM_WORLD," Stopping condition: tol=%.4g, maxit=%D\n",(double)tol,maxit);
        //END

        MFNDestroy (&mfn);
        MatDestroy (&A);
        VecDestroy (&v1);
        //VecDestroy (&v2);
        //VecDestroy (&v3);

      }

      else {        
              
        unsigned CFL_pow = 4;
        
        //MatScale (A, dt / pow (2, CFL_pow));

        if (constant_jac || phi_once) {
          for (int ii = 0; ii < 3*NumberOfLayers; ii++) {
            double value1 = 0.; //tracer 1
            //double value2 = 0.; //tracer 2
            //double value3 = 0.; //tracer 3
            for (unsigned kk = 0; kk < 3*NumberOfLayers; kk++) {
              value1 += phi1A[i][ii][kk] * Res[kk];
              //value2 += phi1A[i][ii][kk] * aResHS[kk];
              //value3 += phi1A[i][ii][kk] * aResHZ[kk];
            }

            VecSetValues (y1, 1, &ii, &value1, INSERT_VALUES);
            //VecSetValues (y2, 1, &ii, &value2, INSERT_VALUES);
            //VecSetValues (y3, 1, &ii, &value3, INSERT_VALUES);
          }

          //tracer 1
          VecAssemblyBegin (y1);
          VecAssemblyEnd (y1);
          VecScale (y1, 0.5 * dt);
/*          //tracer 2
          VecAssemblyBegin (y2);
          VecAssemblyEnd (y2);
          VecScale (y2, 0.5 * dt);
          //tracer 3
          VecAssemblyBegin (y3);
          VecAssemblyEnd (y3);
          VecScale (y3, 0.5 * dt)*/;
        }

        else {
          //tracer 1
          build_phi1Av (CFL_pow, Jac[i], aResHT, y1, JacScaled, phi1APetsc, Temp, Temp2);
          VecScale (y1, 0.5 * dt);
//           //tracer 2
//           build_phi1Av (CFL_pow, Jac[i], aResHS, y2, JacScaled, phi1APetsc, Temp, Temp2);
//           VecScale (y2, 0.5 * dt);
//           //tracer 3
//           build_phi1Av (CFL_pow, Jac[i], aResHZ, y3, JacScaled, phi1APetsc, Temp, Temp2);
//           VecScale (y3, 0.5 * dt);
        }

//         std::cout<<" ------------- y1 ------------- " << std::endl;
//         VecView (y1, PETSC_VIEWER_STDOUT_WORLD);
//         std::cout<<" ------------- y2 ------------- " << std::endl;
//         VecView (y2, PETSC_VIEWER_STDOUT_WORLD);
//         std::cout<<" ------------- y3 ------------- " << std::endl;
//         VecView (y3, PETSC_VIEWER_STDOUT_WORLD);

      }

      std::vector <double> EPS_local (NLayers, 0.);
      std::vector <double> EPS_HS (NLayers, 0.);
      std::vector <double> EPS_HZ (NLayers, 0.);

      int kkk = 0.;
      for (int k = 0; k < NLayers; k++) {
        //tracer 1
        PetscScalar valueHT = 0.;
        VecGetValues (y1, 1, &k, &valueHT);
        EPS_local[k] = valueHT;
        //tracer 2
        PetscScalar valueHS = 0.;
        kkk = k + NLayers;
        VecGetValues (y1, 1, &kkk, &valueHS);
        EPS_HS[k] = valueHS;
        //tracer 3
        PetscScalar valueHZ = 0.;
        kkk = k + 2*NLayers;
        VecGetValues (y1, 1, &kkk, &valueHZ);
        EPS_HZ[k] = valueHZ;
      }

      EPS->add_vector_blocked (EPS_local, l2GMapRow); //tracer 1
      EPS->add_vector_blocked (EPS_HS, l2GMapRowHS); //tracer 2
      EPS->add_vector_blocked (EPS_HZ, l2GMapRowHZ); //tracer 3

      VecDestroy (&y1);
      //VecDestroy (&y2);
      //VecDestroy (&y3);

    }

  }

  sol->UpdateSol (mlPdeSys->GetSolPdeIndex(), EPS, pdeSys->KKoffset);

//   for (unsigned k = 0; k < NumberOfLayers; k++) {
//     for (unsigned i =  start; i <  end; i++) {
//       sol->_Sol[solIndexHS[k]]->add(i, EPS_HS[i*NLayers + k]);
//       sol->_Sol[solIndexHZ[k]]->add(i, EPS_HZ[i*NLayers + k]);
//     }
// 
//     sol->_Sol[solIndexHS[k]]->close();
//     sol->_Sol[solIndexHZ[k]]->close();
//   }

  //PARAVIEW
  unsigned solIndexeta = mlSol->GetIndex ("eta");
  unsigned solIndexb = mlSol->GetIndex ("b");
  sol->_Sol[solIndexeta]->zero();

  for (unsigned k = 0; k < NumberOfLayers; k++) {
    sol->_Sol[solIndexeta]->add (*sol->_Sol[solIndexh[k]]);
  }

  sol->_Sol[solIndexeta]->add (-1, *sol->_Sol[solIndexb]);

  for (unsigned k = 0; k < NumberOfLayers; k++) {

    //if(counter == numberOfTimeSteps-1) std::cout << "T" << k << "  ----------------------------------------------------" << std::endl;
    for (unsigned i =  msh->_dofOffset[solTypeHT][iproc]; i <  msh->_dofOffset[solTypeHT][iproc + 1]; i++) {
      double valueHT = (*sol->_Sol[solIndexHT[k]]) (i);
      double valueHS = (*sol->_Sol[solIndexHS[k]]) (i);
      double valueHZ = (*sol->_Sol[solIndexHZ[k]]) (i);
      double valueH = (*sol->_Sol[solIndexh[k]]) (i);

      double valueT = valueHT / valueH;
      double valueS = valueHS / valueH;
      double valueZ = valueHZ / valueH;

      if (counter == numberOfTimeSteps - 1) {
        std::cout.precision (14);
        std::cout << valueT << std::endl;
      }

      //if ( i == 10 ) std::cout << "temperature " << valueT << std::endl;
      //if (i == 0) valueT = 15.;
      //if (i == msh->_dofOffset[solTypeHT][iproc + 1] - 1 ) valueT = 15.;
      sol->_Sol[solIndexT[k]]->set (i, valueT);
      sol->_Sol[solIndexS[k]]->set (i, valueS);
      sol->_Sol[solIndexZ[k]]->set (i, valueZ);
    }

    sol->_Sol[solIndexT[k]]->close();
    sol->_Sol[solIndexS[k]]->close();
    sol->_Sol[solIndexZ[k]]->close();

  }

}


void RK4 (MultiLevelProblem& ml_prob, const bool & implicitEuler, const unsigned & numberOfTimeSteps)
{

  const unsigned& NLayers = NumberOfLayers;

  adept::Stack& s = FemusInit::_adeptStack;

  TransientLinearImplicitSystem* mlPdeSys  = &ml_prob.get_system<TransientLinearImplicitSystem> ("SWt");   // pointer to the linear implicit system named "Poisson"

  unsigned level = ml_prob._ml_msh->GetNumberOfLevels() - 1u;

  Mesh* msh = ml_prob._ml_msh->GetLevel (level);   // pointer to the mesh (level) object
  elem* el = msh->el;  // pointer to the elem object in msh (level)

  MultiLevelSolution* mlSol = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution* sol = ml_prob._ml_sol->GetSolutionLevel (level);   // pointer to the solution (level) object

  LinearEquationSolver* pdeSys = mlPdeSys->_LinSolver[level]; // pointer to the equation (level) object

  const unsigned  dim = msh->GetDimension(); // get the domain dimension of the problem

  unsigned    iproc = msh->processor_id(); // get the process_id (for parallel computation)
  unsigned    nprocs = msh->n_processors(); // get the process_id (for parallel computation)


  //solution variable
  std::vector < unsigned > solIndexh (NLayers);

  std::vector < unsigned > solIndexv (NLayers);

  std::vector < unsigned > solIndexHT (NLayers);
  std::vector < unsigned > solPdeIndexHT (NLayers);

  std::vector < unsigned > solIndexT (NLayers);

  vector< int > l2GMapRow; // local to global mapping
  vector< int > l2GMapColumn; // local to global mapping

  for (unsigned i = 0; i < NLayers; i++) {
    char name[10];
    sprintf (name, "h%d", i);
    solIndexh[i] = mlSol->GetIndex (name);   // get the position of "hi" in the sol object

    sprintf (name, "v%d", i);
    solIndexv[i] = mlSol->GetIndex (name);   // get the position of "vi" in the sol object

    sprintf (name, "HT%d", i);
    solIndexHT[i] = mlSol->GetIndex (name);   // get the position of "Ti" in the sol object
    solPdeIndexHT[i] = mlPdeSys->GetSolPdeIndex (name);   // get the position of "Ti" in the pdeSys object

    sprintf (name, "T%d", i);
    solIndexT[i] = mlSol->GetIndex (name);   // get the position of "Ti" in the sol object

  }

  unsigned solTypeh = mlSol->GetSolutionType (solIndexh[0]);   // get the finite element type for "hi"
  unsigned solTypev = mlSol->GetSolutionType (solIndexv[0]);   // get the finite element type for "vi"
  unsigned solTypeHT = mlSol->GetSolutionType (solIndexHT[0]);   // get the finite element type for "Ti"

  for (unsigned k = 0; k < NumberOfLayers; k++) {
    for (unsigned i =  msh->_dofOffset[solTypeHT][iproc]; i <  msh->_dofOffset[solTypeHT][iproc + 1]; i++) {
      double valueT = (*sol->_SolOld[solIndexT[k]]) (i);
      //double valueT = ( *sol->_Sol[solIndexT[k]] ) ( i );
      double valueH = (*sol->_Sol[solIndexh[k]]) (i);

      double valueHT = valueT * valueH;

      sol->_Sol[solIndexHT[k]]->set (i, valueHT);
    }

    sol->_Sol[solIndexHT[k]]->close();
  }

  std::vector < double > maxW (NLayers, -1.e6);
  maxW[0] = 0.;

  unsigned start = msh->_dofOffset[solTypeHT][iproc];
  unsigned end = msh->_dofOffset[solTypeHT][iproc + 1];

  for (unsigned i =  start; i <  end; i++) {

    vector < double > solhm (NLayers);
    vector < double > solh (NLayers);   // local coordinates
    vector < double > solhp (NLayers);
    vector < double > solvm (NLayers);   // local coordinates
    vector < double > solvp (NLayers);   // local coordinates
    vector < double > solHTm (NLayers);   // local coordinates
    vector < double > solHT (NLayers);   // local coordinates
    vector < double > solHTp (NLayers);   // local coordinates

    vector < double > solHTmm (NLayers);   // local coordinates
    vector < double > solHTpp (NLayers);   // local coordinates

    //vector< adept::adouble > aResHT ( NLayers );

    unsigned bc1 = (i == start) ? 0 : 1;
    unsigned bc2 = (i == end - 1) ? 0 : 1;

//     unsigned bc3 = ( i > start + 1 ) ? 1 : 0;
//     unsigned bc4 = ( i < end - 2 ) ? 1 : 0;

    l2GMapRow.resize (NLayers);
    l2GMapColumn.resize ( (1 + bc1 + bc2) * NLayers);

    //std::fill ( aResHT.begin(), aResHT.end(), 0 ); //set aRes to zero

    for (unsigned j = 0; j < NLayers; j++) {

      solh[j] = (*sol->_Sol[solIndexh[j]]) (i);
      solHT[j] = (*sol->_Sol[solIndexHT[j]]) (i);
      l2GMapRow[/*NLayers +*/ j] = pdeSys->GetSystemDof (solIndexHT[j], solPdeIndexHT[j], 0, i);

      l2GMapColumn[/*NLayers +*/ j] = pdeSys->GetSystemDof (solIndexHT[j], solPdeIndexHT[j], 0, i);

      solvm[j] = (*sol->_Sol[solIndexv[j]]) (i);
      solvp[j] = (*sol->_Sol[solIndexv[j]]) (i + 1);

      if (i > start) {
        solhm[j] = (*sol->_Sol[solIndexh[j]]) (i - 1);
        solHTm[j] = (*sol->_Sol[solIndexHT[j]]) (i - 1);

        l2GMapColumn[NLayers + j] = pdeSys->GetSystemDof (solIndexHT[j], solPdeIndexHT[j], 0, i - 1);

      }

      if (i < end - 1) {
        solhp[j] = (*sol->_Sol[solIndexh[j]]) (i + 1);
        solHTp[j] = (*sol->_Sol[solIndexHT[j]]) (i + 1);

        l2GMapColumn[ (1 + bc1) * NLayers + j] = pdeSys->GetSystemDof (solIndexHT[j], solPdeIndexHT[j], 0, i + 1);
      }

//       if ( i > start + 1 ) {
//         solHTmm[j] = ( *sol->_Sol[solIndexHT[j]] ) ( i - 2 );
//         if (i == end - 1) l2GMapColumn[( 1 + bc1 ) * NLayers + j] = pdeSys->GetSystemDof ( solIndexHT[j], solPdeIndexHT[j], 0, i - 2 );
//         else l2GMapColumn[( (1 + bc1) + bc3 ) * NLayers + j] = pdeSys->GetSystemDof ( solIndexHT[j], solPdeIndexHT[j], 0, i - 2 );
//       }
//
//       if ( i < end - 2 ) {
//         solHTpp[j] = ( *sol->_Sol[solIndexHT[j]] ) ( i + 2 );
//         l2GMapColumn[( (1 + bc1) + bc3 + bc4 ) * NLayers + j] = pdeSys->GetSystemDof ( solIndexHT[j], solPdeIndexHT[j], 0, i + 2 );
//       }

    }

//   s.new_recording();

    vector < double > x (2);   // local coordinates

    for (unsigned j = 0; j < 2; j++) {
      unsigned xDof  = msh->GetSolutionDof (j, i, 2);   // global to global mapping between coordinates node and coordinate dof
      x[j] = (*msh->_topology->_Sol[0]) (xDof);     // global extraction and local storage for the element coordinates
    }

    double dx = x[1] - x[0];

    double b = 10.; //( H_shelf + H_0 / 2 * (1 + tanh(hh / phi)) );

    std::vector < double > w (NLayers + 1, 0.);
    //w[0] = 1.;

//     std::vector < double > zMid ( NLayers );
//     for ( unsigned k = 0; k < NLayers; k++ ) {
//       zMid[k] = -b + solh[k] / 2.;
//       for ( unsigned i = k + 1; i < NLayers; i++ ) {
//         zMid[k] += solh[i];
//       }
//     }

    std::vector < double > zTop (NLayers);
    zTop[0] = 0;

    for (unsigned k = 1; k < NLayers; k++) {
      zTop[k] = zTop[k - 1] - solh[k];
    }

    std::vector < double > psi2 (NLayers);

    for (unsigned k = 0; k < NLayers; k++) {
      //psi2[k] = ( - ( zMid[k] + 10 ) * zMid[k] ) / 25;
      psi2[k] = 1. - (zTop[k] + 5.) * (zTop[k] + 5.) / (25.);
    }

    double xmid = 0.5 * (x[1] + x[0]);

    for (unsigned k = NLayers; k > 1; k--) {
      //w[k - 1] = - ( -4. / 625.* ( xmid - 5 ) * ( xmid - 5 ) * ( xmid - 5 ) ) * psi2[k - 1];
      w[k - 1] = - (- 8. / (pow (10., 8)) * pow ( (xmid - 10.), 7)) * psi2[k - 1];

      //w[k - 1] = - ( - (2./25.) * (xmid - 5.) ) * psi2[k - 1];
      //w[k - 1] = ( ( 10. - 2. * xmid ) / 25. ) * psi2[k - 1];
      if (maxW[k - 1] < w[k - 1]) {
        maxW[k - 1] = w[k - 1];
      }
    }


    std::vector < double > k1_RK (NLayers, 0.);
    std::vector < double > k2_RK (NLayers, 0.);
    std::vector < double > k3_RK (NLayers, 0.);
    std::vector < double > k4_RK (NLayers, 0.);

    for (unsigned RK_step = 0; RK_step < 4; RK_step++) {
      for (unsigned k = 0; k < NLayers; k++) {
        double LHS = 0.;
        double addition = 0.;

        if (RK_step == 1) {
          addition = k1_RK[k] * 0.5;
        }

        else if (RK_step == 2) {
          addition = k2_RK[k] * 0.5;
        }

        else if (RK_step == 3) {
          addition = k3_RK[k];
        }

        //BEGIN FIRST ORDER
        if (i > start) {
          if (solvm[k] > 0) {
            LHS += (solHTm[k] + addition) * solvm[k] / dx;
          }

          else {
            LHS += (solHT[k] + addition) * solvm[k] / dx;
          }
        }

        if (i < end - 1) {
          if (solvp[k] > 0) {
            LHS -= (solHT[k] + addition) * solvp[k] / dx;   //first order upwind
          }

          else {
            LHS -= (solHTp[k] + addition) * solvp[k] / dx;   //first order upwind
          }
        }

        //END

        //BEGIN THIRD ORDER
//       if ( i > start ) {
//         aResHT[k] += 0.5 * ( solHTm[k].value() + solHT[k].value() ) * solvm[k] / dx;
//         if ( solvm[k] > 0 ) {
//           if ( i > start + 1 ) {
//             aResHT[k] += - 1. / 6. * ( solHT[k].value() - 2.*solHTm[k].value() + solHTmm[k].value() ) * solvm[k]  / dx;
//           }
//         }
//         else {
//           if ( i < end - 1 ) {
//             aResHT[k] += - 1. / 6. * ( solHTp[k].value() - 2.*solHT[k].value() + solHTm[k].value() ) * solvm[k]  / dx;
//           }
//         }
//       }
//       if ( i < end - 1 ) {
//         aResHT[k] -= 0.5 * ( solHTp[k].value() + solHT[k].value() ) * solvp[k] / dx;
//         if ( solvp[k] > 0 ) {
//           if (i > start) {
//             aResHT[k] -= - 1. / 6. * ( solHTp[k].value() - 2.*solHT[k].value() + solHTm[k].value() ) * solvp[k]  / dx;
//           }
//         }
//         else {
//           if ( i < end - 2 ) {
//             aResHT[k] -= - 1. / 6. * ( solHTpp[k].value() - 2.*solHTp[k].value() + solHT[k].value() ) * solvp[k]  / dx;
//           }
//         }
//       }
        //END

        if (k < NLayers - 1) {
//           LHS += w[k + 1] * 0.5 * ( ( solHT[k] + addition ) / solh[k] + ( solHT[k + 1] + addition ) / solh[k + 1] );
          if (w[k + 1] > 0) {
            LHS += w[k + 1] * ( (solHT[k + 1] / solh[k + 1] + addition));
          }

          else {
            LHS += w[k + 1] * ( (solHT[k] / solh[k] + addition));
          }
        }

        if (k > 0) {
//           LHS -= w[k] * 0.5 * ( ( solHT[k - 1] + addition ) / solh[k - 1] + ( solHT[k] + addition ) / solh[k] );
          if (w[k] > 0) {
            LHS -= w[k] * ( (solHT[k] / solh[k] + addition));
          }

          else {
            LHS -= w[k] * ( (solHT[k - 1] / solh[k - 1] + addition));
          }
        }

//         if(i > start){
//           LHS += k_h * (0.5 * (solhm[k] + solh[k])) * ((solHTm[k] / solhm[k] + addition)  - (solHT[k] / solh[k] + addition))/(dx*dx); // horizontal diffusion
//         }
//         if(i < end-1){
//           LHS += k_h * (0.5 * (solhp[k] + solh[k])) * ((solHTp[k] / solhp[k] + addition)  - (solHT[k] / solh[k] + addition))/(dx*dx); // horizontal diffusion
//         }


        if (RK_step == 0) {
          k1_RK[k] = LHS * dt;
        }

        else if (RK_step == 1) {
          k2_RK[k] = LHS * dt;
        }

        else if (RK_step == 2) {
          k3_RK[k] = LHS * dt;
        }

        else {
          k4_RK[k] = LHS * dt;
        }

      }

    }

    for (unsigned k = 0; k < NumberOfLayers; k++) {

//       std::cout << k1_RK[k] << " " << k2_RK[k] << " " << k3_RK[k] << " " << k4_RK[k] << std::endl;

      double valueHT = solHT[k] + 1. / 6. * (k1_RK[k] + 2.*k2_RK[k] + 2.*k3_RK[k] + k4_RK[k]);
      sol->_Sol[solIndexHT[k]]->set (i, valueHT);
      sol->_Sol[solIndexHT[k]]->close();
    }


    if (implicitEuler == false) {
      //BEGIN forward Euler for vertical diffusion
      std::vector < double > vert_diff (NLayers, 0.);

      for (unsigned k = 0; k < NumberOfLayers; k++) {
        double deltaZt = 0.;
        double deltaZb = 0.;
        double ht = 0.;
        double hb = 0.;

        if (k > 0) {
//        ht = ( solhm[k - 1] + solhm[k] + solhp[k - 1] + solhp[k] ) / 4.;
          ht = (solh[k - 1] + solh[k]) / 2.;
          deltaZt = (solHT[k - 1] / solh[k - 1] - solHT[k] / solh[k]) / ht;
        }

        else {
//        ht = 0.5 * ( solhm[k] + solhp[k] );
          ht = solh[k];
          deltaZt = 0.* (0. - solHT[k]) / ht;
        }

        if (k < NLayers - 1) {
//        hb = ( solhm[k] + solhm[k + 1] + solhp[k] + solhp[k + 1] ) / 4.;
          hb = (solh[k] + solh[k + 1]) / 2.;
          deltaZb = (solHT[k] / solh[k] - solHT[k + 1] / solh[k + 1]) / hb;
        }

        else {
//        hb = 0.5 * ( solhm[k] + solhp[k] );
          hb = solh[k];
          deltaZb = 0.* (solHT[k] - 0.) / hb;
        }

        //std::cout << "AAAAAAAAAAAAAAAAAAAAAAAAAA" << deltaZt - deltaZb << std::endl;
        vert_diff[k] = solh[k] * k_v * (deltaZt - deltaZb) / ( (ht + hb) / 2.);      // vertical diffusion
      }

      for (unsigned k = 0; k < NumberOfLayers; k++) {
        double valueHT = (*sol->_Sol[solIndexHT[k]]) (i);
        double valueH = (*sol->_Sol[solIndexh[k]]) (i);

        double valueT = valueHT / valueH;
        valueT = valueT + dt * vert_diff[k];

        sol->_Sol[solIndexT[k]]->set (i, valueT);
        sol->_Sol[solIndexT[k]]->close();
      }

      //END
    }

    else if (implicitEuler == true) {

      std::vector <double> Trhs (NLayers, 0.);
      std::vector < std::vector < double > > sysMatrix (NLayers);

      for (unsigned k = 0; k < NLayers; k++) {
        sysMatrix[k].assign (NLayers, 0.);

        double A = 0.;
        double C = 0.;
        double ht = 0.;
        double hb = 0.;

        if (k > 0) {
          //ht = ( solhm[k - 1] + solhm[k] + solhp[k - 1] + solhp[k] ) / 4.;
          ht = (solh[k - 1] + solh[k]) / 2.;
          A = solh[k] * k_v / ht ;
        }

        else {
          //ht = 0.5 * ( solhm[k] + solhp[k] );
          ht = solh[k];
        }

        if (k < NLayers - 1) {
          //hb = ( solhm[k] + solhm[k + 1] + solhp[k] + solhp[k + 1] ) / 4.;
          hb = (solh[k] + solh[k + 1]) / 2.;
          C = solh[k] * k_v / hb;
          C /= (ht + hb) * 0.5 ;

          if (k > 0) {
            A /= (ht + hb) * 0.5 ;
          }
        }

        else {
          //hb = 0.5 * ( solhm[k] + solhp[k] );
          hb = solh[k];
          A /= (ht + hb) * 0.5 ;
        }

        double B = 1. - A - C;

        sysMatrix[k][k] = B;

        if (k > 0) {
          sysMatrix[k][k - 1] = A;
        }

        if (k < NLayers - 1) {
          sysMatrix[k][k + 1] = C;
        }

        Trhs[k] = (*sol->_Sol[solIndexHT[k]]) (i) / (*sol->_Sol[solIndexh[k]]) (i);

      }

      //risolvere il sistema Nlayer X Nlayer
      KSP                solver;
      Mat                triDiagA;
      Vec                b, x;
      PetscInt           k, j, nlayers;
      PetscErrorCode     ierr;

      nlayers = static_cast<PetscInt> (NLayers);
      ierr = VecCreate (PETSC_COMM_WORLD, &x);
      ierr = VecSetSizes (x, PETSC_DECIDE, nlayers);
      ierr = VecSetFromOptions (x);
      ierr = VecDuplicate (x, &b);
      ierr = MatCreate (PETSC_COMM_WORLD, &triDiagA);
      ierr = MatSetSizes (triDiagA, PETSC_DECIDE, PETSC_DECIDE, nlayers, nlayers);
      ierr = MatSetFromOptions (triDiagA);
      ierr = MatSetUp (triDiagA);

      for (k = 0; k < nlayers; k++) {
        MatSetValues (triDiagA, 1, &k, 1, &k, &sysMatrix[k][k], INSERT_VALUES);
        VecSetValues (b, 1, &k, &Trhs[k], INSERT_VALUES);

        if (k > 0) {
          j = k - 1;
          MatSetValues (triDiagA, 1, &k, 1, &j, & sysMatrix[k - 1][k], INSERT_VALUES);
        }

        if (k < nlayers - 1) {
          j = k + 1;
          MatSetValues (triDiagA, 1, &k, 1, &j, &sysMatrix[k][k + 1], INSERT_VALUES);
        }
      }

      ierr = VecAssemblyBegin (b);
      ierr = VecAssemblyEnd (b);

      ierr = MatAssemblyBegin (triDiagA, MAT_FINAL_ASSEMBLY);
      ierr = MatAssemblyEnd (triDiagA, MAT_FINAL_ASSEMBLY);



//       PetscViewer    viewer;
//       PetscViewerDrawOpen(PETSC_COMM_WORLD,NULL,NULL,0,0,900,900,&viewer);
//       PetscObjectSetName((PetscObject)viewer,"implicit Euler matrix");
//       PetscViewerPushFormat(viewer,PETSC_VIEWER_DRAW_LG);
//       MatView(triDiagA,viewer);
//       double a;
//       std::cin>>a;


      ierr = KSPCreate (PETSC_COMM_WORLD, &solver);
      ierr = KSPSetOperators (solver, triDiagA, triDiagA);
      ierr = KSPSetType (solver, KSPRICHARDSON);
      ierr = KSPSolve (solver, b, x);

      //1. aggiornare solT con x
      for (k = 0; k < nlayers; k++) {
        PetscScalar valueT = 0.;
        ierr = VecGetValues (x, 1, &k, &valueT);

        if (counter == numberOfTimeSteps - 1) {
          std::cout.precision (14);
          std::cout << valueT << std::endl;
        }

        sol->_Sol[solIndexT[k]]->set (i, valueT);
        sol->_Sol[solIndexT[k]]->close();

        //2. aggiornare solHT
        double valueH = (*sol->_Sol[solIndexh[k]]) (i);
        double valueHT = valueH * valueT;
        sol->_Sol[solIndexHT[k]]->set (i, valueHT);
        sol->_Sol[solIndexHT[k]]->close();
      }

      MatDestroy (&triDiagA);
      VecDestroy (&b);
      VecDestroy (&x);
      KSPDestroy (&solver);

    }

  }

  //BEGIN no vertical diffusion
//   for ( unsigned k = 0; k < NumberOfLayers; k++ ) {
//     for ( unsigned i =  msh->_dofOffset[solTypeHT][iproc]; i <  msh->_dofOffset[solTypeHT][iproc + 1]; i++ ) {
//       double valueHT = ( *sol->_Sol[solIndexHT[k]] ) ( i );
//       double valueH = ( *sol->_Sol[solIndexh[k]] ) ( i );
//
//       double valueT = valueHT / valueH;
//       std::cout.precision(14);
//       if(counter==numberOfTimeSteps-2) std::cout << valueT << std::endl;
//
//       sol->_Sol[solIndexT[k]]->set ( i, valueT );
//     }
//
//     sol->_Sol[solIndexT[k]]->close();
//
//   }
  //END

//   PetscViewer    viewer;
//   PetscViewerDrawOpen(PETSC_COMM_WORLD,NULL,NULL,0,0,900,900,&viewer);
//   PetscObjectSetName((PetscObject)viewer,"FSI matrix");
//   PetscViewerPushFormat(viewer,PETSC_VIEWER_DRAW_LG);
//   MatView((static_cast<PetscMatrix*>(KK))->mat(),viewer);
//   double a;
//   std::cin>>a;

//  abort();

//   unsigned solIndexeta = mlSol->GetIndex ( "eta" );
//   unsigned solIndexb = mlSol->GetIndex ( "b" );
//   sol->_Sol[solIndexeta]->zero();
//   for ( unsigned k = 0; k < NumberOfLayers; k++ ) {
//     sol->_Sol[solIndexeta]->add ( *sol->_Sol[solIndexh[k]] );
//   }
//   sol->_Sol[solIndexeta]->add ( -1, *sol->_Sol[solIndexb] );

}

void build_phi1Av (const unsigned &CFL_pow, const std::vector < double > &NodeJac, const std::vector< double > &Res, Vec &y, Mat &A, Mat &phi1APetsc, Mat &Temp, Mat &Temp2)
{

  //BEGIN IMPLEMENTATION WITH FOR CYCLES 
  
//   //std::cout << "scaling factor = " << (dt / pow (2, CFL_pow)) << std::endl;
// 
//   std::vector< std::vector <double > > A (NumberOfLayers);
//   std::vector< std::vector <double > > AA (NumberOfLayers);
//   std::vector< std::vector <double > > AAA (NumberOfLayers);
//   std::vector< std::vector <double > > phi1A (NumberOfLayers);
// 
//   for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
//     A[k2].assign (NumberOfLayers, 0.);
//     AA[k2].assign (NumberOfLayers, 0.);
//     AAA[k2].assign (NumberOfLayers, 0.);
//     phi1A[k2].assign (NumberOfLayers, 0.);
//   }
// 
//   for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
//     for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
//       A[k1][k2] = (dt / pow (2, CFL_pow)) * NodeJac[k1 * NumberOfLayers + k2]; //save the scaled jac in A
// 
//       if (k1 == k2) phi1A[k1][k2] = 1.; //initialize phi1A to the identity
//     }
//   }
// 
// 
// //   std::cout << "A ------------------- " << std::endl;
// //
// //   for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
// //     for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
// //       std::cout.precision(16);
// //       std::cout << A[k1][k2] * pow (2, CFL_pow) / dt << " ";
// //     }
// //
// //     std::cout << std::endl;
// //   }
// 
//   for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
//     for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
//       for (unsigned k3 = 0; k3 < NumberOfLayers; k3++) {
//         AA[k1][k2] += A[k1][k3] * A[k3][k2];
//       }
//     }
//   }
// 
//   for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
//     for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
//       for (unsigned k3 = 0; k3 < NumberOfLayers; k3++) {
//         AAA[k1][k2] += AA[k1][k3] * A[k3][k2];
//       }
//     }
//   }
// 
//   for (unsigned i = 0; i < NumberOfLayers; i++) {
//     for (unsigned j = 0; j < NumberOfLayers; j++) {
//       phi1A[i][j] += 0.5 * A[i][j] + 1. / 6. * AA[i][j] + 1. / 24. * AAA[i][j];
//     }
//   }
// 
//   for (unsigned i = 0; i < CFL_pow; i++) {
// 
//     std::vector< std::vector <double > > phi1ASquared (NumberOfLayers);
//     std::vector< std::vector <double > > Temp (NumberOfLayers);
// 
//     for (unsigned ii = 0; ii < NumberOfLayers; ii++) {
//       phi1ASquared[ii].assign (NumberOfLayers, 0.);
//       Temp[ii].assign (NumberOfLayers, 0.);
//     }
// 
//     for (unsigned ii = 0; ii < NumberOfLayers; ii++) {
//       for (unsigned jj = 0; jj < NumberOfLayers; jj++) {
//         for (unsigned kk = 0; kk < NumberOfLayers; kk++) {
//           phi1ASquared[ii][jj] += phi1A[ii][kk] * phi1A[kk][jj];
//         }
//       }
//     }
// 
//     for (unsigned ii = 0; ii < NumberOfLayers; ii++) {
//       for (unsigned jj = 0; jj < NumberOfLayers; jj++) {
//         for (unsigned kk = 0; kk < NumberOfLayers; kk++) {
//           Temp[ii][jj] += A[ii][kk] * phi1ASquared[kk][jj];
//         }
//       }
//     }
// 
//     double power = 0.5;
// 
//     if (i == 1) power = 1.;
// 
//     else if (i > 1) power = pow (2, i - 1);
// 
//     for (unsigned ii = 0; ii < NumberOfLayers; ii++) {
//       for (unsigned jj = 0; jj < NumberOfLayers; jj++) {
//         phi1A[ii][jj] += power * Temp[ii][jj];
//       }
//     }
// 
//   }
// 
// //   std::cout << "phi1A ------------------- " << std::endl;
// //
// //   for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
// //     for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
// //       std::cout.precision(16);
// //       std::cout << phi1A[k1][k2] << " ";
// //     }
// //
// //     std::cout << std::endl;
// //   }
// 
//   for (int ii = 0; ii < NumberOfLayers; ii++) {
//     double value = 0.;
// 
//     for (unsigned kk = 0; kk < NumberOfLayers; kk++) {
//       value += phi1A[ii][kk] * Res[kk];
//     }
// 
//     VecSetValues (y, 1, &ii, &value, INSERT_VALUES);
//   }
// 
//   VecAssemblyBegin (y);
//   VecAssemblyEnd (y);
// 
//   //VecView (y, PETSC_VIEWER_STDOUT_WORLD);
  
  //END IMPLEMENTATION WITH FOR CYCLES 

  
  //BEGIN PETSC NEW

  PetscInt k1;
  PetscInt k2;
  Vec v;
  
  VecCreateSeq(PETSC_COMM_SELF,NumberOfLayers,&v);

  for (k1 = 0; k1 < NumberOfLayers; k1++) {
    for (k2 = 0; k2 < NumberOfLayers; k2++) {
    double value = (dt / pow (2, CFL_pow)) * NodeJac[k1 * NumberOfLayers + k2] ;
      MatSetValue (A, k1, k2, value, INSERT_VALUES);
    }
  }
  
  MatAssemblyBegin (A, MAT_FINAL_ASSEMBLY);
  MatAssemblyEnd (A, MAT_FINAL_ASSEMBLY);
  
  MatZeroEntries(phi1APetsc);   
  for (k1 = 0; k1 < NumberOfLayers; k1++) {
    double value = 1.;
    MatSetValues (phi1APetsc, 1, &k1, 1, &k1, &value, INSERT_VALUES);
    //phi1A has been set has the identity matrix
  }
  
  MatAssemblyBegin (phi1APetsc, MAT_FINAL_ASSEMBLY);
  MatAssemblyEnd (phi1APetsc, MAT_FINAL_ASSEMBLY);

  MatMatMult (A, A, MAT_REUSE_MATRIX, PETSC_DEFAULT, &Temp);
  
  MatMatMult (Temp, A, MAT_REUSE_MATRIX, PETSC_DEFAULT, &Temp2);

  MatAXPY (phi1APetsc, 1. / 2., A, DIFFERENT_NONZERO_PATTERN);

  MatAXPY (phi1APetsc, 1. / 6., Temp, DIFFERENT_NONZERO_PATTERN);

  MatAXPY (phi1APetsc, 1. / 24., Temp2, DIFFERENT_NONZERO_PATTERN);

  for (unsigned i = 0; i < CFL_pow; i++) {

    MatMatMult (phi1APetsc, phi1APetsc, MAT_REUSE_MATRIX, PETSC_DEFAULT, &Temp);

    MatMatMult (A, Temp, MAT_REUSE_MATRIX, PETSC_DEFAULT, &Temp2);

    PetscScalar power = 0.5;

    if (i == 1) power = 1.;

    else if (i > 1) power = pow (2, i - 1);

    MatAXPY (phi1APetsc, power, Temp2, DIFFERENT_NONZERO_PATTERN);
    //phi1(2A) = (1 / 2) * (2 * I + 2^i * A * phi1A) * phi1A = phi1A + 2^(i-1)*A*(phi1A)^2;
    
  }
  
  //MatView ( phi1APetsc, PETSC_VIEWER_STDOUT_WORLD );
  
  for (k1 = 0; k1 < NumberOfLayers; k1++) {
    double value = Res[k1];
    VecSetValue (v, k1, value, INSERT_VALUES);
  }
  VecAssemblyBegin (v);
  VecAssemblyEnd (v);
  
  MatMult (phi1APetsc, v, y);
  
  VecDestroy (&v);

  //END PETSC NEW

}

void create_phi1A (const unsigned &CFL_pow, const std::vector < double > &NodeJac, std::vector< std::vector <double > > &phi1A, Mat &A, Mat &phi1APetsc, Mat &Temp, Mat &Temp2)
{

// //   std::cout << "scaling factor = " << (dt / pow (2, CFL_pow)) << std::endl;
// 
//   std::vector< std::vector <double > > Abis (NumberOfLayers);
//   std::vector< std::vector <double > > AA (NumberOfLayers);
//   std::vector< std::vector <double > > AAA (NumberOfLayers);
// 
//   phi1A.resize (NumberOfLayers);
// 
//   for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
//     Abis[k2].assign (NumberOfLayers, 0.);
//     AA[k2].assign (NumberOfLayers, 0.);
//     AAA[k2].assign (NumberOfLayers, 0.);
//     phi1A[k2].assign (NumberOfLayers, 0.);
//   }
// 
//   for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
//     for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
//       Abis[k1][k2] = (dt / pow (2, CFL_pow)) * NodeJac[k1 * NumberOfLayers + k2]; //save the scaled jac in A
// 
//       if (k1 == k2) phi1A[k1][k2] = 1.; //initialize phi1A to the identity
//     }
//   }
// 
// //   std::cout << "A ------------------- " << std::endl;
// //
// //   for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
// //     for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
// //       std::cout.precision(16);
// //       std::cout << A[k1][k2] * pow (2, CFL_pow) / dt << " ";
// //     }
// //
// //     std::cout << std::endl;
// //   }
// 
//   for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
//     for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
//       for (unsigned k3 = 0; k3 < NumberOfLayers; k3++) {
//         AA[k1][k2] += Abis[k1][k3] * Abis[k3][k2];
//       }
//     }
//   }
// 
//   for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
//     for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
//       for (unsigned k3 = 0; k3 < NumberOfLayers; k3++) {
//         AAA[k1][k2] += AA[k1][k3] * Abis[k3][k2];
//       }
//     }
//   }
// 
//   for (unsigned i = 0; i < NumberOfLayers; i++) {
//     for (unsigned j = 0; j < NumberOfLayers; j++) {
//       phi1A[i][j] += 0.5 * Abis[i][j] + 1. / 6. * AA[i][j] + 1. / 24. * AAA[i][j];
//     }
//   }
// 
//   for (unsigned i = 0; i < CFL_pow; i++) {
// 
//     std::vector< std::vector <double > > phi1ASquared (NumberOfLayers);
//     std::vector< std::vector <double > > Temp (NumberOfLayers);
// 
// 
//     for (unsigned ii = 0; ii < NumberOfLayers; ii++) {
//       phi1ASquared[ii].assign (NumberOfLayers, 0.);
//       Temp[ii].assign (NumberOfLayers, 0.);
//     }
// 
//     for (unsigned ii = 0; ii < NumberOfLayers; ii++) {
//       for (unsigned jj = 0; jj < NumberOfLayers; jj++) {
//         for (unsigned kk = 0; kk < NumberOfLayers; kk++) {
//           phi1ASquared[ii][jj] += phi1A[ii][kk] * phi1A[kk][jj];
//         }
//       }
//     }
// 
//    for (unsigned ii = 0; ii < NumberOfLayers; ii++) {
//       for (unsigned jj = 0; jj < NumberOfLayers; jj++) {
//         for (unsigned kk = 0; kk < NumberOfLayers; kk++) {
//           Temp[ii][jj] += Abis[ii][kk] * phi1ASquared[kk][jj];
//         }
//       }
//     }
// 
//     double power = 0.5;
// 
//     if (i == 1) power = 1.;
// 
//     else if (i > 1) power = pow (2, i - 1);
// 
//     for (unsigned ii = 0; ii < NumberOfLayers; ii++) {
//       for (unsigned jj = 0; jj < NumberOfLayers; jj++) {
//         phi1A[ii][jj] += power * Temp[ii][jj];
//       }
//     }
// 
//   }
// 
// //   std::cout << "phi1A ------------------- " << std::endl;
// //
// //   for (unsigned k1 = 0; k1 < NumberOfLayers; k1++) {
// //     for (unsigned k2 = 0; k2 < NumberOfLayers; k2++) {
// //       std::cout.precision(16);
// //       std::cout << phi1A[k1][k2] << " ";
// //     }
// //
// //     std::cout << std::endl;
// //   }

  //BEGIN PETSC
  phi1A.resize (3*NumberOfLayers);

  PetscInt k1;
  PetscInt k2;

  MatZeroEntries(phi1APetsc); 
  
  for (k1 = 0; k1 < 3*NumberOfLayers; k1++) {
    phi1A[k1].assign (3*NumberOfLayers, 0.);
    double value1 = 1.;
    MatSetValue (phi1APetsc, k1, k1, value1, INSERT_VALUES);
    //phi1APetsc has been set has the identity matrix
    for (k2 = 0; k2 < 3*NumberOfLayers; k2++) {
    double value2 = (dt / pow (2, CFL_pow)) * NodeJac[k1 * NumberOfLayers + k2] ;
      MatSetValue (A, k1, k2, value2, INSERT_VALUES);
    }
  }

  MatAssemblyBegin (A, MAT_FINAL_ASSEMBLY);
  MatAssemblyEnd (A, MAT_FINAL_ASSEMBLY);
  
  MatAssemblyBegin (phi1APetsc, MAT_FINAL_ASSEMBLY);
  MatAssemblyEnd (phi1APetsc, MAT_FINAL_ASSEMBLY);

  MatMatMult (A, A, MAT_REUSE_MATRIX, PETSC_DEFAULT, &Temp);
  
  MatMatMult (Temp, A, MAT_REUSE_MATRIX, PETSC_DEFAULT, &Temp2);

  MatAXPY (phi1APetsc, 1. / 2., A, DIFFERENT_NONZERO_PATTERN);

  MatAXPY (phi1APetsc, 1. / 6., Temp, DIFFERENT_NONZERO_PATTERN);

  MatAXPY (phi1APetsc, 1. / 24., Temp2, DIFFERENT_NONZERO_PATTERN);

  for (unsigned i = 0; i < CFL_pow; i++) {

    MatMatMult (phi1APetsc, phi1APetsc, MAT_REUSE_MATRIX, PETSC_DEFAULT, &Temp);

    MatMatMult (A, Temp, MAT_REUSE_MATRIX, PETSC_DEFAULT, &Temp2);

    PetscScalar power = 0.5;

    if (i == 1) power = 1.;

    else if (i > 1) power = pow (2, i - 1);

    MatAXPY (phi1APetsc, power, Temp2, DIFFERENT_NONZERO_PATTERN);
    //phi1(2A) = (1 / 2) * (2 * I + 2^i * A * phi1A) * phi1A = phi1A + 2^(i-1)*A*(phi1A)^2;
    
  }
  
  for (k1 = 0; k1 < 3*NumberOfLayers; k1++) {
    for (k2 = 0; k2 < 3*NumberOfLayers; k2++) {
      double value;
      MatGetValues (phi1APetsc, 1, &k1, 1, &k2, &value);
      phi1A[k1][k2] = value;
    }
  }

  //MatView ( phi1APetsc, PETSC_VIEWER_STDOUT_WORLD );

  //END

}










