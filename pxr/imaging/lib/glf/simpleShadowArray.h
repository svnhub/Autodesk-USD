//
// Copyright 2016 Pixar
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
#ifndef GLF_SIMPLE_SHADOW_ARRAY_H
#define GLF_SIMPLE_SHADOW_ARRAY_H

/// \file glf/simpleShadowArray.h

#include "pxr/pxr.h"
#include "pxr/imaging/glf/api.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/imaging/garch/gl.h"
#include "pxr/imaging/glf/shadowSource.h"

PXR_NAMESPACE_OPEN_SCOPE

class GlfSimpleShadowArray : public GlfShadowSource
{
public:
    GLF_API
    GlfSimpleShadowArray(GfVec2i const & size);
    GLF_API
    ~GlfSimpleShadowArray() override;

    GLF_API
    void InitResourceBindings(GlfBindingMapPtr const &bindingMap) override;

    GLF_API
    void BindResources(GlfBindingMapPtr const &bindingMap) override;
    GLF_API
    void UnbindResources(GlfBindingMapPtr const &bindingMap) override;
    
    GLF_API
    void SetViewMatrix(size_t index, GfMatrix4d const & matrix) override;
    GLF_API
    void SetProjectionMatrix(size_t index, GfMatrix4d const & matrix) override;

    GLF_API
    GfMatrix4d GetWorldToShadowMatrix(size_t index) const override;

    // End of GlfShadowSource overrides

    GLF_API
    GfVec2i GetSize() const;
    GLF_API
    void SetSize(GfVec2i const & size);

    GLF_API
    size_t GetNumLayers() const;
    GLF_API
    void SetNumLayers(size_t numLayers);

    GLF_API
    GfMatrix4d GetViewMatrix(size_t index) const;
    GLF_API
    GfMatrix4d GetProjectionMatrix(size_t index) const;

    GLF_API
    void BeginCapture(size_t index, bool clear);
    GLF_API
    void EndCapture(size_t index);

private:
    void _AllocTextureArray();
    void _FreeTextureArray();

    void _BindFramebuffer(size_t index);
    void _UnbindFramebuffer();

private:
    GfVec2i _size;
    size_t _numLayers;

    std::vector<GfMatrix4d> _viewMatrices;
    std::vector<GfMatrix4d> _projectionMatrices;

    GLuint _texture;
    GLuint _framebuffer;

    GLuint _shadowDepthSampler;
    GLuint _shadowCompareSampler;

    GLuint _unbindRestoreDrawFramebuffer;
    GLuint _unbindRestoreReadFramebuffer;

    GLint  _unbindRestoreViewport[4];
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
