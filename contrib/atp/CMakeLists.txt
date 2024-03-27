check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

build_lib(
    LIBNAME atp
    SOURCE_FILES model/sim-channel.cc
                 model/sim-net-device.cc
                 model/atp-driver.cc
                 model/atp-header.cc
    HEADER_FILES model/sim-channel.h
                 model/sim-net-device.h
                 model/atp-driver.h
                 model/atp-server.h
                 model/Common.h
                 model/atp-client.h
                 model/HashTable.h
                 model/WindowManager.h
                 model/CC_manager.h
                 model/atp-header.h
                 helper/atp-driver-helper.h
                 utils/atp-tag.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libnetwork}
                      ${libinternet}
                      ${libpoint-to-point}
                      ${libapplications}
)
