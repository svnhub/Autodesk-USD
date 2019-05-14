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
#include "pxr/imaging/glf/glew.h"

#include "pxrUsdMayaGL/vp2Task.h"
#include "pxrUsdMayaGL/package.h"

#include "pxr/imaging/hdx/simpleLightTask.h"
#include "pxr/imaging/hdx/tokens.h"
#include "pxr/imaging/hdx/package.h"

#include "pxr/imaging/hd/changeTracker.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/primGather.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/imaging/hd/resourceRegistry.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/camera.h"

#include "pxr/imaging/cameraUtil/conformWindow.h"

#include "pxr/imaging/hdSt/glConversions.h"
#include "pxr/imaging/hdSt/glslfxShader.h"
#include "pxr/imaging/hdSt/light.h"
#include "pxr/imaging/hdSt/lightingShader.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/renderPassShader.h"
#include "pxr/imaging/hdSt/renderPassState.h"

#include "pxr/imaging/glf/simpleLightingContext.h"
#include "pxr/imaging/glf/diagnostic.h"

PXR_NAMESPACE_OPEN_SCOPE

HdStShaderCodeSharedPtr UsdMayaGL_Vp2Task::_overrideShader;

TfToken UsdMayaGL_Vp2Task::GetToken()
{
    static const TfToken token("vp2ShadowTask");
    return token;
}

UsdMayaGL_Vp2Task::UsdMayaGL_Vp2Task(HdSceneDelegate* delegate, SdfPath const& id)
    : HdTask(id)
    , _renderPass(
        &delegate->GetRenderIndex(),
        HdRprimCollection(
            HdTokens->geometry,
            HdReprSelector(HdReprTokens->refined)
        )
    )
    , _renderPassState(boost::make_shared<HdStRenderPassState>(
        boost::make_shared<HdStRenderPassShader>(UsdMayaGLVp2ShadowShader())
    ))
{
}

UsdMayaGL_Vp2Task::~UsdMayaGL_Vp2Task()
{
}

void
UsdMayaGL_Vp2Task::Sync(HdSceneDelegate* delegate,
                    HdTaskContext* ctx,
                    HdDirtyBits* dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    GLF_GROUP_FUNCTION();

    HdRenderIndex &renderIndex = delegate->GetRenderIndex();

    const bool dirtyParams = (*dirtyBits) & HdChangeTracker::DirtyParams;
    if (dirtyParams) {
        // Extract the new shadow task params from exec
        if (!_GetTaskParams(delegate, &_params)) {
            return;
        }
    }
    
    // Ensure all passes have the right params set.
    if (dirtyParams)
        _UpdateDirtyParams(_renderPassState, _params);

    // This state is invariant of parameter changes so set it
    // once.
    _renderPassState->SetLightingEnabled(false);
    // XXX : This can be removed when Hydra has support for 
    //       transparent objects.
    //       We use an epsilon offset from 1.0 to allow for 
    //       calculation during primvar interpolation which
    //       doesn't fully saturate back to 1.0.
    const float TRANSPARENT_ALPHA_THRESHOLD = (1.0f - 1e-6f);
    _renderPassState->SetAlphaThreshold(TRANSPARENT_ALPHA_THRESHOLD);

    // But if it is not then we still have to make sure we don't
    // buffer overrun here.
    
    // Move the camera to the correct position to take the shadow map
    const auto camera = static_cast<const HdCamera*>(
        renderIndex.GetSprim(HdPrimTypeTokens->camera, _params.camera));

    if (camera) {
        VtValue modelViewVt = camera->Get(HdCameraTokens->worldToViewMatrix);
        VtValue projectionVt = camera->Get(HdCameraTokens->projectionMatrix);
        GfMatrix4d modelView = modelViewVt.Get<GfMatrix4d>();
        GfMatrix4d projection = projectionVt.Get<GfMatrix4d>();

        const auto& viewport = _params.viewport;

        // If there is a window policy available in this camera
        // we will extract it and adjust the projection accordingly.
        VtValue windowPolicy = camera->Get(HdCameraTokens->windowPolicy);
        if (windowPolicy.IsHolding<CameraUtilConformWindowPolicy>()) {
            const CameraUtilConformWindowPolicy policy =
                windowPolicy.Get<CameraUtilConformWindowPolicy>();
            
            projection = CameraUtilConformedWindow(projection, policy,
                viewport[3] != 0.0 ? viewport[2] / viewport[3] : 1.0);
        }

        const VtValue &vClipPlanes = camera->Get(HdCameraTokens->clipPlanes);
        const HdRenderPassState::ClipPlanesVector &clipPlanes =
            vClipPlanes.Get<HdRenderPassState::ClipPlanesVector>();

        // sync render pass state
        _renderPassState->SetCamera(modelView, projection, viewport);
        _renderPassState->SetClipPlanes(clipPlanes);
    }

    _renderPassState->Sync(
        renderIndex.GetResourceRegistry());
    _renderPass.Sync();

    *dirtyBits = HdChangeTracker::Clean;
}

void
UsdMayaGL_Vp2Task::Execute(HdTaskContext* ctx)
{
    static const TfTokenVector SHADOW_RENDER_TAGS =
    {
        HdTokens->geometry,
        HdxRenderTagsTokens->interactiveOnlyGeom
    };

    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();
    GLF_GROUP_FUNCTION();

    if (_params.depthBiasEnable) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(_params.depthBiasSlopeFactor,
            _params.depthBiasConstantFactor);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // XXX: Move conversion to sync time once Task header becomes private.
    glDepthFunc(HdStGLConversions::GetGlDepthFunc(_params.depthFunc));
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Render the actual geometry in the collection
    _renderPass.Execute(
        _renderPassState,
        SHADOW_RENDER_TAGS);

    // restore GL states to default
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void
UsdMayaGL_Vp2Task::_CreateOverrideShader()
{
    static std::mutex shaderCreateLock;

    if (!_overrideShader) {
        std::lock_guard<std::mutex> lock(shaderCreateLock);
        if (!_overrideShader) {
            _overrideShader = HdStShaderCodeSharedPtr(new HdStGLSLFXShader(
                GlfGLSLFXSharedPtr(new GlfGLSLFX(
                    HdStPackageFallbackSurfaceShader()))));
        }
    }
}

void
UsdMayaGL_Vp2Task::_SetHdStRenderPassState(UsdMayaGL_Vp2TaskParams const &params,
    HdStRenderPassState *renderPassState)
{
    if (params.enableSceneMaterials) {
        renderPassState->SetOverrideShader(HdStShaderCodeSharedPtr());
    } else {
        if (!_overrideShader) {
            _CreateOverrideShader();
        }
        renderPassState->SetOverrideShader(_overrideShader);
    }
}

void
UsdMayaGL_Vp2Task::_UpdateDirtyParams(HdRenderPassStateSharedPtr &renderPassState,
    UsdMayaGL_Vp2TaskParams const &params)
{
    renderPassState->SetOverrideColor(params.overrideColor);
    renderPassState->SetWireframeColor(params.wireframeColor);
    renderPassState->SetCullStyle(HdInvertCullStyle(params.cullStyle));

    if (HdStRenderPassState* extendedState =
            dynamic_cast<HdStRenderPassState*>(renderPassState.get())) {
        _SetHdStRenderPassState(params, extendedState);
    }
}

// ---------------------------------------------------------------------------//
// VtValue Requirements
// ---------------------------------------------------------------------------//

std::ostream& operator<<(std::ostream& out, const UsdMayaGL_Vp2TaskParams& pv)
{
    out << "ShadowTask Params: (...) "
        << pv.overrideColor << " " 
        << pv.wireframeColor << " " 
        << pv.enableLighting << " "
        << pv.enableIdRender << " "
        << pv.enableSceneMaterials << " "
        << pv.alphaThreshold << " "
        << pv.depthBiasEnable << " "
        << pv.depthBiasConstantFactor << " "
        << pv.depthBiasSlopeFactor << " "
        << pv.depthFunc << " "
        << pv.cullStyle << " "
        << pv.camera << " "
        << pv.viewport << " "
        ;
        TF_FOR_ALL(it, pv.lightIncludePaths) {
            out << *it;
        }
        TF_FOR_ALL(it, pv.lightExcludePaths) {
            out << *it;
        }
    return out;
}

bool operator==(const UsdMayaGL_Vp2TaskParams& lhs, const UsdMayaGL_Vp2TaskParams& rhs)
{
    return  lhs.overrideColor == rhs.overrideColor                      && 
            lhs.wireframeColor == rhs.wireframeColor                    && 
            lhs.enableLighting == rhs.enableLighting                    &&
            lhs.enableIdRender == rhs.enableIdRender                    &&
            lhs.enableSceneMaterials == rhs.enableSceneMaterials        &&
            lhs.alphaThreshold == rhs.alphaThreshold                    &&
            lhs.depthBiasEnable == rhs.depthBiasEnable                  && 
            lhs.depthBiasConstantFactor == rhs.depthBiasConstantFactor  && 
            lhs.depthBiasSlopeFactor == rhs.depthBiasSlopeFactor        && 
            lhs.depthFunc == rhs.depthFunc                              && 
            lhs.cullStyle == rhs.cullStyle                              && 
            lhs.camera == rhs.camera                                    && 
            lhs.viewport == rhs.viewport                                && 
            lhs.lightIncludePaths == rhs.lightIncludePaths              && 
            lhs.lightExcludePaths == rhs.lightExcludePaths
            ;
}

bool operator!=(const UsdMayaGL_Vp2TaskParams& lhs, const UsdMayaGL_Vp2TaskParams& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE

