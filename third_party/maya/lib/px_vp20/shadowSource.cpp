//
// Copyright 2019 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//

#include "pxr/imaging/glf/glew.h"
#include "px_vp20/shadowSource.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/imaging/glf/bindingMap.h"


PXR_NAMESPACE_OPEN_SCOPE

UsdMayaShadowMap::UsdMayaShadowMap(
    GLuint texture,
    const MHWRender::MSamplerStateDesc& samplerDesc
)
    : _textureHandle(texture)
{
    GLuint samplerHandle = INVALID_GLUINT;

    // The shadow mapping technique used by Maya/VP2 (PCF-filtered color
    // channel depth) puts certain requirements on shadow samplers.
    // That's why we just assert most of the parameters here.
    glGenSamplers(1, &samplerHandle);

    TF_VERIFY(samplerDesc.addressU == MSamplerState::kTexBorder);
    glSamplerParameteri(samplerHandle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);

    TF_VERIFY(samplerDesc.addressV == MSamplerState::kTexBorder);
    glSamplerParameteri(samplerHandle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    TF_VERIFY(samplerDesc.addressW == MSamplerState::kTexBorder);
    glSamplerParameteri(samplerHandle, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);

    glSamplerParameterfv(
        samplerHandle, GL_TEXTURE_BORDER_COLOR, samplerDesc.borderColor
    );

    TF_VERIFY(samplerDesc.comparisonFn == MStateManager::kCompareAlways);
    TF_VERIFY(samplerDesc.coordCount == 2);
    TF_VERIFY(samplerDesc.elementIndex == 0);
    
    TF_VERIFY(samplerDesc.filter == MSamplerState::kMinMagMipPoint);
    TF_VERIFY(samplerDesc.maxAnisotropy == 1.0f);

    glSamplerParameteri(samplerHandle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(samplerHandle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    _sampler = _UniqueSampler(samplerHandle);
}

UsdMayaShadowMap::_UniqueSampler::_UniqueSampler(UsdMayaShadowMap::_UniqueSampler&& rhs)
{
    _handle = rhs._handle;
    rhs._handle = INVALID_GLUINT;
}

UsdMayaShadowMap::_UniqueSampler::~_UniqueSampler()
{
    if (_handle != INVALID_GLUINT)
        glDeleteSamplers(1, &_handle);
}

UsdMayaShadowMap::_UniqueSampler&
UsdMayaShadowMap::_UniqueSampler::operator=(
    UsdMayaShadowMap::_UniqueSampler&& rhs
)
{
    if (TF_VERIFY(_handle != rhs._handle)) {
        if (_handle != INVALID_GLUINT)
            glDeleteSamplers(1, &_handle);

        _handle = rhs._handle;
        rhs._handle = INVALID_GLUINT;
    }
    return *this;
}

void
UsdMayaShadowMap::BindResources(
    GlfBindingMapPtr const& bindingMap
) const
{
    if (!IsValid())
        return;

    const int samplerIndex = bindingMap->GetSamplerUnit(_samplerToken);

    glActiveTexture(GL_TEXTURE0 + samplerIndex);
    glBindTexture(GL_TEXTURE_2D, _textureHandle);
    glBindSampler(samplerIndex, _sampler.Get());
}

void
UsdMayaShadowMap::UnbindResources(
    GlfBindingMapPtr const& bindingMap
) const
{
    if (!IsValid())
        return;

    const int samplerIndex = bindingMap->GetSamplerUnit(_samplerToken);

    glActiveTexture(GL_TEXTURE0 + samplerIndex);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindSampler(samplerIndex, 0);
}

UsdMayaShadowSource::UsdMayaShadowSource()
{
}

UsdMayaShadowSource::~UsdMayaShadowSource()
{
}

void
UsdMayaShadowSource::DefineShaderMacros(std::stringstream& defineStream) const
{
    defineStream << "#define NUM_SHADOWS " << _shadowMaps.size() << "\n";
}

size_t
UsdMayaShadowSource::CombineShaderHash(size_t hash) const
{
    boost::hash_combine(hash, _shadowMaps.size());
    return hash;
}

size_t
UsdMayaShadowSource::AddShadowMap(
    GLuint texture, const MHWRender::MSamplerStateDesc& samplerDesc
)
{
    _shadowMaps.emplace_back(texture, samplerDesc);
    return _shadowMaps.size() - 1;
}

void
UsdMayaShadowSource::InitResourceBindings(GlfBindingMapPtr const &bindingMap)
{
    for (size_t iShadowMap = 0; iShadowMap < _shadowMaps.size(); ++iShadowMap)
    {
        UsdMayaShadowMap& shadowMap = _shadowMaps[iShadowMap];
        if (shadowMap.IsValid()) {
            TfToken token(TfStringPrintf("shadowTextures[%d]", iShadowMap));
            shadowMap._samplerToken = token;
            bindingMap->GetSamplerUnit(token);
        }
    }
}

void
UsdMayaShadowSource::BindResources(GlfBindingMapPtr const &bindingMap)
{
    for (const UsdMayaShadowMap& shadowMap : _shadowMaps)
        shadowMap.BindResources(bindingMap);

    glActiveTexture(GL_TEXTURE0);
}

void
UsdMayaShadowSource::UnbindResources(GlfBindingMapPtr const& bindingMap)
{
    for (const UsdMayaShadowMap& shadowMap : _shadowMaps)
        shadowMap.UnbindResources(bindingMap);

    glActiveTexture(GL_TEXTURE0);
}

void
UsdMayaShadowSource::SetViewMatrix(
    size_t index, GfMatrix4d const& matrix
)
{
    if (!TF_VERIFY(index < _shadowMaps.size()))
        return;

    _shadowMaps[index]._viewMatrix = matrix;
}

void
UsdMayaShadowSource::SetProjectionMatrix(
    size_t index, GfMatrix4d const& matrix
)
{
    if (!TF_VERIFY(index < _shadowMaps.size()))
        return;

    _shadowMaps[index]._projectionMatrix = matrix;
}

GfMatrix4d
UsdMayaShadowSource::GetWorldToShadowMatrix(size_t index) const
{
    if (!TF_VERIFY(index < _shadowMaps.size()))
        return GfMatrix4d(1.0);

    const UsdMayaShadowMap& shadowMap = _shadowMaps[index];

    // VP2 shadow projection matrices already include the conversion of Z
    // from the standard OpenGL NDC range [-1, 1] to [0, 1]
    static const GfMatrix4d sizeCenter =
        GfMatrix4d().SetScale(GfVec3d(0.5, 0.5, 1.0))
        * GfMatrix4d().SetTranslate(GfVec3d(0.5, 0.5, 0.0));

    return shadowMap._viewMatrix * shadowMap._projectionMatrix * sizeCenter;
}

PXR_NAMESPACE_CLOSE_SCOPE
