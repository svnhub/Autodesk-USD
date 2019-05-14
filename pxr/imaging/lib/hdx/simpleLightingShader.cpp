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
#include "pxr/imaging/hdx/simpleLightingShader.h"
#include "pxr/imaging/hdx/package.h"

#include "pxr/imaging/hd/binding.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hf/perfLog.h"

#include "pxr/imaging/glf/glslfx.h"
#include "pxr/imaging/glf/bindingMap.h"
#include "pxr/imaging/glf/simpleLightingContext.h"

#include <boost/functional/hash.hpp>

#include <string>
#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE


HdxSimpleLightingShader::HdxSimpleLightingShader(
    TfToken glslfxOverridePath /*= TfToken()*/
)
    : _lightingContext(GlfSimpleLightingContext::New())
    , _useLighting(true)
    , _glslfxPath(glslfxOverridePath.IsEmpty()
        ? HdxPackageSimpleLightingShader()
        : glslfxOverridePath
    )
{
    _lightingContext->InitResourceBindings();
    _glslfx.reset(new GlfGLSLFX(_glslfxPath));
}

HdxSimpleLightingShader::~HdxSimpleLightingShader()
{
}

/* virtual */
HdxSimpleLightingShader::ID
HdxSimpleLightingShader::ComputeHash() const
{
    HD_TRACE_FUNCTION();

    size_t numLights = _useLighting ? _lightingContext->GetNumLightsUsed() : 0;
    bool useShadows = _useLighting ? _lightingContext->GetUseShadows() : false;

    size_t hash = _glslfxPath.Hash();
    boost::hash_combine(hash, numLights);
    boost::hash_combine(hash, useShadows);

    if (const GlfShadowSourceRefPtr shadows = _lightingContext->GetShadows())
    {
        hash = shadows->CombineShaderHash(hash);
    }

    return (ID)hash;
}

/* virtual */
std::string
HdxSimpleLightingShader::GetSource(TfToken const &shaderStageKey) const
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    std::string source = _glslfx->GetSource(shaderStageKey);

    if (source.empty()) return source;

    std::stringstream defineStream;
    size_t numLights = _useLighting ? _lightingContext->GetNumLightsUsed() : 0;
    bool useShadows = _useLighting ? _lightingContext->GetUseShadows() : false;
    defineStream << "#define NUM_LIGHTS " << numLights<< "\n";
    defineStream << "#define USE_SHADOWS " << (int)(useShadows) << "\n";

	if (const GlfShadowSourceRefPtr shadows = _lightingContext->GetShadows())
	{
		shadows->DefineShaderMacros(defineStream);
	}

    return defineStream.str() + source;
}

/* virtual */
void
HdxSimpleLightingShader::SetCamera(GfMatrix4d const &worldToViewMatrix,
                                          GfMatrix4d const &projectionMatrix)
{
    _lightingContext->SetCamera(worldToViewMatrix, projectionMatrix);
}

/* virtual */
void
HdxSimpleLightingShader::BindResources(HdSt_ResourceBinder const& /*binder*/,
                                      int program)
{    
    _lightingContext->BindResouces(static_cast<GLuint>(program));
}

/* virtual */
void
HdxSimpleLightingShader::UnbindResources(HdSt_ResourceBinder const &/*binder*/,
                                        int /*program*/)
{
    _lightingContext->UnbindResources();
}

/*virtual*/
void
HdxSimpleLightingShader::AddBindings(HdBindingRequestVector *customBindings)
{
}

void
HdxSimpleLightingShader::SetLightingStateFromOpenGL()
{
    _lightingContext->SetStateFromOpenGL();
}

void
HdxSimpleLightingShader::SetLightingState(
    GlfSimpleLightingContextPtr const &src)
{
    if (src) {
        _useLighting = true;
        _lightingContext->SetUseLighting(!src->GetLights().empty());
        _lightingContext->SetLights(src->GetLights());
        _lightingContext->SetMaterial(src->GetMaterial());
        _lightingContext->SetSceneAmbient(src->GetSceneAmbient());
        _lightingContext->SetShadows(src->GetShadows());
    } else {
        // XXX:
        // if src is null, turn off lights (this is temporary used for shadowmap drawing).
        // see GprimUsdBaseIcBatch::Draw()
        _useLighting = false;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE

