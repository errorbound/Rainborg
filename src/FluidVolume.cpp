#include "FluidVolume.h"
#include <iostream>

FluidVolume::FluidVolume(scalar minX, scalar maxX, scalar minY, scalar maxY, scalar minZ, scalar maxZ,
            int numParticles, fluid_volume_mode_t mode, bool random)
: m_minX(minX)
, m_maxX(maxX)
, m_minY(minY)
, m_maxY(maxY)
, m_minZ(minZ)
, m_maxZ(maxZ)
, m_numParticles(numParticles)
, m_mode(mode)
, m_random(random) {

    scalar xwid = maxX - minX;
    scalar ywid = maxY - minY;
    scalar zwid = maxZ - minZ;

    scalar inv_dens = (xwid*ywid*zwid) / numParticles;
    m_dens_cbrt = std::cbrt(inv_dens);
}

void FluidVolume::setParticlePositions(Vector3s* pos, int offset) {

    if (m_mode == kFLUID_VOLUME_MODE_BOX) {

        if (m_random) {

            scalar x;
            scalar y;
            scalar z;
            for(int i = 0; i < m_numParticles; ++i){
                x = static_cast <scalar> (rand()) / static_cast<scalar>(RAND_MAX);
                y = static_cast <scalar> (rand()) / static_cast<scalar>(RAND_MAX);
                z = static_cast <scalar> (rand()) / static_cast<scalar>(RAND_MAX);
                x = x*(m_maxX - m_minX) + m_minX;
                y = y*(m_maxY - m_minY) + m_minY;
                z = z*(m_maxZ - m_minZ) + m_minZ;
                pos[offset+i] = Vector3s(x, y, z);
            }
        }
        else {
            int i=0;
            for (scalar x = m_minX; x < m_maxX; x += m_dens_cbrt) {
                for (scalar y = m_minY; y < m_maxY; y += m_dens_cbrt) {
                    for (scalar z = m_minZ; z < m_maxZ; z += m_dens_cbrt) {

                        if (i == m_numParticles){
                            // std::cout << "stopping at " << x << ", " << y << ", " << z << std::endl;
                            return;
                        }
                        pos[offset+i] = Vector3s(x, y, z);
                        i++;
                    }
                }
            }
            if (i < m_numParticles) {
                // just in case we have more particles than we planned for
                for (scalar y = m_minY; y < m_maxY; y += m_dens_cbrt) {
                    for (scalar z = m_minZ; z < m_maxZ; z += m_dens_cbrt) {

                        if (i == m_numParticles) return;
                        pos[offset+i] = Vector3s(m_maxX, y, z);
                        i++;
                    }
                }
            }
            assert(i >= m_numParticles);
        }
    }
    else if (m_mode == kFLUID_VOLUME_MODE_SPHERE) {
        // not implemented
        assert(false);
    }
}