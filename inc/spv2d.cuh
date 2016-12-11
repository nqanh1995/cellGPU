#ifndef __SPV2D_CUH__
#define __SPV2D_CUH__


#include "std_include.h"
#include <cuda_runtime.h>
#include "indexer.h"
#include "gpubox.h"


bool gpu_init_curand(curandState *states,
                    int N,
                    int Timestep
                    );


bool gpu_displace_and_rotate(
                    Dscalar2 *d_points,
                    Dscalar2 *d_force,
                    Dscalar  *directors,
                    Dscalar2 *d_motility,
                    int N,
                    Dscalar dt,
                    int seed,
                    curandState *states,
                    gpubox &Box
                    );

bool gpu_compute_geometry(
                    Dscalar2 *d_points,
                    Dscalar2 *d_AP,
                    int    *d_nn,
                    int    *d_n,
                    Dscalar2 *d_vc,
                    Dscalar4 *d_vln,
                    int    N,
                    Index2D &n_idx,
                    gpubox &Box
                    );

bool gpu_force_sets(
                    Dscalar2 *d_points,
                    Dscalar2 *d_AP,
                    Dscalar2 *d_APpref,
                    int2   *d_delSets,
                    int    *d_detOther,
                    Dscalar2 *d_vc,
                    Dscalar4 *d_vln,
                    Dscalar2 *d_forceSets,
                    int2    *d_nidx,
                    Dscalar  KA,
                    Dscalar  KP,
                    int    NeighIdxNum,
                    Index2D &n_idx,
                    gpubox &Box
                    );

bool gpu_force_sets_tensions(
                    Dscalar2 *d_points,
                    Dscalar2 *d_AP,
                    Dscalar2 *d_APpref,
                    int2   *d_delSets,
                    int    *d_detOther,
                    Dscalar2 *d_vc,
                    Dscalar4 *d_vln,
                    Dscalar2 *d_forceSets,
                    int2   *d_nidx,
                    int    *d_cellTypes,
                    Dscalar  KA,
                    Dscalar  KP,
                    Dscalar  gamma,
                    int    NeighIdxNum,
                    Index2D &n_idx,
                    gpubox &Box
                    );

bool gpu_sum_force_sets(
                    Dscalar2 *d_forceSets,
                    Dscalar2 *d_forces,
                    int    *d_nn,
                    int     N,
                    Index2D &n_idx
                    );

bool gpu_sum_force_sets_with_exclusions(
                    Dscalar2 *d_forceSets,
                    Dscalar2 *d_forces,
                    Dscalar2 *d_external_forces,
                    int    *d_exes,
                    int    *d_nn,
                    int     N,
                    Index2D &n_idx
                    );




#endif

