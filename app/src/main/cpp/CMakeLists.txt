cmake_minimum_required( VERSION 3.4.1 )

add_library( mfcc_impl01 SHARED mfcc_impl01.cpp )

add_library( cpufeatures STATIC ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c )

target_include_directories(mfcc_impl01 PRIVATE ${ANDROID_NDK}/sources/android/cpufeatures )

target_link_libraries( mfcc_impl01 cpufeatures android log )

if ( ${ANDROID_ABI} STREQUAL "armeabi-v7a" )

    # make a list of neon files and add neon compiling flags to them
    set( neon_SRCS  mfcc_impl01.cpp )

    set_property( SOURCE ${neon_SRCS} APPEND_STRING PROPERTY COMPILE_FLAGS " -mfpu=neon -O3" )
    add_definitions( "-DHAVE_NEON=1" )

elseif ( ${ANDROID_ABI} STREQUAL "x86" )

    set( neon_SRCS  mfcc_impl01 )

    set_property( SOURCE ${neon_SRCS} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -mssse3 \
              -Wno-unknown-attributes \
              -Wno-deprecated-declarations \
              -Wno-constant-conversion \
              -Wno-static-in-inline \
              -O3 " )

    add_definitions( -DHAVE_NEON_X86=1 -DHAVE_NEON=1 )

elseif ( ${ANDROID_ABI} STREQUAL "x86_64" )

    set( neon_SRCS  mfcc_impl01 )

    set_property( SOURCE ${neon_SRCS} APPEND_STRING PROPERTY COMPILE_FLAGS
            " -mssse3 \
              -Wno-unknown-attributes \
              -Wno-deprecated-declarations \
              -Wno-constant-conversion \
              -Wno-static-in-inline \
              -O3 " )

    add_definitions( -DHAVE_NEON_X86=1 -DHAVE_NEON=1 )

else ()
    set( neon_SRCS )

endif ()