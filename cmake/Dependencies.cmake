# Platform dependencies for the JUCE shell

if(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(SECURITY_FRAMEWORK Security REQUIRED)
endif()
