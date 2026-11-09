// Copyright 2015 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TRACETOOLS__VERSION_H_
#define TRACETOOLS__VERSION_H_

/// \def TRACETOOLS_VERSION_MAJOR
/// Defines TRACETOOLS major version number
#define TRACETOOLS_VERSION_MAJOR (8)

/// \def TRACETOOLS_VERSION_MINOR
/// Defines TRACETOOLS minor version number
#define TRACETOOLS_VERSION_MINOR (2)

/// \def TRACETOOLS_VERSION_PATCH
/// Defines TRACETOOLS version patch number
#define TRACETOOLS_VERSION_PATCH (6)

/// \def TRACETOOLS_VERSION_STR
/// Defines TRACETOOLS version string
#define TRACETOOLS_VERSION_STR "8.2.6"

/// \def TRACETOOLS_VERSION_GTE
/// Defines a macro to check whether the version of TRACETOOLS is greater than or equal to
/// the given version triple.
#define TRACETOOLS_VERSION_GTE(major, minor, patch) ( \
     (major < TRACETOOLS_VERSION_MAJOR) ? true \
     : ((major > TRACETOOLS_VERSION_MAJOR) ? false \
     : ((minor < TRACETOOLS_VERSION_MINOR) ? true \
     : ((minor > TRACETOOLS_VERSION_MINOR) ? false \
     : ((patch < TRACETOOLS_VERSION_PATCH) ? true \
     : ((patch > TRACETOOLS_VERSION_PATCH) ? false \
     : true))))))

#endif  // TRACETOOLS__VERSION_H_
