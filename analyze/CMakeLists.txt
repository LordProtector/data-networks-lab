#The name of the project to build
PROJECT(Analyze)

# We currently require at least version 2.6 of cmake
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)


# Tell cmake to generate an executable called analyze that depends
# on the file analyze.cpp
ADD_EXECUTABLE(analyze analyze.cpp)

