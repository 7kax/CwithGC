add_library(gc_common OBJECT
    common/memory_layout.cpp
    common/pointer_table.cpp
)

target_include_directories(gc_common
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)
gc_apply_build_options(gc_common)

if(GC_ENABLE_DEBUG_API)
    target_compile_definitions(gc_common PRIVATE GC_DEBUG)
endif()

function(add_gc_collector TARGET SOURCE)
    add_library(${TARGET} STATIC
        ${SOURCE}
        $<TARGET_OBJECTS:gc_common>
    )

    target_include_directories(${TARGET}
        PUBLIC
            ${PROJECT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    gc_apply_build_options(${TARGET})

    if(GC_ENABLE_DEBUG_API)
        target_compile_definitions(${TARGET} PUBLIC GC_DEBUG)
    endif()
endfunction()

add_gc_collector(gc_mark_sweep collectors/mark_sweep/mark_sweep.cpp)
add_gc_collector(gc_copying collectors/copying/copying.cpp)
add_gc_collector(gc_ref_count collectors/ref_count/ref_count.cpp)
