# Using HgiWebGPU

Due to some limitations for WebGPU, some features need to be disabled
and others will limit the kind of models that will be correctly rendered.
For example, there are a few environment variables to set.
```
export HDST_ENABLE_PIPELINE_DRAW_BATCH_GPU_FRUSTUM_CULLING=0
export HD_ENABLE_PACKED_NORMALS=0
export HDX_ENABLE_OIT=0
export HGI_ENABLE_WEBGPU=1
```
