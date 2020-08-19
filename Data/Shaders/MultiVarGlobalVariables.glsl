// Current number of variables to display
uniform int numVariables;
// Maximum number of variables to display
uniform int maxNumVariables;

// Number of instances for rendering
uniform uint numInstances;
// Radius of tubes
uniform float radius;

// Structs for SSBOs
struct VarData
{
    float value;
};

struct LineDescData
{
    float startIndex;
};

struct VarDescData
{
//    float startIndex;
//    vec2 minMax;
//    float dummy;
    vec4 info;
};

struct LineVarDescData
{
    vec4 minMax;
};

// SSBOs which contain all data for all variables per trajectory
layout (std430, binding = 2) buffer VariableArray
{
    float varArray[];
};

layout (std430, binding = 3) buffer LineDescArray
{
    float lineDescs[];
};

layout (std430, binding = 4) buffer VarDescArray
{
    VarDescData varDescs[];
};

layout (std430, binding = 5) buffer LineVarDescArray
{
    LineVarDescData lineVarDescs[];
};

layout (std430, binding = 6) buffer VarSelectedArray
{
    uint selectedVars[];
};

// Sample the actual variable ID from the current user selection
int sampleActualVarID(in uint varID)
{
    if (varID < 0 || varID >= maxNumVariables)
    {
        return -1;
    }

    uint index = varID + 1;

    uint numSelected = 0;
    // HACK: change to dynamic variable
    for (int c = 0; c < maxNumVariables; ++c)
    {
        if (selectedVars[c] > 0)
        {
            numSelected++;
        }

        if (numSelected >= index)
        {
            return c;
        }
    }

    return -1;
}

// Function to sample from SSBOs
void sampleVariableFromLineSSBO(in uint lineID, in uint varID, in uint elementID,
                                out float value, out vec2 minMax)
{
    uint startIndex = uint(lineDescs[lineID]);
    VarDescData varDesc = varDescs[maxNumVariables * lineID + varID];
    const uint varOffset = uint(varDesc.info.r);
    // Output
    minMax = varDesc.info.gb;
    value = varArray[startIndex + varOffset + elementID];
}

// Function to sample distribution from SSBO
void sampleVariableDistributionFromLineSSBO(in uint lineID, in uint varID, out vec2 minMax)
{
    LineVarDescData lineVarDesc = lineVarDescs[maxNumVariables * lineID + varID];
    minMax = lineVarDesc.minMax.xy;
}