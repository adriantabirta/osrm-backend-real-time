# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.25

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /opt/homebrew/Cellar/cmake/3.25.2/bin/cmake

# The command to remove a file.
RM = /opt/homebrew/Cellar/cmake/3.25.2/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/at/Development/osrm-backend-real-time/osrm-realtime

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/at/Development/osrm-backend-real-time/osrm-realtime/build

# Include any dependencies generated for this target.
include CMakeFiles/osrm-realtime-routing.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/osrm-realtime-routing.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/osrm-realtime-routing.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/osrm-realtime-routing.dir/flags.make

CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.o: CMakeFiles/osrm-realtime-routing.dir/flags.make
CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.o: /Users/at/Development/osrm-backend-real-time/osrm-realtime/osrm-realtime-routing.cpp
CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.o: CMakeFiles/osrm-realtime-routing.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/at/Development/osrm-backend-real-time/osrm-realtime/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.o -MF CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.o.d -o CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.o -c /Users/at/Development/osrm-backend-real-time/osrm-realtime/osrm-realtime-routing.cpp

CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/at/Development/osrm-backend-real-time/osrm-realtime/osrm-realtime-routing.cpp > CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.i

CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/at/Development/osrm-backend-real-time/osrm-realtime/osrm-realtime-routing.cpp -o CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.s

# Object files for target osrm-realtime-routing
osrm__realtime__routing_OBJECTS = \
"CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.o"

# External object files for target osrm-realtime-routing
osrm__realtime__routing_EXTERNAL_OBJECTS =

osrm-realtime-routing: CMakeFiles/osrm-realtime-routing.dir/osrm-realtime-routing.cpp.o
osrm-realtime-routing: CMakeFiles/osrm-realtime-routing.dir/build.make
osrm-realtime-routing: /opt/homebrew/lib/libboost_regex-mt.dylib
osrm-realtime-routing: /opt/homebrew/lib/libboost_date_time-mt.dylib
osrm-realtime-routing: /opt/homebrew/lib/libboost_chrono-mt.dylib
osrm-realtime-routing: /opt/homebrew/lib/libboost_filesystem-mt.dylib
osrm-realtime-routing: /opt/homebrew/lib/libboost_iostreams-mt.dylib
osrm-realtime-routing: /opt/homebrew/lib/libboost_thread-mt.dylib
osrm-realtime-routing: /opt/homebrew/lib/libboost_system-mt.dylib
osrm-realtime-routing: /opt/homebrew/lib/libtbb.12.9.dylib
osrm-realtime-routing: /Library/Developer/CommandLineTools/SDKs/MacOSX13.sdk/usr/lib/libz.tbd
osrm-realtime-routing: CMakeFiles/osrm-realtime-routing.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/at/Development/osrm-backend-real-time/osrm-realtime/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable osrm-realtime-routing"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/osrm-realtime-routing.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/osrm-realtime-routing.dir/build: osrm-realtime-routing
.PHONY : CMakeFiles/osrm-realtime-routing.dir/build

CMakeFiles/osrm-realtime-routing.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/osrm-realtime-routing.dir/cmake_clean.cmake
.PHONY : CMakeFiles/osrm-realtime-routing.dir/clean

CMakeFiles/osrm-realtime-routing.dir/depend:
	cd /Users/at/Development/osrm-backend-real-time/osrm-realtime/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/at/Development/osrm-backend-real-time/osrm-realtime /Users/at/Development/osrm-backend-real-time/osrm-realtime /Users/at/Development/osrm-backend-real-time/osrm-realtime/build /Users/at/Development/osrm-backend-real-time/osrm-realtime/build /Users/at/Development/osrm-backend-real-time/osrm-realtime/build/CMakeFiles/osrm-realtime-routing.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/osrm-realtime-routing.dir/depend

