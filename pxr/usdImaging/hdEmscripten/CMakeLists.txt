set(PXR_PREFIX pxr/hdEmscripten)
set(PXR_PACKAGE hdEmscripten)

if (NOT PXR_ENABLE_JS_SUPPORT)
  return()
endif()
pxr_library(hdEmscripten
    LIBRARIES
      hd
      usd
      usdUtils
      sdf
      -Wl,--whole-archive usdShaders -Wl,--no-whole-archive

    PUBLIC_CLASSES
        webRenderDelegate

    PUBLIC_HEADERS
        webRenderDelegate.h
)

set(RESOURCE_PARAMETERS "")
function(add_resources target)
    # This local var is needed since list append cannot update the PARENT_SCOPE
    set(LOCAL_VAR ${RESOURCE_PARAMETERS})
    get_property(RESSOURCES TARGET ${target} PROPERTY EMSCRIPTEN_RESOURCES)
    message("RESOURCE FILES ${RESSOURCES}")
    list(APPEND LOCAL_VAR "${RESSOURCES}")
    set(RESOURCE_PARAMETERS "${LOCAL_VAR}" PARENT_SCOPE)
endfunction()

add_resources(usdShade)
add_resources(sdf)
add_resources(hd)
add_resources(usdHydra)
add_resources(usdShaders)
#add_resources(glf)
add_resources(usdImaging)
add_resources(usd)
add_resources(usdLux)
add_resources(ar)
add_resources(usdGeom)
add_resources(ndr)

list(APPEND RESOURCE_PARAMETERS "--preload-file ${PROJECT_BINARY_DIR}/plugins_plugInfo.json@/usd/plugInfo.json")

message("RESOURCE LIST ${RESOURCE_PARAMETERS}")

pxr_cpp_bin(emHdBindings
  LIBRARIES
      hdEmscripten
      usdImaging
      ${RESOURCE_PARAMETERS}
)

install(
    FILES 
        ${CMAKE_CURRENT_BINARY_DIR}/emHdBindings.js 
        ${CMAKE_CURRENT_BINARY_DIR}/emHdBindings.data 
        ${CMAKE_CURRENT_BINARY_DIR}/emHdBindings.wasm 
        ${CMAKE_CURRENT_BINARY_DIR}/emHdBindings.worker.js 
    DESTINATION hdEmscripten
)
