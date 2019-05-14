set(PXR_PACKAGE pxrUsdMayaGL)

if (NOT ${PXR_ENABLE_GL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_ENABLE_GL_SUPPORT is OFF")
    return()
endif()

pxr_shared_library(${PXR_PACKAGE}
    LIBRARIES
        arch
        gf
        glf
        hd
        hdSt
        hdx
        px_vp20
        sdf
        tf
        usd
        usdGeom
        usdImaging
        usdImagingGL
        usdMaya
        vt
        ${Boost_LIBRARIES}
        ${MAYA_LIBRARIES}

    INCLUDE_DIRS
        ${Boost_INCLUDE_DIRS}
        ${MAYA_INCLUDE_DIRS}

    PUBLIC_HEADERS
        api.h
        renderParams.h

    PUBLIC_CLASSES
        batchRenderer
        debugCodes
        hdImagingShapeDrawOverride
        hdImagingShapeUI
        hdRenderer
        package
        proxyDrawOverride
        proxyShapeUI
        sceneDelegate
        shapeAdapter
        softSelectHelper
        usdProxyShapeAdapter
        userData
        vp2Task

    PRIVATE_CLASSES
        instancerImager
        instancerShapeAdapter

    CPPFILES
        proxyShapeDelegate
        
    RESOURCE_FILES
        plugInfo.json
        shaders/renderPassVp2ShadowShader.glslfx
        shaders/vp2LightingShader.glslfx

    DISABLE_PRECOMPILED_HEADERS
)

