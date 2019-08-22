/*=========================================================================

 Program: FEMUS
 Module: MED_IO
 Authors: Sureka Pathmanathan, Giorgio Bornia
 
 Copyright (c) FEMTTU
 All rights reserved. 

 This software is distributed WITHOUT ANY WARRANTY; without even
 the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef __femus_mesh_MED_IO_hpp__
#define __femus_mesh_MED_IO_hpp__

// Local includes
#include "MeshInput.hpp"
#include "GeomElTypeEnum.hpp"
#include "GeomElemBase.hpp"
#include "GeomElemHex27.hpp"
#include "GeomElemTet10.hpp"
#include "GeomElemQuad9.hpp"
#include "GeomElemTri6.hpp"
#include "GeomElemEdge3.hpp"

#ifdef HAVE_HDF5

#include "hdf5.h"

namespace femus
{

 #define TYPE_FOR_FAM_FLAGS  int  //do not use "unsigned" because these numbers in MED are NEGATIVE!
   
// Auxiliary struct to store group information
  struct GroupInfo {
      TYPE_FOR_FAM_FLAGS _med_flag;
      int _user_defined_flag;
      int _user_defined_property;
      GeomElemBase* _geom_el;
      int _size;
  };
    
// Forward declarations
class Mesh;


/**
 * This class implements reading meshes in the MED format.
 */

// ------------------------------------------------------------
class MED_IO : public MeshInput<Mesh>
{
 public:

  /**
   * Constructor.  Takes a non-const Mesh reference which it
   * will fill up with elements via the read() command.
   */
  explicit
  MED_IO (Mesh& mesh);
  
//   ~MED_IO();
  
  /**
   * Reads in a mesh in the neutral gambit *.neu format
   * from the ASCII file given by name.
   *
   */
  virtual void read (const std::string& name, vector < vector < double> > &coords, const double Lref, std::vector<bool> &type_elem_flag);

 private:
     
   unsigned int get_user_flag_from_med_flag(const std::vector< GroupInfo > & group_info, const TYPE_FOR_FAM_FLAGS med_flag_in ) const;

   void set_elem_group_ownership(const hid_t&  file_id, const std::string mesh_menu, const int i,  const GeomElemBase* geom_elem_per_dimension, const std::vector<GroupInfo> & group_info);
   
   void compute_group_geom_elem_and_size(const hid_t&  file_id, const std::string mesh_menu, GroupInfo & group_info)  const;

   void set_elem_connectivity(const hid_t&  file_id, const std::string mesh_menu, const unsigned i, const GeomElemBase* geom_elem_per_dimension, std::vector<bool>& type_elem_flag);
   
   void find_boundary_faces_and_set_face_flags(const hid_t&  file_id, const std::string mesh_menu, const GeomElemBase* geom_elem_per_dimension, const std::vector<GroupInfo> & group_info);

   void set_node_coordinates(const hid_t&  file_id, const std::string mesh_menu, vector < vector < double> >& coords, const double Lref);

   const GroupInfo                get_group_flags_per_mesh(const std::string & group_names) const;
   
   const std::vector< GroupInfo > get_group_vector_flags_per_mesh(const hid_t &  file_id, const std::string & mesh_menu) const;
   
   const std::vector<std::string>  get_mesh_names(const hid_t & file_id) const;
     
   std::pair<int,int>  isolate_number_in_string(const std::string & string_in, const int begin_pos_to_investigate) const;
      
   /** Determine mesh dimension from mesh file */
   const std::vector< GeomElemBase* >  set_mesh_dimension_and_get_geom_elems_by_looping_over_element_types(const hid_t &  file_id, const std::string & menu_name); //this cannot be const because it sets the dimension in the mesh

   GeomElemBase * get_geom_elem_from_med_name(const  std::string el_type) const;

   /** Read FE type */
  const std::vector< GeomElemBase* > get_geom_elem_type_per_dimension(const hid_t & file_id, const std::string my_mesh_name_dir);   //@todo this should be const
   
   /** Map from Salome vertex index to Femus vertex index */
   static const unsigned MEDToFemusVertexIndex[N_GEOM_ELS][MAX_EL_N_NODES]; 
 
   /** Map from Salome face index to Femus face index */
   static const unsigned MEDToFemusFaceIndex[N_GEOM_ELS][MAX_EL_N_FACES];

//    std::vector<char*> menu_names;
   static const std::string mesh_ensemble;             //ENS_MAA
   static const std::string aux_zeroone;               // -0000000000000000001-0000000000000000001
   static const std::string elem_list;                 //MAI
   static const std::string group_fam;                 //FAM
   static const std::string connectivity;              //NOD    //These are written based on the NOE/NUM numbering !
   static const std::string node_or_elem_global_num;   //NUM    //this is the MED global numbering (as you see in Salome) both for nodes (in NOE) and for elements of all dimensions (in MAI). 
                                                       //Salome global Numbering of both Nodes and Elements starts at 1.
                                                       //For Elements, lower dimensional elements are numbered first
   static const std::string node_list;                 //NOE
   static const std::string coord_list;                //COO
   static const std::string group_ensemble;            //FAS
   static const std::string group_elements;            //ELEME
   static const std::string group_nodes;               //NOEUD  //if you ever use group of nodes, not used now
   static const uint max_length;

   std::vector< GeomElemBase* > _geom_elems;
   
};

inline
MED_IO::MED_IO (Mesh& mesh) :
   MeshInput<Mesh>  (mesh)
{
    
       _geom_elems.resize(N_GEOM_ELS);
       
       _geom_elems[0] = new GeomElemHex27();
       _geom_elems[1] = new GeomElemTet10();
//        _geom_elems[2] = new GeomElemWedge18();
       _geom_elems[3] = new GeomElemQuad9();
       _geom_elems[4] = new GeomElemTri6();
       _geom_elems[5] = new GeomElemEdge3();
    
}


// MED_IO::~MED_IO() {
//     
//         for(unsigned g = 0; g < _geom_elems.size(); g++) delete _geom_elems[g];
// 
// }

// @todo hybrid meshes (MED can export them)
// @todo groups with more than one type of element (in the sense hybrid, but at the same dimension)
// @todo mesh with overlapping groups  (MED can export them, defining the intersection independently; now the name parsing wouldn't work with them; Salome splits into non-intersecting family, so one needs to read the GRO/NOM dataset )
// @todo how would I use a 0d element? or a ball?
// @todo triggering of the copy of new input files happens right after "Configure" of cmake
// @todo Redirect CERR together with COUT
// @todo Update Xdmf to Xdmf3
// @todo not running in parallel
// @todo I know I can make only groups of edges/faces/volumes, but can I do hybrid faces, say quad and tri together?
// @todo Generate MED format from other software such as Gmsh
// @todo FEHex20, FEQuad8, FETri7 missing

} // namespace femus

#endif 

#endif

