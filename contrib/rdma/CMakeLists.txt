check_include_file_cxx(stdint.h HAVE_STDINT_H)
if(HAVE_STDINT_H)
    add_definitions(-DHAVE_STDINT_H)
endif()

build_lib(
    LIBNAME rdma
    SOURCE_FILES 
        model/switch-mmu.cc
        model/switch-node.cc  
        model/rdma-queue-pair.cc      
        model/cn-header.cc
        model/pause-header.cc
        model/qbb-net-device.cc
        model/qbb-channel.cc
        model/qbb-header.cc
        helper/qbb-helper.cc
        model/rdma-hw.cc
        model/rdma-driver.cc
        model/rdma-client.cc
        helper/rdma-client-helper.cc
        model/seq-pg-header.cc
    HEADER_FILES 
        model/switch-mmu.h
        model/switch-node.h
        model/rdma-queue-pair.h
        model/pause-header.h
        model/pause-header.h
        model/qbb-net-device.h
        model/qbb-channel.h
        model/rdma.h
        model/qbb-header.h
        model/cn-header.h
        helper/qbb-helper.h
        model/rdma-hw.h
        model/rdma-driver.h
        model/rdma-client.h
        helper/rdma-client-helper.h
        model/seq-pg-header.h
    LIBRARIES_TO_LINK ${libcore}
                      ${libnetwork}
                      ${libinternet}
                      ${libpoint-to-point}
                      ${libapplications}
)
