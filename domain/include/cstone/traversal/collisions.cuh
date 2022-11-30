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
 * @brief  GPU driver for halo discovery using traversal of an octree
 *
 * @author Sebastian Keller <sebastian.f.keller@gmail.com>
 */

#pragma once

#include "cstone/traversal/collisions.hpp"

namespace cstone
{

/*! @brief mark halo nodes with flags
 *
 * @tparam KeyType               32- or 64-bit unsigned integer
 * @tparam RadiusType            float or double, float is sufficient for 64-bit codes or less
 * @tparam T                     float or double
 * @param[in]  nodePrefixes      Warren-Salmon node keys of the octree, length = numTreeNodes
 * @param[in]  childOffsets      child offsets array, length = numTreeNodes
 * @param[in]  toInternalOrder   map cstone leaf node indices to fully linked format
 * @param[in]  toLeafOrder       map leaf node indices of fully linked format to cornerstone order
 * @param[in]  interactionRadii  effective halo search radii per octree (leaf) node
 * @param[in]  box               coordinate bounding box
 * @param[in]  firstNode         first cstone leaf node index to consider as local
 * @param[in]  lastNode          last cstone leaf node index to consider as local
 * @param[out] collisionFlags    array of length numLeafNodes, each node that is a halo
 *                               from the perspective of [firstNode:lastNode] will be marked
 *                               with a non-zero value.
 *                               Note: does NOT reset non-colliding indices to 0, so @p collisionFlags
 *                               should be zero-initialized prior to calling this function.
 */
template<class KeyType, class RadiusType, class T>
__global__ void findHalosKernel(const KeyType* nodePrefixes,
                                const TreeNodeIndex* childOffsets,
                                const TreeNodeIndex* toInternalOrder,
                                const TreeNodeIndex* toLeafOrder,
                                const RadiusType* interactionRadii,
                                const Box<T> box,
                                TreeNodeIndex firstNode,
                                TreeNodeIndex lastNode,
                                int* collisionFlags)
{
    unsigned leafIdx = blockIdx.x * blockDim.x + threadIdx.x + firstNode;

    auto markCollisions = [collisionFlags, toLeafOrder](TreeNodeIndex i) { collisionFlags[toLeafOrder[i]] = 1; };

    if (leafIdx < lastNode)
    {
        RadiusType radius      = interactionRadii[leafIdx];
        TreeNodeIndex idx      = toInternalOrder[leafIdx];
        KeyType lowerTargetKey = decodePlaceholderBit(nodePrefixes[idx]);
        KeyType upperTargetKey = lowerTargetKey + nodeRange<KeyType>(decodePrefixLength(nodePrefixes[idx]) / 3);
        IBox haloBox           = makeHaloBox(lowerTargetKey, upperTargetKey, radius, box);

        KeyType firstPrefix = nodePrefixes[toInternalOrder[firstNode]];
        KeyType lastPrefix  = nodePrefixes[toInternalOrder[lastNode - 1]];
        KeyType lowestKey   = decodePlaceholderBit(firstPrefix);
        KeyType highestKey  = decodePlaceholderBit(lastPrefix) + nodeRange<KeyType>(decodePrefixLength(lastPrefix) / 3);

        // if the halo box is fully inside the assigned SFC range, we skip collision detection
        if (containedIn(lowestKey, highestKey, haloBox)) { return; }

        // mark all colliding node indices outside [lowestKey:highestKey]
        findCollisions(nodePrefixes, childOffsets, markCollisions, haloBox, lowestKey, highestKey);
    }
}

//! @brief convenience kernel wrapper
template<class KeyType, class RadiusType, class T>
void findHalosGpu(const KeyType* nodePrefixes,
                  const TreeNodeIndex* childOffsets,
                  const TreeNodeIndex* toInternalOrder,
                  const TreeNodeIndex* toLeafOrder,
                  const RadiusType* interactionRadii,
                  const Box<T>& box,
                  TreeNodeIndex firstNode,
                  TreeNodeIndex lastNode,
                  int* collisionFlags)
{
    constexpr unsigned numThreads = 128;
    unsigned numBlocks            = iceil(lastNode - firstNode, numThreads);

    findHalosKernel<<<numBlocks, numThreads>>>(nodePrefixes, childOffsets, toInternalOrder, toLeafOrder,
                                               interactionRadii, box, firstNode, lastNode, collisionFlags);
}

} // namespace cstone
