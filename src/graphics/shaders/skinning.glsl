// MAX_SKIN_JOINTS由资源服务根据设备uniform能力填写，不能直接假定64矩阵总能链接。
layout(location=6) in uvec4 aJoints;
layout(location=7) in vec4 aWeights;
uniform bool skinEnabled;
uniform mat4 jointMatrices[MAX_SKIN_JOINTS];

mat4 skinTransform()
{
    if (!skinEnabled) { return mat4(1); }
    return jointMatrices[aJoints.x] * aWeights.x + jointMatrices[aJoints.y] * aWeights.y +
        jointMatrices[aJoints.z] * aWeights.z + jointMatrices[aJoints.w] * aWeights.w;
}
