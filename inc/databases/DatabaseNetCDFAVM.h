#ifndef DATABASE_AVM_H
#define DATABASE_AVM_H

#include "vertexQuadraticEnergy.h"
#include "voronoiQuadraticEnergy.h"
#include "DatabaseNetCDF.h"


/*! \file DatabaseNetCDFAVM.h */
//!Simple databse for reading/writing 2d AVM states
/*!
Class for a state database for an active vertex model state
the box dimensions are stored, the 2d unwrapped coordinate of vertices,
and the set of connections between vertices
*/
class AVMDatabaseNetCDF : public BaseDatabaseNetCDF
{
private:
    typedef shared_ptr<AVM2D> STATE;
    int Nv; //!< number of vertices in AVM
    int Nc; //!< number of cells in AVM
    int Nvn; //!< the number of vertex-vertex connections
    NcDim *recDim, *NvDim, *dofDim, *NvnDim, *ncDim, *boxDim, *unitDim; //!< NcDims we'll use
    NcVar *posVar, *forceVar, *vneighVar, *directorVar, *BoxMatrixVar, *timeVar; //!<NcVars we'll use
    int Current;    //!< keeps track of the current record when in write mode


public:
    //!The default constructor takes the number of *vertices* as the parameter
    AVMDatabaseNetCDF(int nv, string fn="temp.nc", NcFile::FileMode mode=NcFile::ReadOnly);
    ~AVMDatabaseNetCDF(){File.close();};

private:
    void SetDimVar();
    void GetDimVar();

public:
    int  GetCurrentRec(); //!<Return the current record of the database
    //!Get the total number of records in the database
    int GetNumRecs()
        {
        recDim = File.get_dim("rec");
        return recDim->size();
        };

    //!Write the current state of the system to the database. If the default value of "rec=-1" is used, just append the current state to a new record at the end of the database
    void WriteState(STATE c, Dscalar time = -1.0, int rec=-1);
    //!Read the "rec"th entry of the database into AVM2D state c. If geometry=true, after reading a CPU-based triangulation is performed, and local geometry of cells computed.
    void ReadState(STATE c, int rec,bool geometry=true);
};

#endif
