if (TARGET tinyusb_device)
    add_executable(${ProjectName}
        main.c
    )

    target_include_directories(${ProjectName} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
    )

    target_link_libraries(${ProjectName} 
        pico_stdlib 
        FreeRTOS-Kernel-Heap4 
    )

    

    # Enable USB output, disable UART output
    pico_enable_stdio_usb(${ProjectName} 1)
    pico_enable_stdio_uart(${ProjectName} 0)

    pico_add_extra_outputs(${ProjectName})
    
    # Add URL via pico_set_program_url if needed
    # example_auto_set_url(${ProjectName})
elseif(PICO_ON_DEVICE)
    message(WARNING "USB not available: TinyUSB submodule is not initialized in the SDK")
endif()
