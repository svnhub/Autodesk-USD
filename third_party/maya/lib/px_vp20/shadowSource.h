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

#ifndef PXRUSDMAYAGL_SHADOWSOURCE_H_
#define PXRUSDMAYAGL_SHADOWSOURCE_H_

#include "pxr/base/tf/token.h"
#include "pxr/imaging/glf/shadowSource.h"
#include <maya/MStateManager.h>

PXR_NAMESPACE_OPEN_SCOPE

class UsdMayaShadowMap
{
public:
    UsdMayaShadowMap() = default;
    UsdMayaShadowMap(GLuint texture, const MHWRender::MSamplerStateDesc&);

    void BindResources(GlfBindingMapPtr const&) const;
    void UnbindResources(GlfBindingMapPtr const&) const;

    bool IsValid() const
    {
        return _textureHandle != INVALID_GLUINT
            && _sampler.Get() != INVALID_GLUINT;
    }

    GfMatrix4d _viewMatrix, _projectionMatrix;
    TfToken	_samplerToken;

private:
    static constexpr auto
        INVALID_GLUINT = std::numeric_limits<GLuint>::max();

    class _UniqueSampler
    {
    public:
        explicit _UniqueSampler(GLuint handle = INVALID_GLUINT)
            : _handle(handle)
        {}

        _UniqueSampler(const _UniqueSampler&) = delete;
        _UniqueSampler(_UniqueSampler&&);

        ~_UniqueSampler();

        _UniqueSampler& operator=(const _UniqueSampler&) = delete;
        _UniqueSampler& operator=(_UniqueSampler&&);

        GLuint Get() const { return _handle; }

    private:
        GLuint _handle;
    };

    GLuint _textureHandle = INVALID_GLUINT;
    _UniqueSampler _sampler;
};

TF_DECLARE_WEAK_AND_REF_PTRS(UsdMayaShadowSource);

class UsdMayaShadowSource : public GlfShadowSource
{
    ~UsdMayaShadowSource() override;
public:
    UsdMayaShadowSource();

    void InitResourceBindings(GlfBindingMapPtr const&) override;

    void BindResources(GlfBindingMapPtr const&) override;
    void UnbindResources(GlfBindingMapPtr const&) override;

    void DefineShaderMacros(std::stringstream& defineStream) const override;
    size_t CombineShaderHash(size_t hash) const override;
    
    void SetViewMatrix(size_t index, GfMatrix4d const& matrix) override;
    void SetProjectionMatrix(size_t index, GfMatrix4d const& matrix) override;

    GfMatrix4d GetWorldToShadowMatrix(size_t index) const override;

    // End of GlfShadowSource overrides

    // Returns the new shadow map index
    size_t AddShadowMap(GLuint texture, const MHWRender::MSamplerStateDesc&);

private:
    std::vector<UsdMayaShadowMap> _shadowMaps;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
