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
 * @brief Turbulence simulation data initialization
 *
 * @author Axel Sanz <axelsanzlechuga@gmail.com>
 */

#pragma once

#include <iostream>

#include "sph/hydro_turb/turbulence_data.hpp"
#include "sph/hydro_turb/st_ounoise.hpp"

namespace sph
{

/*! @brief TODO: Short description of this function
 *
 * @tparam       T                 float or double
 * @param[inout] d                 the turbulence dataset
 * @param[in]    Lx
 * @param[in]    Ly
 * @param[in]    Lz
 * @param[in]    st_maxmodes
 * @param[in]    st_energy
 * @param[in]    st_stirmax
 * @param[in]    st_stirmin
 * @param[in]    ndim
 * @param[in]    st_seed
 * @param[in]    st_spectform
 * @param[in]    st_power_law_exp
 * @param[in]    st_angles_exp
 *
 * This function does ???
 */
template<class T>
void createStirringModes(TurbulenceData<T>& d, T Lx, T Ly, T Lz, size_t st_maxmodes, T st_energy, T st_stirmax,
                         T st_stirmin, size_t ndim, long int& st_seed, size_t st_spectform, T st_power_law_exp,
                         T st_angles_exp)
{
    size_t ikx, iky, ikz, st_tot_nmodes;
    T      kx, ky, kz, k, kc, amplitude, parab_prefact;

    size_t  nang, ikmin, ikmax;
    T       rand, phi, theta;
    const T twopi = 2.0 * M_PI;

    d.variance = std::sqrt(st_energy / d.decayTime);

    // prefactor for amplitude normalistion to 1 at kc = 0.5*(st_stirmin+st_stirmax)
    parab_prefact = -4.0 / ((st_stirmax - st_stirmin) * (st_stirmax - st_stirmin));

    // characteristic k for scaling the amplitude below
    kc = st_stirmin;
    if (st_spectform == 1) { kc = 0.5 * (st_stirmin + st_stirmax); }

    // this makes the rms force const irrespective of the solenoidal weight
    d.solWeight = std::sqrt(3.0) * std::sqrt(3.0 / T(ndim)) /
                  std::sqrt(1.0 - 2.0 * d.stSolWeight + T(ndim) * d.stSolWeight * d.stSolWeight);

    size_t ikxmin = 0;
    size_t ikymin = 0;
    size_t ikzmin = 0;

    size_t ikxmax = 256;
    size_t ikymax = (ndim > 1) ? 256 : 0;
    size_t ikzmax = (ndim > 2) ? 256 : 0;

    // determine the number of required modes (in case of full sampling)
    d.numModes = 0;
    for (ikx = ikxmin; ikx <= ikxmax; ikx++)
    {
        kx = twopi * ikx / Lx;
        for (iky = ikymin; iky <= ikymax; iky++)
        {
            ky = twopi * iky / Ly;
            for (ikz = ikzmin; ikz <= ikzmax; ikz++)
            {
                kz = twopi * ikz / Lz;
                k  = std::sqrt(kx * kx + ky * ky + kz * kz);
                if (k >= st_stirmin && k <= st_stirmax)
                {
                    d.numModes += 1;
                    if (ndim > 1) { d.numModes += 1; }
                    if (ndim > 2) { d.numModes += 2; }
                }
            }
        }
    }
    st_tot_nmodes = d.numModes;

    d.numModes = -1;

    if (st_spectform != 2)
    {
        std::cout << "Generating " << st_tot_nmodes << " driving modes..." << std::endl;
        // for band and parabolic spectrum, use the standard full sampling
        // loop over all kx, ky, kz to generate driving modes
        for (ikx = ikxmin; ikx <= ikxmax; ikx++)
        {
            kx = twopi * ikx / Lx;
            for (iky = ikymin; iky <= ikymax; iky++)
            {
                ky = twopi * iky / Ly;
                for (ikz = ikzmin; ikz <= ikzmax; ikz++)
                {
                    kz = twopi * ikz / Lz;
                    k  = std::sqrt(kx * kx + ky * ky + kz * kz);

                    if ((k >= st_stirmin) && (k <= st_stirmax))
                    {

                        if ((d.numModes + 1 + std::pow(2, ndim - 1)) > st_maxmodes)
                        {
                            std::cout << "init_stir:  number of modes: = " << d.numModes + 1
                                      << " maxstirmodes = " << st_maxmodes << std::endl;
                            std::cout << "Too many stirring modes" << std::endl;
                            break;
                        }

                        if (st_spectform == 0) { amplitude = 1.0; } // Band
                        if (st_spectform == 1)
                        {
                            amplitude = std::abs(parab_prefact * (k - kc) * (k - kc) + 1.0);
                        } // Parabola

                        // note: power spectrum ~ amplitude^2 (1D), amplitude^2 * 2pi k (2D), amplitude^2 * 4pi k^2 (3D)
                        amplitude = 2.0 * std::sqrt(amplitude) * std::pow((kc / k), 0.5 * (ndim - 1));

                        d.numModes += 1;
                        d.amplitudes[d.numModes] = amplitude;

                        d.modes[ndim * d.numModes]     = kx;
                        d.modes[ndim * d.numModes + 1] = ky;
                        d.modes[ndim * d.numModes + 2] = kz;

                        if (ndim > 1)
                        {
                            d.numModes += 1;
                            d.amplitudes[d.numModes] = amplitude;

                            d.modes[ndim * d.numModes]     = kx;
                            d.modes[ndim * d.numModes + 1] = -ky;
                            d.modes[ndim * d.numModes + 2] = kz;
                        }

                        if (ndim > 2)
                        {
                            d.numModes += 1;
                            d.amplitudes[d.numModes] = amplitude;

                            d.modes[ndim * d.numModes]     = kx;
                            d.modes[ndim * d.numModes + 1] = ky;
                            d.modes[ndim * d.numModes + 2] = -kz;

                            d.numModes += 1;
                            d.amplitudes[d.numModes] = amplitude;

                            d.modes[ndim * d.numModes]     = kx;
                            d.modes[ndim * d.numModes + 1] = -ky;
                            d.modes[ndim * d.numModes + 2] = -kz;
                        }

                        if (d.numModes % 1000 == 0)
                        {
                            std::cout << " ..." << d.numModes << " of total " << st_tot_nmodes << " modes generated..."
                                      << std::endl;
                        }
                    } // in k range
                }     // ikz
            }         // iky
        }             // ikx
    }

    if (st_spectform == 2)
    {
        std::cout << "There would be " << st_tot_nmodes
                  << " driving modes, if k-space were fully sampled (st_angles_exp = 2.0)..." << std::endl;
        std::cout << "Here we are using st_angles_exp = " << st_angles_exp << std::endl;

        // loop between smallest and largest k
        ikmin = std::max(1, int(st_stirmin * Lx / twopi + 0.5));
        ikmax = int(st_stirmax * Lx / twopi + 0.5);

        std::cout << "Generating driving modes within k = [ " << ikmin << " , " << ikmax << " ]" << std::endl;

        for (int ik = ikmin; ik <= ikmax; ik++)
        {
            nang = std::pow(2, ndim) * ceil(std::pow(ik, st_angles_exp));
            std::cout << "ik = " << ik << " , number of angles = " << nang << std::endl;
            for (int iang = 1; iang <= nang; iang++)
            {
                phi = twopi * sph::ran1s<T>(st_seed); // phi = [0,2pi] sample the whole sphere
                if (ndim == 1)
                {
                    if (phi < twopi / 2) { phi = 0.0; }
                    if (phi >= twopi / 2) { phi = twopi / 2.0; }
                }

                theta = twopi / 4.0;
                if (ndim > 2)
                {
                    theta = std::acos(1.0 - 2.0 * sph::ran1s<T>(st_seed));
                } // theta = [0,pi] sample the whole sphere

                rand = ik + sph::ran1s<T>(st_seed) - 0.5;
                kx   = twopi * std::round(rand * std::sin(theta) * std::cos(phi)) / Lx;
                ky   = 0.0;
                if (ndim > 1) { ky = twopi * std::round(rand * std::sin(theta) * std::sin(phi)) / Ly; }
                kz = 0.0;
                if (ndim > 2) { kz = twopi * std::round(rand * std::cos(theta)) / Lz; }

                k = std::sqrt(kx * kx + ky * ky + kz * kz);

                if ((k >= st_stirmin) && (k <= st_stirmax))
                {
                    if ((d.numModes + 1 + std::pow(2, ndim - 1)) > st_maxmodes)
                    {
                        std::cout << "init_stir:  number of modes: = " << d.numModes + 1
                                  << " maxstirmodes = " << st_maxmodes << std::endl;
                        std::cout << "Too many stirring modes" << std::endl;
                        break;
                    }

                    amplitude = std::pow(k / kc, st_power_law_exp); // Power law

                    // note: power spectrum ~ amplitude^2 (1D), amplitude^2 * 2pi k (2D), amplitude^2 * 4pi k^2 (3D)
                    // ...and correct for the number of angles sampled relative to the full sampling (k^2 per k-shell in
                    // 3D)
                    amplitude = std::sqrt(amplitude * (std::pow(ik, ndim - 1) * 4.0 * (std::sqrt(3.0)) / nang)) *
                                std::pow(kc / k, (ndim - 1) / 2.0);

                    d.numModes = d.numModes + 1;

                    d.amplitudes[d.numModes] = amplitude;

                    d.modes[ndim * d.numModes]     = kx;
                    d.modes[ndim * d.numModes + 1] = ky;
                    d.modes[ndim * d.numModes + 2] = kz;

                    if ((d.numModes + 1) % 1000 == 0)
                    {
                        std::cout << "... " << d.numModes << " modes generated..." << std::endl;
                    }

                } // in k range
            }     // loop over angles
        }         // loop over k
    }             // st_spectform .eq. 2
    d.numModes += 1;
}

} // namespace sph