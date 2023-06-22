set(DIRS
    usdImaging
    usdImagingGL
    usdImagingDX
    usdProcImaging
    usdRiImaging
    usdSkelImaging
    usdVolImaging
    usdAppUtils
    usdviewq
    bin
    plugin
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()
