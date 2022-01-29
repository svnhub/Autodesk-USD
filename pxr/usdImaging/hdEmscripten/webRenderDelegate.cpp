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
#include "webRenderDelegate.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/imaging/hd/bufferArray.h"
#include "pxr/imaging/hd/coordSys.h"
#include "pxr/imaging/hd/material.h"
#include "pxr/imaging/hd/mesh.h"
#include "pxr/imaging/hd/basisCurves.h"
#include "pxr/imaging/hd/points.h"
#include "pxr/imaging/hd/bprim.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/repr.h"
#include "pxr/imaging/hd/resourceRegistry.h"
#include "pxr/imaging/hd/strategyBase.h"
#include "pxr/imaging/hd/unitTestNullRenderPass.h"
#include "pxr/imaging/hd/meshUtil.h"
#include "pxr/imaging/hd/smoothNormals.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE
using namespace emscripten;

////////////////////////////////////////////////////////////////
// Null Prims

const std::map<HdInterpolation, std::string> InterpolationStrings = {
    {HdInterpolationConstant, "constant"},
    {HdInterpolationUniform, "uniform"},
    {HdInterpolationVarying, "varying"},
    {HdInterpolationVertex, "vertex"},
    {HdInterpolationFaceVarying, "facevarying"},
    {HdInterpolationInstance, "instance"}
};

void _runInMainThread(int funPointer) {
    std::function<void()>  *function = reinterpret_cast<std::function<void()>*>(funPointer);
    (*function)();
}

void runInMainThread(std::function<void()> fun) {
    //emscripten_sync_run_in_main_thread_1((int) &_runInMainThread, &fun);
    em_queued_call q = {EM_FUNC_SIG_VI, (void*) &_runInMainThread};
    q.args[0].vp = (void *) &fun;
    q.returnValue.vp = 0;
    emscripten_sync_run_in_main_thread(&q);
}

class Emscripten_Rprim final : public HdMesh {
public:
    Emscripten_Rprim(TfToken const& typeId,
                 SdfPath const& id,
                 emscripten::val renderDelegateInterface)
     : HdMesh(id)
     , _typeId(typeId)
     , _renderDelegateInterface(renderDelegateInterface)
     , _rPrim(val::undefined())
     , _normalsValid(false)
     , _adjacencyValid(false)
     , _meshUtil(NULL)
    {
      _rPrim = _renderDelegateInterface.call<val>("createRPrim", std::string(typeId.GetText()), id.GetAsString());
    }

    virtual ~Emscripten_Rprim() = default;

    TfTokenVector const & GetBuiltinPrimvarNames() const override {
        static const TfTokenVector primvarNames;
        return primvarNames;
    }

    virtual void Sync(HdSceneDelegate *delegate,
                      HdRenderParam   *renderParam,
                      HdDirtyBits     *dirtyBits,
                      TfToken const   &reprToken) override
    {
        // Get the id of this mesh. This is used to get various resources associated with it.
        SdfPath const& id = GetId();

        // TODO: Debug
        //std::cout << "Syncing mesh " << id.GetAsString() << "..." << std::endl;

        // A mesh can have up to 2 representation descriptions (e.g. different draw style for front and back faces).
        // We only consider the first one. This could be improved in the future.
        _MeshReprConfig::DescArray descs = _GetReprDesc(reprToken);
        const HdMeshReprDesc &desc = descs[0];

        // Get all the dirty computed primvars
        // TODO: Debug
        // std::cout << "  Evaluating dirty computed primvars..." << std::endl;
        // HdExtComputationPrimvarDescriptorVector dirtyCompPrimvars;
        // for (size_t i=0; i < HdInterpolationCount; ++i) {
        //     HdExtComputationPrimvarDescriptorVector compPrimvars;
        //     HdInterpolation interp = static_cast<HdInterpolation>(i);
        //     compPrimvars = delegate->GetExtComputationPrimvarDescriptors
        //                                 (GetId(),interp);

        //     for (auto const& pv: compPrimvars) {
        //         if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pv.name)) {
        //             dirtyCompPrimvars.emplace_back(pv);
        //             // TODO: Debug
        //             std::cout << "    " << pv.name.GetText() << " is dirty." << std::endl;
        //         }
        //     }
        // }

        // PrimId dirty bit is internal to Hydra.

        // if (HdChangeTracker::IsExtentDirty(*dirtyBits, id)) {
        //     GetExtent(delegate);
        // }

        // if (HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id)) {
        //     delegate->GetDisplayStyle(id);
        // }

        // Materials need to be synced before primvars, to allow the JS side to apply primvar information like
        // displayColor if no other material is set.
        if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
            auto materialId = delegate->GetMaterialId(id);
            //std::cout << "Setting material '" << materialId.GetAsString() << "' for mesh '" << id << "'." << std::endl;
            runInMainThread([&]() {
                _rPrim.call<void>("setMaterial", materialId.GetAsString());
            });
        }

        // Update points
        if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)) {
            VtValue value = delegate->Get(id, HdTokens->points);
            _points = value.Get<VtVec3fArray>();
            _normalsValid = false;
            runInMainThread([&]() {
                _rPrim.call<void>("updatePoints", val(typed_memory_view(3 * _points.size(), reinterpret_cast<float*>(_points.data()))));
            });
        }

        // The repr defines whether we should compute smooth normals for this mesh:
        // per-vertex normals taken as an average of adjacent faces, and
        // interpolated smoothly across faces.
        /*_smoothNormals = !desc.flatShadingEnabled;

        // If the subdivision scheme is "none" or "bilinear", force us not to use
        // smooth normals.
        _smoothNormals = _smoothNormals &&
            (_topology.GetScheme() != PxOsdOpenSubdivTokens->none) &&
            (_topology.GetScheme() != PxOsdOpenSubdivTokens->bilinear);*/
        _smoothNormals = true;

        // If the scene delegate has provided authored normals, force us to not use
        // smooth normals.
        bool authoredNormals = false;
        /*if (_primvarSourceMap.count(HdTokens->normals) > 0) {
            authoredNormals = true;
        }*/
        _smoothNormals = _smoothNormals && !authoredNormals;

        if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
            // The topology getter depends on prim type
            if (_typeId == HdPrimTypeTokens->mesh) {
                // When pulling a new topology, we don't want to overwrite the
                // refine level or subdiv tags, which are provided separately by the
                // scene delegate, so we save and restore them.
                PxOsdSubdivTags subdivTags = _topology.GetSubdivTags();
                int refineLevel = _topology.GetRefineLevel();
                _topology = HdMeshTopology(delegate->GetMeshTopology(id), refineLevel);
                _topology.SetSubdivTags(subdivTags);

                // Triangulate the input faces.
                if (_meshUtil != NULL) delete _meshUtil;
                _meshUtil = new HdMeshUtil(&_topology, GetId());
                _meshUtil->ComputeTriangleIndices(&_triangulatedIndices, &_trianglePrimitiveParams);

                runInMainThread([&]() {
                    _rPrim.call<void>("updateIndices", val(typed_memory_view(3 * _triangulatedIndices.size(), reinterpret_cast<int32_t*>(_triangulatedIndices.data()))));
                });

                _normalsValid = false;
                _adjacencyValid = false;

            } else if (_typeId == HdPrimTypeTokens->basisCurves) {
                delegate->GetBasisCurvesTopology(id);
            }
            // Other prim types don't have a topology
        }

        // Points is a primvar
        if (HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id)) {
            _SyncPrimvars(delegate, *dirtyBits);
        }

        // Update the smooth normals in steps:
        // 1. If the topology is dirty, update the adjacency table, a processed
        //    form of the topology that helps calculate smooth normals quickly.
        // 2. If the points are dirty, update the smooth normal buffer itself.
        if (_smoothNormals && !_adjacencyValid) {
            _adjacency.BuildAdjacencyTable(&_topology);
            _adjacencyValid = true;
            // If we rebuilt the adjacency table, force a rebuild of normals.
            _normalsValid = false;
        }

        if (_smoothNormals && !_normalsValid) {
            _computedNormals = Hd_SmoothNormals::ComputeSmoothNormals(
                &_adjacency, _points.size(), _points.cdata());
            _normalsValid = true;

            // Create a sampler for the "normals" primvar. If there are authored
            // normals, the smooth normals flag has been suppressed, so it won't
            // be overwritten by the primvar population below.
            //_CreatePrimvarSampler(HdTokens->normals, VtValue(_computedNormals),
            //    HdInterpolationVertex, false);
            runInMainThread([&]() {
                _rPrim.call<void>("updateNormals", val(typed_memory_view(3 * _computedNormals.size(), reinterpret_cast<float*>(_computedNormals.data()))));
            });
        }

        // If smooth normals are off and there are no authored normals, make sure
        // there's no "normals" sampler so the renderpass can use its fallback
        // behavior.
        if (!_smoothNormals && !authoredNormals) {
            // Force the smooth normals code to rebuild the "normals" primvar the
            // next time smooth normals is enabled.
            _normalsValid = false;
        }

        if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
            _transform = GfMatrix4f(delegate->GetTransform(id));
            runInMainThread([&]() {
                _rPrim.call<void>("setTransform", val(typed_memory_view(16, reinterpret_cast<float*>(_transform.data()))));
            });
        }

        if (HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
            delegate->GetVisible(id);
        }

        // Normals is a primvar

        if (HdChangeTracker::IsDoubleSidedDirty(*dirtyBits, id)) {
            delegate->GetDoubleSided(id);
        }

        if (HdChangeTracker::IsCullStyleDirty(*dirtyBits, id)) {
            delegate->GetCullStyle(id);
        }

        if (HdChangeTracker::IsInstancerDirty(*dirtyBits, id)) {
            // Instancer Dirty doesn't have a corresponding scene delegate pull
        }

        // InstanceIndex applies to Instancer's not Rprim

        if (HdChangeTracker::IsReprDirty(*dirtyBits, id)) {
            delegate->GetReprSelector(id);
        }

        // RenderTag doesn't have a change tracker test
        if (*dirtyBits & HdChangeTracker::DirtyRenderTag) {
            delegate->GetRenderTag(id);
        }

        *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;
    }


    virtual HdDirtyBits GetInitialDirtyBitsMask() const override
    {
        // Set all bits except the varying flag
        return  (HdChangeTracker::AllSceneDirtyBits) &
               (~HdChangeTracker::Varying);
    }

    virtual HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override
    {
        return bits;
    }


protected:
    virtual void _InitRepr(TfToken const &reprToken,
                           HdDirtyBits *dirtyBits) override
    {
        _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
                                                _ReprComparator(reprToken));
        if (it == _reprs.end()) {
            _reprs.emplace_back(reprToken, HdReprSharedPtr());
        }
    }

private:
    TfToken _typeId;
    emscripten::val _renderDelegateInterface;
    emscripten::val _rPrim;
    HdMeshUtil *_meshUtil;

    VtVec3iArray _triangulatedIndices;
    VtIntArray _trianglePrimitiveParams;
    VtVec3fArray _computedNormals;

    HdMeshTopology _topology;
    GfMatrix4f _transform;
    VtVec3fArray _points;
    Hd_VertexAdjacency _adjacency;

    bool _adjacencyValid;
    bool _normalsValid;
    bool _smoothNormals;

    void _SendPrimvar(const VtValue &value, const std::string &name, const HdInterpolation &interpolation)
    {
        const std::string &ip = InterpolationStrings.at(interpolation);
        if (value.CanCast<VtVec2fArray>()) {
            VtVec2fArray primvarData = value.Get<VtVec2fArray>();
            _rPrim.call<void>("updatePrimvar", name, val(typed_memory_view(2 * primvarData.size(), reinterpret_cast<float*>(primvarData.data()))), 2, ip);
        }
        if (value.CanCast<VtVec3fArray>()) {
            VtVec3fArray primvarData = value.Get<VtVec3fArray>();
            _rPrim.call<void>("updatePrimvar", name, val(typed_memory_view(3 * primvarData.size(), reinterpret_cast<float*>(primvarData.data()))), 3, ip);
        }
        if (value.CanCast<VtVec4fArray>()) {
            VtVec4fArray primvarData = value.Get<VtVec4fArray>();
            _rPrim.call<void>("updatePrimvar", name, val(typed_memory_view(4 * primvarData.size(), reinterpret_cast<float*>(primvarData.data()))), 4, ip);
        }
    }

    void _SyncPrimvars(HdSceneDelegate *delegate,
                       HdDirtyBits      dirtyBits)
    {
        runInMainThread([&]() {
            SdfPath const &id = GetId();
            for (size_t interpolation = HdInterpolationConstant;
                        interpolation < HdInterpolationCount;
                        ++interpolation) {
                HdInterpolation ip = static_cast<HdInterpolation>(interpolation);
                HdPrimvarDescriptorVector primvars = GetPrimvarDescriptors(delegate, ip);

                size_t numPrimVars = primvars.size();
                for (size_t primVarNum = 0;
                            primVarNum < numPrimVars;
                        ++primVarNum) {
                    HdPrimvarDescriptor const &primvar = primvars[primVarNum];

                    if (HdChangeTracker::IsPrimvarDirty(dirtyBits,
                                                        id,
                                                        primvar.name)) {
                        VtValue value = GetPrimvar(delegate, primvar.name);

                        switch(ip) {
                            case HdInterpolationFaceVarying: {
                                HdVtBufferSource buffer(primvar.name, value);

                                VtValue triangulated;
                                if (!_meshUtil->ComputeTriangulatedFaceVaryingPrimvar(
                                        buffer.GetData(),
                                        buffer.GetNumElements(),
                                        buffer.GetTupleType().type,
                                        &triangulated)) {
                                    TF_CODING_ERROR("[%s] Could not triangulate face-varying data.",
                                        primvar.name.GetText());
                                    continue;
                                }

                                _SendPrimvar(triangulated, primvar.name.GetString(), ip);
                                break;
                            }
                            case HdInterpolationConstant:
                            case HdInterpolationVertex: {
                                _SendPrimvar(value, primvar.name.GetText(), ip);
                                break;
                            }
                            default:
                                TF_WARN("Unsupported interpolation type '%s' for primvar %s",
                                    InterpolationStrings.at(ip).c_str(),
                                    primvar.name.GetText());
                        }
                    }
                }
            }
        });
    }

    Emscripten_Rprim()                                 = delete;
    Emscripten_Rprim(const Emscripten_Rprim &)             = delete;
    Emscripten_Rprim &operator =(const Emscripten_Rprim &) = delete;
};

class Emscripten_Material final : public HdMaterial {
public:
    Emscripten_Material(SdfPath const& id, emscripten::val renderDelegateInterface) :
      HdMaterial(id)
     , _renderDelegateInterface(renderDelegateInterface)
     , _sPrim(val::undefined())
    {
      _sPrim = _renderDelegateInterface.call<val>("createSPrim", std::string("material"), id.GetAsString());
    }

    virtual ~Emscripten_Material() = default;

    virtual void Sync(HdSceneDelegate *sceneDelegate,
                      HdRenderParam   *renderParam,
                      HdDirtyBits     *dirtyBits) override
    {
      if (*dirtyBits == HdMaterial::Clean) {
        return;
      }
      runInMainThread([&]() {

        VtValue vtMat = sceneDelegate->GetMaterialResource(GetId());
        if (vtMat.IsHolding<HdMaterialNetworkMap>()) {
            HdMaterialNetworkMap const& hdNetworkMap =
                vtMat.UncheckedGet<HdMaterialNetworkMap>();

            for (auto& [networkId, network]: hdNetworkMap.map) {
            for (auto& node :  network.nodes) {
                val parameters = val::object();
                for (auto &[parameterName, value] : node.parameters) {
                parameters.set(parameterName, value._GetJsVal());
                }

                _sPrim.call<val>("updateNode", networkId.GetString(), node.path.GetAsString(), parameters);
            }

            val relationships = val::array();
            int i = 0;
            for (auto &relationship : network.relationships) {
                val relationshipObj = val::object();
                relationshipObj.set("inputId", relationship.inputId.GetAsString());
                relationshipObj.set("inputName", relationship.inputName);
                relationshipObj.set("outputId", relationship.outputId.GetAsString());
                relationshipObj.set("outputName", relationship.outputName);
                relationships.set(i++, relationshipObj);
            }

            _sPrim.call<val>("updateFinished", networkId.GetString(), relationships);
            }
        }
        *dirtyBits = HdMaterial::Clean;
      });
    };

    virtual HdDirtyBits GetInitialDirtyBitsMask() const override {
        return HdMaterial::AllDirty;
    }

private:
    emscripten::val _renderDelegateInterface;
    emscripten::val _sPrim;


    Emscripten_Material()                                  = delete;
    Emscripten_Material(const Emscripten_Material &)             = delete;
    Emscripten_Material &operator =(const Emscripten_Material &) = delete;
};

class Emscripten_CoordSys final : public HdCoordSys {
public:
    Emscripten_CoordSys(SdfPath const& id) : HdCoordSys(id) {}
    virtual ~Emscripten_CoordSys() = default;

    virtual void Sync(HdSceneDelegate *sceneDelegate,
                      HdRenderParam   *renderParam,
                      HdDirtyBits     *dirtyBits) override
    {
        *dirtyBits = HdCoordSys::Clean;
    };

    virtual HdDirtyBits GetInitialDirtyBitsMask() const override {
        return HdCoordSys::AllDirty;
    }

private:
    Emscripten_CoordSys()                                  = delete;
    Emscripten_CoordSys(const Emscripten_CoordSys &)             = delete;
    Emscripten_CoordSys &operator =(const Emscripten_CoordSys &) = delete;
};

const TfTokenVector WebRenderDelegate::SUPPORTED_RPRIM_TYPES =
{
    HdPrimTypeTokens->mesh,
    HdPrimTypeTokens->basisCurves,
    HdPrimTypeTokens->points
};

const TfTokenVector WebRenderDelegate::SUPPORTED_SPRIM_TYPES =
{
    HdPrimTypeTokens->coordSys,
    HdPrimTypeTokens->material
};

const TfTokenVector WebRenderDelegate::SUPPORTED_BPRIM_TYPES =
{
};

const TfTokenVector &
WebRenderDelegate::GetSupportedRprimTypes() const
{
    return SUPPORTED_RPRIM_TYPES;
}

const TfTokenVector &
WebRenderDelegate::GetSupportedSprimTypes() const
{
    return SUPPORTED_SPRIM_TYPES;
}

const TfTokenVector &
WebRenderDelegate::GetSupportedBprimTypes() const
{
    return SUPPORTED_BPRIM_TYPES;
}

HdRenderParam *
WebRenderDelegate::GetRenderParam() const
{
    return nullptr;
}

HdResourceRegistrySharedPtr
WebRenderDelegate::GetResourceRegistry() const
{
    static HdResourceRegistrySharedPtr resourceRegistry(new HdResourceRegistry);
    return resourceRegistry;
}

HdRenderPassSharedPtr
WebRenderDelegate::CreateRenderPass(HdRenderIndex *index,
                                HdRprimCollection const& collection)
{
    return HdRenderPassSharedPtr(
        new Hd_UnitTestNullRenderPass(index, collection));
}

HdInstancer *
WebRenderDelegate::CreateInstancer(HdSceneDelegate *delegate,
                                               SdfPath const& id)
{
    return new HdInstancer(delegate, id);
}

void
WebRenderDelegate::DestroyInstancer(HdInstancer *instancer)
{
    delete instancer;
}


HdRprim *
WebRenderDelegate::CreateRprim(TfToken const& typeId,
                                    SdfPath const& rprimId)
{
    return new Emscripten_Rprim(typeId, rprimId, _renderDelegateInterface);
}

void
WebRenderDelegate::DestroyRprim(HdRprim *rPrim)
{
    delete rPrim;
}

HdSprim *
WebRenderDelegate::CreateSprim(TfToken const& typeId,
                                           SdfPath const& sprimId)
{
    if (typeId == HdPrimTypeTokens->material) {
        return new Emscripten_Material(sprimId, _renderDelegateInterface);
    } else if (typeId == HdPrimTypeTokens->coordSys) {
        return new Emscripten_CoordSys(sprimId);
    } else {
        TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    }

    return nullptr;
}

HdSprim *
WebRenderDelegate::CreateFallbackSprim(TfToken const& typeId)
{
    if (typeId == HdPrimTypeTokens->material) {
        return new Emscripten_Material(SdfPath::EmptyPath(), _renderDelegateInterface);
    } else if (typeId == HdPrimTypeTokens->coordSys) {
        return new Emscripten_CoordSys(SdfPath::EmptyPath());
    } else {
        TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    }

    return nullptr;
}


void
WebRenderDelegate::DestroySprim(HdSprim *sPrim)
{
    delete sPrim;
}

HdBprim *
WebRenderDelegate::CreateBprim(TfToken const& typeId,
                                    SdfPath const& bprimId)
{
    TF_CODING_ERROR("Unknown Bprim Type %s", typeId.GetText());

    return nullptr;
}

HdBprim *
WebRenderDelegate::CreateFallbackBprim(TfToken const& typeId)
{
    TF_CODING_ERROR("Unknown Bprim Type %s", typeId.GetText());

    return nullptr;
}

void
WebRenderDelegate::DestroyBprim(HdBprim *bPrim)
{
    delete bPrim;
}

void
WebRenderDelegate::CommitResources(HdChangeTracker *tracker)
{
    _renderDelegateInterface.call<void>("CommitResources");    
}

PXR_NAMESPACE_CLOSE_SCOPE
