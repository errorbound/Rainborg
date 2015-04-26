#ifndef __SERIAL_FLUID_H__
#define __SERIAL_FLUID_H__

#include <vector>
#include <cstring>
#include <iostream>

#include "MathDefs.h"
#include "FluidBoundingBox.h"
#include "FluidVolume.h"
#include "Fluid.h"

class Scene;

class SerialFluid : public Fluid{

public:

    SerialFluid(scalar mass, scalar p0, scalar h, int iters, int maxNeigh = 20, int minNeighbor = 3);
    SerialFluid(const SerialFluid& otherFluid);
    virtual ~SerialFluid();

    virtual void stepSystem(Scene& scene, scalar dt);
    virtual void setFPMass(scalar fpm); 
    virtual void setRestDensity(scalar p0);
    // void setFPPos(int fp, const Vector3s& pos);
    virtual void setFPVel(int fp, const Vector3s& vel);
    virtual void setKernelH(scalar h);
    virtual void setNumIterations(int iter); 
    virtual void setBoundingBox(FluidBoundingBox& newBound);
    virtual void setColor(int i, const Vector4s& col); 
    virtual void insertFluidVolume(FluidVolume& volume);

    virtual void loadFluidVolumes();

    virtual int getNumParticles() const;
    virtual int getNumIterations() const;
    virtual int getMaxNeighbors() const;
    virtual int getMinNeighbors() const; 
    virtual scalar getFPMass() const;
    virtual scalar getRestDensity() const;
    virtual scalar getKernelH() const;
    //scalar* getFPPos() const;
    //scalar* getFPVel() const;
    virtual Vector3s* getFPPos() const;
    virtual Vector3s* getFPVel() const; 
    virtual Vector4s* getColors() const;
    virtual const FluidBoundingBox& getBoundingBox() const;
    virtual const std::vector<FluidVolume>& getFluidVolumes() const;

private: 

    void accumulateForce(Scene& scene);     
    void updateVelocityFromForce(scalar dt); 
    void updatePredPosition(scalar dt); 
    void clearGrid();
    void buildGrid(); // how to parallelize? 
//    void getGridIdx(scalar x, scalar y, scalar z, int& idx); // make separate in case of smarter coallesced memory access    
    void getGridIdx(Vector3s &pos, int& idx, int& idy, int &idz);  
    int getGridIdx(int i, int j, int k); 

    void calculatePressures(); 
    void calculateLambdas(); 
    void calculatedPos(); 
  
    void dealWithCollisions(Scene& scene); // ... Deal with scene collisions
    void preserveOwnBoundary(); // Make sure within own bounding box

    Vector3s calcGradConstraint(Vector3s& pi, Vector3s& pj);
    Vector3s calcGradConstraintAtI(int p);  

    void applydPToPredPos(); 
    void recalculateVelocity(scalar dt); 
    // Vorticity confinement, XSPH
    void updateFinalPosition();     
    
    
    int m_maxNeighbors; 
    int m_minNeighbors; // minimum number of neighbors to do calculations

    int *m_grid; // grid of particles, x, then y, then z, size w*h*d*maxNeighbors
    int *m_gridCount; // store number of particles in grid so far (needed for GPU)
    int *m_gridInd; // indices for grid per particle
    int m_gridX; 
    int m_gridY; 
    int m_gridZ; 

    scalar m_fpmass; // float particle mass, shared by all
    scalar m_p0; // rest pressure
    scalar *m_pcalc; // calculated pressure
    //scalar *m_pos; // actual positinos
    //scalar *m_ppos; // predicted positions
    scalar *m_lambda; // calculated lambdas 
    //scalar *m_dpos; // change in positions
    //scalar *m_vel; 

    Vector3s *m_pos; 
    Vector3s *m_ppos; 
    Vector3s *m_dpos; 
    Vector3s *m_vel; 

    scalar m_eps; 
    scalar m_h; // kernel radius
    int m_iters; // how many iterations through solver?

    //scalar *m_accumForce; 
    Vector3s *m_accumForce;

    FluidBoundingBox m_boundingBox; 

    // Colors? 
    Vector4s *m_colors;     

    std::vector<FluidVolume> m_volumes;
 
};


#endif

