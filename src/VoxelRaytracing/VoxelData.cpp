//
// Created by christoph on 04.10.18.
//

#include <cstring>
#include <cassert>
#include <fstream>
#include <iostream>

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <Utils/Events/Stream/Stream.hpp>
#include <Utils/File/Logfile.hpp>
#include <Utils/Convert.hpp>
#include <Math/Math.hpp>
#include <Graphics/Renderer.hpp>
#include <Graphics/OpenGL/Texture.hpp>

#include "../TransferFunctionWindow.hpp"
#include "VoxelData.hpp"

/**
 * New in version 4: Support for non-uniform grids.
 */
const uint32_t VOXEL_GRID_FORMAT_VERSION = 4u;

void saveToFile(const std::string &filename, const VoxelGridDataCompressed &data)
{
    std::ofstream file(filename.c_str(), std::ofstream::binary);
    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in saveToFile: File \"" + filename + "\" not found.");
        return;
    }

    sgl::BinaryWriteStream stream;
    stream.write((uint32_t)VOXEL_GRID_FORMAT_VERSION);
    stream.write(data.gridResolution);
    stream.write(data.quantizationResolution);
    stream.write(data.worldToVoxelGridMatrix);
    stream.write(data.dataType);

    if (data.dataType == 0u) {
        stream.write(data.maxVorticity);
        stream.writeArray(data.attributes);
    } else if (data.dataType == 1u) {
        stream.write(data.hairStrandColor);
        stream.write(data.hairThickness);
    }

    stream.writeArray(data.voxelLineListOffsets);
    stream.writeArray(data.numLinesInVoxel);
    stream.writeArray(data.voxelDensities);
    stream.writeArray(data.voxelAOFactors);
    stream.writeArray(data.lineSegments);
    std::cout << "Number of line segments written: " << data.lineSegments.size() << std::endl;
    std::cout << "Buffer size (in MB): " << (stream.getSize() / 1024. / 1024.) << std::endl;

    file.write((const char*)stream.getBuffer(), stream.getSize());
    file.close();
}

void loadFromFile(const std::string &filename, VoxelGridDataCompressed &data)
{
    std::ifstream file(filename.c_str(), std::ifstream::binary);
    if (!file.is_open()) {
        sgl::Logfile::get()->writeError(std::string() + "Error in loadFromFile: File \"" + filename + "\" not found.");
        return;
    }

    file.seekg(0, file.end);
    size_t size = file.tellg();
    file.seekg(0);
    char *buffer = new char[size];
    file.read(buffer, size);

    sgl::BinaryReadStream stream(buffer, size);
    uint32_t version;
    stream.read(version);
    if (version != VOXEL_GRID_FORMAT_VERSION) {
        sgl::Logfile::get()->writeError(std::string() + "Error in loadFromFile: Invalid version in file \""
                                        + filename + "\".");
        return;
    }

    stream.read(data.gridResolution);
    stream.read(data.quantizationResolution);
    stream.read(data.worldToVoxelGridMatrix);

    if (version > 1u) {
        stream.read(data.dataType);

        if (data.dataType == 0u) {
            stream.read(data.maxVorticity);
            stream.readArray(data.attributes);
        } else if (data.dataType == 1u) {
            stream.read(data.hairStrandColor);
            stream.read(data.hairThickness);
        }
    } else {
        data.dataType = 0u;
        data.maxVorticity = 0.0f;
    }

    stream.readArray(data.voxelLineListOffsets);
    stream.readArray(data.numLinesInVoxel);
    stream.readArray(data.voxelDensities);
    stream.readArray(data.voxelAOFactors);
    stream.readArray(data.lineSegments);

    //delete[] buffer; // BinaryReadStream does deallocation
    file.close();
}


std::vector<float> generateMipmapsForDensity(float *density, glm::ivec3 size)
{
    std::vector<float> allLODs;
    size_t memorySize = 0;
    for (glm::ivec3 lodSize = size; lodSize.x > 0 && lodSize.y > 0 && lodSize.z > 0; lodSize /= 2) {
        memorySize += lodSize.x * lodSize.y * lodSize.z;
    }
    allLODs.reserve(memorySize);

    for (int i = 0; i < size.x * size.y * size.z; i++) {
        allLODs.push_back(density[i]);
    }

    float *lodData = new float[size.x * size.y * size.z];
    float *lodDataLast = new float[size.x * size.y * size.z];
    memcpy(lodDataLast, density, size.x * size.y * size.z * sizeof(float));
    for (glm::ivec3 lodSize = size/2; lodSize.x > 0 && lodSize.y > 0 && lodSize.z > 0; lodSize /= 2) {
        // Averaging operation
        for (int z = 0; z < lodSize.z; z++) {
            for (int y = 0; y < lodSize.y; y++) {
                for (int x = 0; x < lodSize.x; x++) {
                    int childIdx = z*lodSize.y*lodSize.x + y*lodSize.x + x;
                    lodData[childIdx] = 0;
                    for (int offsetZ = 0; offsetZ < 2; offsetZ++) {
                        for (int offsetY = 0; offsetY < 2; offsetY++) {
                            for (int offsetX = 0; offsetX < 2; offsetX++) {
                                int parentIdx = (z*2+offsetZ)*lodSize.y*lodSize.x*4
                                                + (y*2+offsetY)*lodSize.y*2 + x*2+offsetX;
                                lodData[childIdx] += lodDataLast[parentIdx];
                            }
                        }
                    }
                    lodData[childIdx] /= 8.0f;
                    allLODs.push_back(lodData[childIdx]);
                }
            }
        }
        float *tmp = lodData;
        lodData = lodDataLast;
        lodDataLast = tmp;
        /*int N = lodSize.x * lodSize.y * lodSize.z;
        for (int i = 0; i < N; i++) {
            lodData[i] = (lodDataLast[i*8]+lodDataLast[i*8+1]
                          + lodDataLast[i*8 + lodSize.x*2]+lodDataLast[i*8+1 + lodSize.x*2]
                          + lodDataLast[i*8 + lodSize.x*lodSize.y*4]+lodDataLast[i*8+1 + lodSize.x*lodSize.y*4]
                          + lodDataLast[i*8 + lodSize.x*2 + lodSize.x*lodSize.y*4]
                            +lodDataLast[i*8+1 + lodSize.x*2 + lodSize.x*lodSize.y*4]);
            lodData[i] /= 8.0f;
            allLODs.push_back(lodData[i]);
        }*/
    }

    delete[] lodData;
    delete[] lodDataLast;
    return allLODs;
}


std::vector<uint32_t> generateMipmapsForOctree(uint32_t *numLines, glm::ivec3 size)
{
    std::vector<uint32_t> allLODs;
    size_t memorySize = 0;
    for (glm::ivec3 lodSize = size; lodSize.x > 0 && lodSize.y > 0 && lodSize.z > 0; lodSize /= 2) {
        memorySize += lodSize.x * lodSize.y * lodSize.z;
    }
    allLODs.reserve(memorySize);

    for (int i = 0; i < size.x * size.y * size.z; i++) {
        allLODs.push_back(numLines[i]);
    }

    uint32_t *lodData = new uint32_t[size.x * size.y * size.z];
    uint32_t *lodDataLast = new uint32_t[size.x * size.y * size.z];
    memcpy(lodDataLast, numLines, size.x * size.y * size.z * sizeof(uint32_t));
    for (glm::ivec3 lodSize = size/2; lodSize.x > 0 && lodSize.y > 0 && lodSize.z > 0; lodSize /= 2) {
        // Sum operation
        for (int z = 0; z < lodSize.z; z++) {
            for (int y = 0; y < lodSize.y; y++) {
                for (int x = 0; x < lodSize.x; x++) {
                    int childIdx = z*lodSize.y*lodSize.x + y*lodSize.x + x;
                    lodData[childIdx] = 0;
                    for (int offsetZ = 0; offsetZ < 2; offsetZ++) {
                        for (int offsetY = 0; offsetY < 2; offsetY++) {
                            for (int offsetX = 0; offsetX < 2; offsetX++) {
                                int parentIdx = (z*2+offsetZ)*lodSize.y*lodSize.x*4
                                        + (y*2+offsetY)*lodSize.y*2 + x*2+offsetX;
                                lodData[childIdx] += lodDataLast[parentIdx];
                            }
                        }
                    }
                    allLODs.push_back(lodData[childIdx] > 0 ? 1 : 0);
                }
            }
        }
        uint32_t *tmp = lodData;
        lodData = lodDataLast;
        lodDataLast = tmp;
        /*int N = lodSize.x * lodSize.y * lodSize.z;
        for (int i = 0; i < N; i++) {
            lodData[i] = (lodDataLast[i*8]+lodDataLast[i*8+1]
                          + lodDataLast[i*8 + lodSize.x*2]+lodDataLast[i*8+1 + lodSize.x*2]
                          + lodDataLast[i*8 + lodSize.x*lodSize.y*4]+lodDataLast[i*8+1 + lodSize.x*lodSize.y*4]
                          + lodDataLast[i*8 + lodSize.x*2 + lodSize.x*lodSize.y*4]
                            +lodDataLast[i*8+1 + lodSize.x*2 + lodSize.x*lodSize.y*4]);
            allLODs.push_back(lodData[i]);
        }*/
    }

    delete[] lodData;
    delete[] lodDataLast;
    return allLODs;
}


sgl::TexturePtr generateDensityTexture(const std::vector<float> &lods, glm::ivec3 size)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_3D, textureID);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, size.x, size.y, size.z, 0, GL_RED, GL_FLOAT, &lods.front());

    sgl::TextureSettings textureSettings;
    textureSettings.type = sgl::TEXTURE_3D;
    return sgl::TexturePtr(new sgl::TextureGL(textureID, size.x, size.y, size.z, textureSettings));
}



void compressedToGPUData(const VoxelGridDataCompressed &compressedData, VoxelGridDataGPU &gpuData)
{
    gpuData.gridResolution = compressedData.gridResolution;
    gpuData.quantizationResolution = compressedData.quantizationResolution;
    gpuData.worldToVoxelGridMatrix = compressedData.worldToVoxelGridMatrix;

    gpuData.voxelLineListOffsets = sgl::Renderer->createGeometryBuffer(
            sizeof(uint32_t)*compressedData.voxelLineListOffsets.size(),
            (void*)&compressedData.voxelLineListOffsets.front());
    gpuData.numLinesInVoxel = sgl::Renderer->createGeometryBuffer(
            sizeof(uint32_t)*compressedData.numLinesInVoxel.size(),
            (void*)&compressedData.numLinesInVoxel.front());

    /*auto octreeLODs = compressedData.octreeLODs;
    for (uint32_t &value : octreeLODs) {
        value = 1;
    }*/

    gpuData.densityTexture = generateDensityTexture(compressedData.voxelDensities, gpuData.gridResolution);
    gpuData.aoTexture = generateDensityTexture(compressedData.voxelAOFactors, gpuData.gridResolution);

#ifdef PACK_LINES
    int baseSize = sizeof(LineSegmentCompressed);
#else
    int baseSize = sizeof(LineSegment);
#endif

    gpuData.lineSegments = sgl::Renderer->createGeometryBuffer(
            baseSize*compressedData.lineSegments.size(),
            (void*)&compressedData.lineSegments.front());
}


void generateBoxBlurKernel(float *filterKernel, int filterSize)
{
    const float FILTER_NUM_FIELDS = filterSize*filterSize*filterSize;
    const int FILTER_EXTENT = (filterSize - 1) / 2;

    for (int offsetZ = -FILTER_EXTENT; offsetZ <= FILTER_EXTENT; offsetZ++) {
        for (int offsetY = -FILTER_EXTENT; offsetY <= FILTER_EXTENT; offsetY++) {
            for (int offsetX = -FILTER_EXTENT; offsetX <= FILTER_EXTENT; offsetX++) {
                int filterIdx = offsetZ*filterSize*filterSize + offsetY*filterSize + offsetX;
                filterKernel[filterIdx] = 1.0f / FILTER_NUM_FIELDS;
            }
        }
    }
}
void generateGaussianBlurKernel(float *filterKernel, int filterSize, float sigma)
{
    const float FILTER_NUM_FIELDS = filterSize*filterSize*filterSize;
    const int FILTER_EXTENT = (filterSize - 1) / 2;

    for (int offsetZ = -FILTER_EXTENT; offsetZ <= FILTER_EXTENT; offsetZ++) {
        for (int offsetY = -FILTER_EXTENT; offsetY <= FILTER_EXTENT; offsetY++) {
            for (int offsetX = -FILTER_EXTENT; offsetX <= FILTER_EXTENT; offsetX++) {
                int filterIdx = (offsetZ+FILTER_EXTENT)*filterSize*filterSize + (offsetY+FILTER_EXTENT)*filterSize
                        + (offsetX+FILTER_EXTENT);
                filterKernel[filterIdx] = 1.0f / (sgl::TWO_PI * sigma * sigma)
                        * std::exp(-(offsetX*offsetX + offsetY*offsetY + offsetZ*offsetZ) / (2.0f * sigma * sigma));
            }
        }
    }
}

// Divides all values by the maximum value.
void normalizeVoxelAOFactors(std::vector<float> &voxelAOFactors, glm::ivec3 size, bool isHairDataset)
{
    // Find maximum and normalize the values
    float maxAccumDensity = 0.0f;
#pragma omp parallel for reduction(max:maxAccumDensity)
    for (int gz = 0; gz < size.z; gz++) {
        for (int gy = 0; gy < size.y; gy++) {
            for (int gx = 0; gx < size.x; gx++) {
                int readIdx = gz*size.y*size.x + gy*size.x + gx;
                maxAccumDensity = std::max(maxAccumDensity, voxelAOFactors[readIdx]);
            }
        }
    }
    std::cout << "Maximum accumulated density: " << maxAccumDensity << std::endl;

    // Now divide all the values by the maximum, and save 1 - density as occlusion factor.
#pragma omp parallel for
    for (int gz = 0; gz < size.z; gz++) {
        for (int gy = 0; gy < size.y; gy++) {
            for (int gx = 0; gx < size.x; gx++) {
                int writeIdx = gz*size.y*size.x + gy*size.x + gx;
                if (isHairDataset) {
                    voxelAOFactors[writeIdx] = 1.0f - glm::clamp((voxelAOFactors[writeIdx] / maxAccumDensity) * 3.0f, 0.0f, 1.0f);
                } else {
                    voxelAOFactors[writeIdx] = 1.0f - glm::clamp((voxelAOFactors[writeIdx] / maxAccumDensity - 0.1f) * 2.0f, 0.0f, 1.0f);
                }
            }
        }
    }
}

void generateVoxelAOFactorsFromDensity(const std::vector<float> &voxelDensities, std::vector<float> &voxelAOFactors,
                                       glm::ivec3 size, bool isHairDataset)
{
    const int FILTER_SIZE = 7;
    const int FILTER_EXTENT = (FILTER_SIZE - 1) / 2;
    const int FILTER_NUM_FIELDS = FILTER_SIZE*FILTER_SIZE*FILTER_SIZE;
    float blurKernel[FILTER_NUM_FIELDS];
    generateGaussianBlurKernel(blurKernel, FILTER_SIZE, FILTER_EXTENT);

    // 1. Filter the densities
    #pragma omp parallel for
    for (int gz = 0; gz < size.z; gz++) {
        for (int gy = 0; gy < size.y; gy++) {
            for (int gx = 0; gx < size.x; gx++) {
                int writeIdx = gz*size.y*size.x + gy*size.x + gx;
                voxelAOFactors[writeIdx] = 0.0f;
                // 3x3 box filter
                for (int offsetZ = -FILTER_EXTENT; offsetZ <= FILTER_EXTENT; offsetZ++) {
                    for (int offsetY = -FILTER_EXTENT; offsetY <= FILTER_EXTENT; offsetY++) {
                        for (int offsetX = -FILTER_EXTENT; offsetX <= FILTER_EXTENT; offsetX++) {
                            int readX = gx + offsetX;
                            int readY = gy + offsetY;
                            int readZ = gz + offsetZ;
                            if (readX >= 0 && readY >= 0 && readZ >= 0 && readX < size.x
                                    && readY < size.y && readZ < size.z) {
                                int filterIdx = (offsetZ+FILTER_EXTENT)*FILTER_SIZE*FILTER_SIZE
                                        + (offsetY+FILTER_EXTENT)*FILTER_SIZE + (offsetX+FILTER_EXTENT);
                                int readIdx = readZ*size.y*size.x + readY*size.y + readX;
                                voxelAOFactors[writeIdx] += voxelDensities[readIdx] * blurKernel[filterIdx];
                            }
                        }
                    }
                }
            }
        }
    }

    normalizeVoxelAOFactors(voxelAOFactors, size, isHairDataset);
}



// Uses global transfer function window handle to get transfer function.
float opacityMapping(float attr, float maxAttr) {
    assert(g_TransferFunctionWindowHandle);
    attr = glm::clamp(attr/maxAttr, 0.0f, 1.0f);
    return g_TransferFunctionWindowHandle->getOpacityAtAttribute(attr);
}
