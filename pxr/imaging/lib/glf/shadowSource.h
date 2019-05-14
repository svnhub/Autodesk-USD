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
#ifndef GLF_SHADOW_SOURCE_H
#define GLF_SHADOW_SOURCE_H

/// \file glf/shadowSource.h

#include "pxr/pxr.h"
#include "pxr/imaging/glf/api.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/refPtr.h"
#include "pxr/base/tf/weakPtr.h"
#include "pxr/base/gf/matrix4d.h"

#include <boost/noncopyable.hpp>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_WEAK_AND_REF_PTRS(GlfBindingMap);

class GlfShadowSource
    : public TfRefBase
    , public TfWeakBase
    , boost::noncopyable
{
public:
    GLF_API
    virtual ~GlfShadowSource() {}

    GLF_API
    virtual void InitResourceBindings(GlfBindingMapPtr const &bindingMap) = 0;

    GLF_API
    virtual void BindResources(GlfBindingMapPtr const &bindingMap) = 0;
    GLF_API
    virtual void UnbindResources(GlfBindingMapPtr const &bindingMap) = 0;

    GLF_API
    virtual void DefineShaderMacros(std::stringstream& defineStream) const;
    GLF_API
    virtual size_t CombineShaderHash(size_t hash) const;

    GLF_API
    virtual void SetViewMatrix(size_t index, GfMatrix4d const & matrix) = 0;
    GLF_API
    virtual void SetProjectionMatrix(size_t index, GfMatrix4d const & matrix) = 0;

    GLF_API
    virtual GfMatrix4d GetWorldToShadowMatrix(size_t index) const = 0;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
