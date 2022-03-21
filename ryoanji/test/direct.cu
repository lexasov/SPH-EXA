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
 * @brief Direct kernel comparison against the CPU
 *
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#include "gtest/gtest.h"

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>

#include "dataset.hpp"
#include "ryoanji/gpu_config.h"
#include "ryoanji/direct.cuh"
#include "ryoanji/cpu/treewalk.hpp"

using namespace ryoanji;

TEST(DirectSum, MatchCpu)
{
    using T       = double;
    int npOnEdge  = 10;
    int numBodies = npOnEdge * npOnEdge * npOnEdge;

    // the CPU reference uses mass softening, while the GPU P2P kernel still uses plummer softening
    // so the only way to compare is without softening in both versions and make sure that
    // particles are not on top of each other
    T eps = 0.0;

    std::vector<T> x(numBodies), y(numBodies), z(numBodies), m(numBodies);
    ryoanji::makeGridBodies(x.data(), y.data(), z.data(), m.data(), npOnEdge, 0.5);

    // upload to device
    thrust::device_vector<T> d_x = x, d_y = y, d_z = z, d_m = m;
    thrust::device_vector<T> p(numBodies), ax(numBodies), ay(numBodies), az(numBodies);

    directSum(numBodies,
              rawPtr(d_x.data()),
              rawPtr(d_y.data()),
              rawPtr(d_z.data()),
              rawPtr(d_m.data()),
              rawPtr(p.data()),
              rawPtr(ax.data()),
              rawPtr(ay.data()),
              rawPtr(az.data()),
              eps);

    // download body accelerations
    thrust::host_vector<T> h_p = p, h_ax = ax, h_ay = ay, h_az = az;

    std::vector<T> h(numBodies, 0);
    T              G = 1.0;

    std::vector<T> refP(numBodies), refAx(numBodies), refAy(numBodies), refAz(numBodies);
    directSum(x.data(),
              y.data(),
              z.data(),
              h.data(),
              m.data(),
              numBodies,
              G,
              refAx.data(),
              refAy.data(),
              refAz.data(),
              refP.data());

    for (int i = 0; i < numBodies; ++i)
    {
        Vec3<T> ref   = {refAx[i], refAy[i], refAz[i]};
        Vec3<T> probe = {h_ax[i], h_ay[i], h_az[i]};

        EXPECT_NEAR(h_ax[i], refAx[i], 1e-6);
        EXPECT_NEAR(h_ay[i], refAy[i], 1e-6);
        EXPECT_NEAR(h_az[i], refAz[i], 1e-6);
        EXPECT_NEAR(refP[i], h_p[i], 1e-6);

        // printf("%f %f %f\n", ref[1], ref[2], ref[3]);
        // printf("%f %f %f\n", probe[1], probe[2], probe[3]);
    }
}
