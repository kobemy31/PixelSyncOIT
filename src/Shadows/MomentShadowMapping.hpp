//
// Created by christoph on 10.02.19.
//

#ifndef PIXELSYNCOIT_MOMENTSHADOWMAPPING_HPP
#define PIXELSYNCOIT_MOMENTSHADOWMAPPING_HPP

#include <Math/Geometry/AABB3.hpp>
#include "ShadowTechnique.hpp"
#include "../OIT/OIT_MBOIT_Utils.hpp"

class MomentShadowMapping : public ShadowTechnique
{
public:
    MomentShadowMapping();
    virtual ShadowMappingTechniqueName getShadowMappingTechnique() { return MOMENT_SHADOW_MAPPING; }
    virtual void createShadowMapPass(std::function<void()> sceneRenderFunction);
    virtual void loadShaders();
    virtual void setUniformValuesCreateShadowMap();
    virtual void setUniformValuesRenderScene(sgl::ShaderProgramPtr transparencyShader);
    virtual void setShaderDefines();
    virtual void resolutionChanged();
    virtual bool renderGUI();

    // Called by MainApp
    virtual void setGatherShaderList(const std::list<std::string> &shaderIDs);
    void setSceneBoundingBox(const sgl::AABB3 &sceneBB);

private:
    // Called when new moment mode was set
    void updateMomentMode();
    // Called when some setting was changed and the shaders need to be reloaded
    void reloadShaders();

    // Gather shader name used for shading
    std::list<std::string> gatherShaderIDs = {"PseudoPhong.Vertex", "PseudoPhong.Fragment"};

    // Additional shader for clearing moment textures
    sgl::ShaderProgramPtr clearShadowMapShader;
    // Clear render data (ignores model-view-projection matrix and uses normalized device coordinates)
    sgl::ShaderAttributesPtr clearRenderData;

    // Global uniform data containing settings
    MomentOITUniformData momentUniformData;
    sgl::GeometryBufferPtr momentOITUniformBuffer;

    // Moment textures
    sgl::TexturePtr b0;
    sgl::TexturePtr b;
    sgl::TexturePtr bExtra;
    sgl::TextureSettings textureSettingsB0;
    sgl::TextureSettings textureSettingsB;
    sgl::TextureSettings textureSettingsBExtra;

    // For rendering to the moment shadow map
    sgl::FramebufferObjectPtr shadowMapFBO;

    // For computing logarithmic depth
    float logDepthMinShadow = 0.1f;
    float logDepthMaxShadow = 1.0f;
};


#endif //PIXELSYNCOIT_MOMENTSHADOWMAPPING_HPP
