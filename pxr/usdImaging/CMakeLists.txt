set(DIRS
    usdImaging
    hdEmscripten
    usdImagingGL
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
