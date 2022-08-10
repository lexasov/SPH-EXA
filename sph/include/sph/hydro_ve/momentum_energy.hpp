/*
 * MIT License
 *
 * Copyright (c) 2021 CSCS, ETH Zurich
 *               2021 University of Basel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*! @file
 * @brief Gradient of pressure and energy i-loop driver
 *
 * @author Ruben Cabezon <ruben.cabezon@unibas.ch>
 */

#pragma once

#include "sph/sph.cuh"
#include "sph/particles_data_stubs.hpp"

#include "momentum_energy_kern.hpp"

namespace sph
{

template<class T, class Dataset>
void computeMomentumEnergyImpl(size_t startIndex, size_t endIndex, int ngmax, Dataset& d, const cstone::Box<T>& box)
{
    const int* neighbors      = d.neighbors.data();
    const int* neighborsCount = d.neighborsCount.data();

    const T* h     = d.h.data();
    const T* m     = d.m.data();
    const T* x     = d.x.data();
    const T* y     = d.y.data();
    const T* z     = d.z.data();
    const T* vx    = d.vx.data();
    const T* vy    = d.vy.data();
    const T* vz    = d.vz.data();
    const T* c     = d.c.data();
    const T* prho  = d.prho.data();
    const T* alpha = d.alpha.data();

    const T* c11 = d.c11.data();
    const T* c12 = d.c12.data();
    const T* c13 = d.c13.data();
    const T* c22 = d.c22.data();
    const T* c23 = d.c23.data();
    const T* c33 = d.c33.data();

    T* du       = d.du.data();
    T* grad_P_x = d.ax.data();
    T* grad_P_y = d.ay.data();
    T* grad_P_z = d.az.data();

    const T* wh  = d.wh.data();
    const T* whd = d.whd.data();
    const T* kx  = d.kx.data();
    const T* xm  = d.xm.data();

    const T K         = d.K;
    const T sincIndex = d.sincIndex;
    const T Atmin     = d.Atmin;
    const T Atmax     = d.Atmax;
    const T ramp      = d.ramp;

    T minDt = INFINITY;

#pragma omp parallel for schedule(static) reduction(min : minDt)
    for (size_t i = startIndex; i < endIndex; ++i)
    {
        size_t ni = i - startIndex;
        int    nc = stl::min(neighborsCount[i], ngmax);

        T maxvsignal = 0;

        momentumAndEnergyJLoop(i, sincIndex, K, box, neighbors + ngmax * ni, nc, x, y, z, vx, vy, vz, h, m, prho, c,
                               c11, c12, c13, c22, c23, c33, Atmin, Atmax, ramp, wh, whd, kx, xm, alpha, grad_P_x,
                               grad_P_y, grad_P_z, du, &maxvsignal);

        T dt_i = tsKCourant(maxvsignal, h[i], c[i], d.Kcour);
        minDt  = std::min(minDt, dt_i);
    }

    d.minDt_loc = minDt;
}

template<class T, class Dataset>
void computeMomentumEnergy(size_t startIndex, size_t endIndex, int ngmax, Dataset& d, const cstone::Box<T>& box)
{
    if constexpr (cstone::HaveGpu<typename Dataset::AcceleratorType>{})
    {
        cuda::computeMomentumEnergy(startIndex, endIndex, ngmax, d, box);
    }
    else { computeMomentumEnergyImpl(startIndex, endIndex, ngmax, d, box); }
}

} // namespace sph
