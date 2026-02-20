cmake_minimum_required(VERSION 3.22)
project(PluginBuild VERSION 1.0.0)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE")
add_subdirectory(${JUCE_DIR} JUCE)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/plugin_name.txt" PLUGIN_NAME)
string(STRIP "${PLUGIN_NAME}" PLUGIN_NAME)

add_subdirectory("plugin/${PLUGIN_NAME}")
