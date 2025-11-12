function(enable_sanitizers project_name)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES
                                             ".*Clang")
    set(LIST_OF_SANITIZERS "")

    option(ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" FALSE)
    if(ENABLE_SANITIZER_ADDRESS)
      set(LIST_OF_SANITIZERS "address")
    endif()

    option(ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" FALSE)
    if(ENABLE_SANITIZER_MEMORY)
      if (LIST_OF_SANITIZERS)
        set(LIST_OF_SANITIZERS "${LIST_OF_SANITIZERS}, memory")
      else()
        set(LIST_OF_SANITIZERS "memory")
      endif()
    endif()

    option(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR
           "Enable undefined behavior sanitizer" FALSE)
    if(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR)
      if (LIST_OF_SANITIZERS)
        set(LIST_OF_SANITIZERS "${LIST_OF_SANITIZERS}, undefined")
      else()
        set(LIST_OF_SANITIZERS "undefined")
      endif()
    endif()

    option(ENABLE_SANITIZER_THREAD "Enable thread sanitizer" FALSE)
    if(ENABLE_SANITIZER_THREAD)
      if (LIST_OF_SANITIZERS)
        set(LIST_OF_SANITIZERS "${LIST_OF_SANITIZERS}, thread")
      else()
        set(LIST_OF_SANITIZERS "thread")
      endif()
    endif()
  endif()

  if(LIST_OF_SANITIZERS)
    if(NOT "${LIST_OF_SANITIZERS}" STREQUAL "")
      target_compile_options(${project_name}
                             INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
      target_link_libraries(${project_name}
                            INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
    endif()
  endif()

endfunction()

