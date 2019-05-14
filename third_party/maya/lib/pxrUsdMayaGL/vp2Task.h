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
#ifndef USD_MAYA_VP2_TASK_H
#define USD_MAYA_VP2_TASK_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdx/api.h"
#include "pxr/imaging/hdx/version.h"
#include "pxr/imaging/hd/enums.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/task.h"

#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/tf/declarePtrs.h"

#include "pxr/imaging/hdSt/renderPass.h"
#include <boost/shared_ptr.hpp>

PXR_NAMESPACE_OPEN_SCOPE

class HdSceneDelegate;
class HdStRenderPassState;

typedef boost::shared_ptr<class HdStShaderCode> HdStShaderCodeSharedPtr;
typedef boost::shared_ptr<class HdRenderPassState> HdRenderPassStateSharedPtr;

struct UsdMayaGL_Vp2TaskParams
{
    UsdMayaGL_Vp2TaskParams()
        : overrideColor(0.0)
        , wireframeColor(0.0)
        , enableLighting(false)
        , enableIdRender(false)
        , enableSceneMaterials(true)
        , disableSrgb(false)
        , alphaThreshold(0.0)
        , depthBiasEnable(false)
        , depthBiasConstantFactor(0.0f)
        , depthBiasSlopeFactor(1.0f)
        , depthFunc(HdCmpFuncLEqual)
        , cullStyle(HdCullStyleBackUnlessDoubleSided)
        , camera()
        , viewport(0.0)
        , lightIncludePaths(1, SdfPath::AbsoluteRootPath())
        , lightExcludePaths()
        {}

    // RenderPassState
    GfVec4f overrideColor;
    GfVec4f wireframeColor;
    bool enableLighting;
    bool enableIdRender;
    bool enableSceneMaterials;
    bool disableSrgb;
    float alphaThreshold;
    bool  depthBiasEnable;
    float depthBiasConstantFactor;
    float depthBiasSlopeFactor;
    HdCompareFunction depthFunc;
    HdCullStyle cullStyle;

    // RenderPassState index objects
    SdfPath camera;
    GfVec4d viewport;

    // Lights/Shadows specific parameters
    SdfPathVector lightIncludePaths;
    SdfPathVector lightExcludePaths;
};

/// \class UsdMayaVp2Task
///
/// A task for generating shadow maps.
///
class UsdMayaGL_Vp2Task : public HdTask
{
public:
    UsdMayaGL_Vp2Task(
        HdSceneDelegate* delegate, SdfPath const& id, TfToken glslfxPath
    );

    ~UsdMayaGL_Vp2Task() override;

    /// Sync the render pass resources
    void Sync(HdSceneDelegate* delegate,
              HdTaskContext* ctx,
              HdDirtyBits* dirtyBits) override;

    /// Execute render pass task
    void Execute(HdTaskContext* ctx) override;

private:
    void _SetHdStRenderPassState(UsdMayaGL_Vp2TaskParams const &params,
        HdStRenderPassState *renderPassState);

    void _UpdateDirtyParams(HdRenderPassStateSharedPtr &renderPassState, 
        UsdMayaGL_Vp2TaskParams const &params);

    static HdStShaderCodeSharedPtr _overrideShader;
    static void _CreateOverrideShader();

    HdSt_RenderPass _renderPass;
    HdRenderPassStateSharedPtr _renderPassState;
    UsdMayaGL_Vp2TaskParams _params;

    UsdMayaGL_Vp2Task() = delete;
    UsdMayaGL_Vp2Task(const UsdMayaGL_Vp2Task &) = delete;
    UsdMayaGL_Vp2Task &operator =(const UsdMayaGL_Vp2Task &) = delete;
};

// VtValue requirements
std::ostream& operator<<(std::ostream& out, const UsdMayaGL_Vp2TaskParams& pv);
bool operator==(const UsdMayaGL_Vp2TaskParams& lhs, const UsdMayaGL_Vp2TaskParams& rhs);
bool operator!=(const UsdMayaGL_Vp2TaskParams& lhs, const UsdMayaGL_Vp2TaskParams& rhs);


PXR_NAMESPACE_CLOSE_SCOPE

#endif //USD_MAYA_VP2_TASK_H
