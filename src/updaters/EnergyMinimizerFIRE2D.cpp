#include "EnergyMinimizerFIRE2D.h"
#include "EnergyMinimizerFIRE2D.cuh"
#include "utilities.cuh"

/*! \file EnergyMinimizerFIRE2D.cpp
 */

/*!
Initialize the minimizer with a reference to a target system, set a bunch of default parameters.
Of note, the current default is CPU operation
*/
EnergyMinimizerFIRE::EnergyMinimizerFIRE(shared_ptr<Simple2DModel> system, bool usegpu)
    {
    GPUcompute = usegpu;
    set2DModel(system);
    initializeParameters();
    initializeFromModel();
    };

/*!
Initialize the minimizer with some default parameters. that do not depend on N
*/
void EnergyMinimizerFIRE::initializeParameters()
    {
    if(!GPUcompute)
        {
        force.neverGPU =true;
        velocity.neverGPU =true;
        displacements.neverGPU =true;
        forceDotVelocity.neverGPU =true;
        forceDotForce.neverGPU =true;
        velocityDotVelocity.neverGPU =true;
        sumReductionIntermediate.neverGPU =true;
        sumReductions.neverGPU =true;
        }
    sumReductions.resize(3);
    iterations = 0;
    Power = 0;
    NSinceNegativePower = 0;
    forceMax = 100.;
    setMaximumIterations(1000);
    setForceCutoff(1e-7);
    setAlphaStart(0.99);
    setDeltaT(0.01);
    setDeltaTMax(.1);
    setDeltaTInc(1.05);
    setDeltaTDec(0.95);
    setAlphaDec(.9);
    setNMin(5);
    setGPU();
    };


/*!
Initialize the minimizer with some default parameters.
\pre requires a Simple2DModel (to set N correctly) to be already known
*/
void EnergyMinimizerFIRE::initializeFromModel()
    {
    N = State->getNumberOfDegreesOfFreedom();
    forceDotForce.resize(N);
    forceDotVelocity.resize(N);
    velocityDotVelocity.resize(N);
    force.resize(N);
    velocity.resize(N);
    displacements.resize(N);
    sumReductionIntermediate.resize(N);
    ArrayHandle<double2> h_f(force);
    ArrayHandle<double2> h_v(velocity);
    double2 zero; zero.x = 0.0; zero.y = 0.0;
    for(int i = 0; i <N; ++i)
        {
        h_f.data[i]=zero;
        h_v.data[i]=zero;
        };
    };

/*!
 * Call the correct velocity Verlet routine
 */
void EnergyMinimizerFIRE::velocityVerlet()
    {
    if (GPUcompute)
        velocityVerletGPU();
    else
        velocityVerletCPU();
    };

/*!
 * Call the correct FIRE step routine
 */
void EnergyMinimizerFIRE::fireStep()
    {
    if (GPUcompute)
        fireStepGPU();
    else
        fireStepCPU();
    };

/*!
 * Perform a velocity verlet integration step on the GPU
 */
void EnergyMinimizerFIRE::velocityVerletGPU()
    {
    //calculated displacements and update velocities
    if (true) //scope for array handles
        {
        ArrayHandle<double2> d_f(force,access_location::device,access_mode::read);
        ArrayHandle<double2> d_v(velocity,access_location::device,access_mode::readwrite);
        ArrayHandle<double2> d_d(displacements,access_location::device,access_mode::overwrite);
        gpu_displacement_velocity_verlet(d_d.data,d_v.data,d_f.data,deltaT,N);
        gpu_update_velocity(d_v.data,d_f.data,deltaT,N);
        };
    //move particles and update forces
    State->moveDegreesOfFreedom(displacements);
    State->enforceTopology();
    State->computeForces();
    State->getForces(force);

    //update velocities again
    ArrayHandle<double2> d_f(force,access_location::device,access_mode::read);
    ArrayHandle<double2> d_v(velocity,access_location::device,access_mode::readwrite);
    gpu_update_velocity(d_v.data,d_f.data,deltaT,N);
    };


/*!
 * Perform a velocity verlet integration step on the CPU
 */
void EnergyMinimizerFIRE::velocityVerletCPU()
    {
    if(true) // scope for array handles
        {
        ArrayHandle<double2> h_f(force);
        ArrayHandle<double2> h_v(velocity);
        ArrayHandle<double2> h_d(displacements);
        for (int i = 0; i < N; ++i)
            {
            //update displacement
            h_d.data[i].x = deltaT*h_v.data[i].x+0.5*deltaT*deltaT*h_f.data[i].x;
            h_d.data[i].y = deltaT*h_v.data[i].y+0.5*deltaT*deltaT*h_f.data[i].y;
            //do first half of velocity update
            h_v.data[i].x += 0.5*deltaT*h_f.data[i].x;
            h_v.data[i].y += 0.5*deltaT*h_f.data[i].y;
            };
        };
    //move particles, then update the forces
    State->moveDegreesOfFreedom(displacements);
    State->enforceTopology();
    State->computeForces();
    State->getForces(force);

    //update second half of velocity vector based on new forces
    ArrayHandle<double2> h_f(force);
    ArrayHandle<double2> h_v(velocity);
    for (int i = 0; i < N; ++i)
        {
        h_v.data[i].x += 0.5*deltaT*h_f.data[i].x;
        h_v.data[i].y += 0.5*deltaT*h_f.data[i].y;
        };
    };

/*!
 * Perform a FIRE minimization step on the GPU
 */
void EnergyMinimizerFIRE::fireStepGPU()
    {
    Power = 0.0;
    forceMax = 0.0;
    if(true)//scope for array handles
        {
        //ArrayHandle<double2> d_f(force,access_location::device,access_mode::read);
        ArrayHandle<double2> d_f(State->returnForces(),access_location::device,access_mode::read);
        ArrayHandle<double2> d_v(velocity,access_location::device,access_mode::readwrite);
        ArrayHandle<double> d_ff(forceDotForce,access_location::device,access_mode::readwrite);
        ArrayHandle<double> d_fv(forceDotVelocity,access_location::device,access_mode::readwrite);
        ArrayHandle<double> d_vv(velocityDotVelocity,access_location::device,access_mode::readwrite);
        gpu_dot_double2_vectors(d_f.data,d_f.data,d_ff.data,N);
        gpu_dot_double2_vectors(d_f.data,d_v.data,d_fv.data,N);
        gpu_dot_double2_vectors(d_v.data,d_v.data,d_vv.data,N);
        //parallel reduction
        if (true)//scope for reduction arrays
            {
            ArrayHandle<double> d_intermediate(sumReductionIntermediate,access_location::device,access_mode::overwrite);
            ArrayHandle<double> d_assist(sumReductions,access_location::device,access_mode::overwrite);
            gpu_parallel_reduction(d_ff.data,d_intermediate.data,d_assist.data,0,N);
            gpu_parallel_reduction(d_fv.data,d_intermediate.data,d_assist.data,1,N);
            gpu_parallel_reduction(d_vv.data,d_intermediate.data,d_assist.data,2,N);
            };
        ArrayHandle<double> h_assist(sumReductions,access_location::host,access_mode::read);
        double forceNorm = h_assist.data[0];
        Power = h_assist.data[1];
        double velocityNorm = h_assist.data[2];
        forceMax = forceNorm / (double)N;
        double scaling = 0.0;
        if(forceNorm > 0.)
            scaling = sqrt(velocityNorm/forceNorm);
        gpu_update_velocity_FIRE(d_v.data,d_f.data,alpha,scaling,N);
        };

    if (Power > 0)
        {
        if (NSinceNegativePower > NMin)
            {
            deltaT = min(deltaT*deltaTInc,deltaTMax);
            alpha = alpha * alphaDec;
            };
        NSinceNegativePower += 1;
        }
    else
        {
        deltaT = deltaT*deltaTDec;
        alpha = alphaStart;
        ArrayHandle<double2> d_v(velocity,access_location::device,access_mode::overwrite);
        gpu_zero_velocity(d_v.data,N);
        };
    };

/*!
 * Perform a FIRE minimization step on the CPU
 */
void EnergyMinimizerFIRE::fireStepCPU()
    {
    Power = 0.0;
    forceMax = 0.0;
    if (true)//scope for array handles
        {
        //calculate the power, and precompute norms of vectors
        ArrayHandle<double2> h_f(force);
        ArrayHandle<double2> h_v(velocity);
        double forceNorm = 0.0;
        double velocityNorm = 0.0;
        for (int i = 0; i < N; ++i)
            {
            Power += dot(h_f.data[i],h_v.data[i]);
            double fdot = dot(h_f.data[i],h_f.data[i]);
            if (fdot > forceMax) forceMax = fdot;
            forceNorm += fdot;
            velocityNorm += dot(h_v.data[i],h_v.data[i]);
            };
        double scaling = 0.0;
        if(forceNorm > 0.)
            scaling = sqrt(velocityNorm/forceNorm);
        //adjust the velocity according to the FIRE algorithm
        for (int i = 0; i < N; ++i)
            {
            h_v.data[i].x = (1.0-alpha)*h_v.data[i].x + alpha*scaling*h_f.data[i].x;
            h_v.data[i].y = (1.0-alpha)*h_v.data[i].y + alpha*scaling*h_f.data[i].y;
            };
        };

    if (Power > 0)
        {
        if (NSinceNegativePower > NMin)
            {
            deltaT = min(deltaT*deltaTInc,deltaTMax);
            alpha = alpha * alphaDec;
            //alpha = max(alpha, 0.75);
            };
        NSinceNegativePower += 1;
        }
    else
        {
        deltaT = deltaT*deltaTDec;
        deltaT = max (deltaT,deltaTMin);
        alpha = alphaStart;
        ArrayHandle<double2> h_v(velocity);
        for (int i = 0; i < N; ++i)
            {
            h_v.data[i].x = 0.0;
            h_v.data[i].y = 0.0;
            };
        };
    };

/*!
 * Perform a FIRE minimization step on the CPU
 */
void EnergyMinimizerFIRE::minimize()
    {
    if (N != State->getNumberOfDegreesOfFreedom())
        initializeFromModel();
    //initialize the forces?
    State->computeForces();
    State->getForces(force);
    forceMax = 110.0;
    while( (iterations < maxIterations) && (sqrt(forceMax) > forceCutoff) )
        {
        iterations +=1;
        velocityVerlet();
        fireStep();
        };
        printf("step %i max force:%.3g \tpower: %.3g\t alpha %.3g\t dt %g \n",iterations,sqrt(forceMax),Power,alpha,deltaT);
    };

/*!
A utility function to help test the parallel reduction routines
 */
void EnergyMinimizerFIRE::parallelReduce(GPUArray<double> &vec)
    {
    int n = vec.getNumElements();
    //
    if(true)
        {
        double sum = 0.0;
        ArrayHandle<double> v(vec);
        for (int i = 0; i < n; ++i)
            sum += v.data[i];
        printf("CPU-based reduction: %f\n",sum);
        };
    if(true)
    {
    ArrayHandle<double> input(vec,access_location::device,access_mode::read);
    ArrayHandle<double> intermediate(sumReductionIntermediate,access_location::device,access_mode::overwrite);
    ArrayHandle<double> output(sumReductions,access_location::device,access_mode::overwrite);
    gpu_parallel_reduction(input.data,
            intermediate.data,
            output.data,
            0,n);
    };
    ArrayHandle<double> output(sumReductions);
    printf("GPU-based reduction: %f\n",output.data[0]);
    };
