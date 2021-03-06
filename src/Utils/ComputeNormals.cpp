/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2019, Christoph Neuhauser
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <map>
#include <Utils/Convert.hpp>
#include <Utils/File/Logfile.hpp>
#include "ComputeNormals.hpp"
#include <iostream>
#include <algorithm>

/**
 * Creates normals for the specified indexed vertex set.
 * NOTE: If a vertex is indexed by more than one triangle, then the average normal is stored per vertex.
 * If you want to have non-smooth normals, then make sure each vertex is only referenced by one face.
 */
void computeNormals(
        const std::vector<glm::vec3> &vertices,
        const std::vector<uint32_t> &indices,
        std::vector<glm::vec3> &normals,
        std::vector<float> &attributes)
{
    // For finding all triangles with a specific index. Maps vertex index -> first triangle index.
    sgl::Logfile::get()->writeInfo(std::string() + "Creating index map for "
            + sgl::toString(indices.size()) + " indices...");
    std::vector<std::vector<uint32_t>> indexMap;
    indexMap.resize(vertices.size());

#pragma omp parallel for
    for (size_t j = 0; j < indices.size(); j++) {
        size_t vindex = indices.at(j);
        size_t faceIndex = j / 3;

        indexMap[vindex].push_back(faceIndex);
    }

    std::vector<glm::vec3> faceNormals(indices.size() / 3);
    sgl::Logfile::get()->writeInfo(std::string() + "Computing face normals for "
                                   + sgl::toString(faceNormals.size()) + " faces...");
#pragma omp parallel for
    for (size_t f = 0; f < faceNormals.size(); ++f)
    {
        size_t vertIndex = f * 3;
        size_t i1 = indices.at(vertIndex), i2 = indices.at(vertIndex+1), i3 = indices.at(vertIndex+2);
        glm::vec3 faceNormal = glm::cross(vertices.at(i3) - vertices.at(i1), vertices.at(i2) - vertices.at(i1));
//        faceNormal = glm::normalize(faceNormal);
        // don't normalize weights as triangle area is encoded in cross product
        // area is then used to weight contribution of normal to average normal at each vertex
        faceNormals[f] = faceNormal;
    }

    sgl::Logfile::get()->writeInfo(std::string() + "Computing normals for "
            + sgl::toString(vertices.size()) + " vertices...");
    normals.resize(vertices.size());

#pragma omp parallel for
    for (size_t i = 0; i < vertices.size(); i++) {

        glm::vec3 normal(0.0f, 0.0f, 0.0f);
        int numTrianglesSharedBy = 0;

        const auto& faceIndices = indexMap[i];
        for (const auto& face : faceIndices)
        {
//            size_t vertIndex = face * 3;
//            size_t i1 = indices.at(vertIndex), i2 = indices.at(vertIndex+1), i3 = indices.at(vertIndex+2);
//            glm::vec3 faceNormal = glm::cross(vertices.at(i3) - vertices.at(i1), vertices.at(i2) - vertices.at(i1));
            const glm::vec3& faceNormal = faceNormals[face];
            normal += faceNormal;
            numTrianglesSharedBy++;
        }

        if (numTrianglesSharedBy == 0) {
            sgl::Logfile::get()->writeError("Error in createNormals: numTrianglesSharedBy == 0");
            exit(1);
        }
        normal /= (float)numTrianglesSharedBy;
        normal = glm::normalize(normal);
        normals[i] = normal;
    }

    sgl::Logfile::get()->writeInfo(std::string() + "Computing curvature for "
                                   + sgl::toString(vertices.size()) + " vertices...");
    attributes.resize(vertices.size());

#pragma omp parallel for
    for (size_t i = 0; i < vertices.size(); i++)
    {
        // min curvature
//        float k1 = std::numeric_limits<float>::max();
//        float k2 = -std::numeric_limits<float>::min();

        const auto& faceIndices = indexMap[i];
        const glm::vec3& n0 = normals[i];
        const glm::vec3& p0 = vertices[i];

        // find edges
        std::vector<uint32_t> ring1Vertices;

        for (const auto& face : faceIndices)
        {
            size_t vertIndex = face * 3;
//            size_t i1 = indices.at(vertIndex), i2 = indices.at(vertIndex+1), i3 = indices.at(vertIndex+2);
            std::vector<uint32_t> idx = { indices[vertIndex], indices[vertIndex+1], indices[vertIndex+2] };

            for (auto j = 0; j < 3; ++j)
            {
                auto vID = idx[j];
                if (vID == i) { continue; }

                if (std::find(ring1Vertices.begin(), ring1Vertices.end(), vID) == ring1Vertices.end())
                {
                    ring1Vertices.push_back(vID);
                }
            }
        }

        // compute curvature
        std::vector<float> ring1Curvatures(ring1Vertices.size(), 0);

        for (auto v = 0; v < ring1Vertices.size(); ++v)
        {
            const uint32_t vID = ring1Vertices[v];

            const glm::vec3& n1 = normals[vID];
            const glm::vec3& p1 = vertices[vID];

            const glm::vec3 n = n1 - n0;
            const glm::vec3 p = p1 - p0;

            const float l2 = glm::length(p) * glm::length(p);

            ring1Curvatures[v] = glm::dot(n, p) / l2;
        }

        // compute edge curvatures

        double totalCurvature = 0;
        double totalAngle = 0;

        for (auto e = 0; e < ring1Vertices.size() - 1; ++e)
        {
            const glm::vec3& p1 = vertices[ring1Vertices[e]];
            const glm::vec3& p2 = vertices[ring1Vertices[e + 1]];

            // compute edge angle
            glm::vec3 edge0 = p1 - p0;
            glm::vec3 edge1 = p2 - p0;
            glm::vec3 product = glm::cross(edge0, edge1);
            double sineValue = glm::length(product) / (glm::length(edge0), glm::length(edge1));
            double angle = glm::asin(std::min(1.0, sineValue));

            totalAngle += angle;
            totalCurvature += angle * (ring1Curvatures[e] + ring1Curvatures[e + 1]);
        }

        totalCurvature = totalCurvature / (2 * totalAngle);
        attributes[i] = totalCurvature;

        ring1Curvatures.clear();
        ring1Curvatures.shrink_to_fit();

        ring1Vertices.clear();
        ring1Vertices.shrink_to_fit();

        // loop over faces
//        for (const auto& face : faceIndices)
//        {
//            size_t vertIndex = face * 3;
////            size_t i1 = indices.at(vertIndex), i2 = indices.at(vertIndex+1), i3 = indices.at(vertIndex+2);
//            std::vector<uint32_t> idx = { indices[vertIndex], indices[vertIndex+1], indices[vertIndex+2] };
//
//            for (auto j = 0; j < 3; ++j)
//            {
//                auto vID = idx[j];
//                if (vID == i) { continue; }
//
//                // compute curvature
//                const glm::vec3& v1 = vertices[vID];
//                const glm::vec3& n1 = normals[vID];
//
//                auto pD = glm::normalize(v1 - v0);
//
//                float k = glm::dot((n1 - n0), pD);
//
//                k1 = std::min(k, k1);
//                k2 = std::max(k, k2);
//            }
//        }

        //float meanCurvature = (k1 + k2) / 2;
//        float gaussianCurvature = k1 * k2;
//        attributes[i] = meanCurvature;
    }


    // try to free memory for large data
    std::cout << "Free memory" << std::endl << std::flush;
    faceNormals.clear();
    faceNormals.shrink_to_fit();
    indexMap.clear();
    indexMap.shrink_to_fit();
    std::cout << "Free memory done." << std::endl << std::flush;
}
