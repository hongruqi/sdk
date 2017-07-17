// Copyright (c) 2017, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#if defined(DART_IO_DISABLED)

#include "bin/builtin.h"
#include "bin/dartutils.h"
#include "include/dart_api.h"

namespace dart {
namespace bin {

bool short_socket_read = false;

bool short_socket_write = false;

void FUNCTION_NAME(InternetAddress_Parse)(Dart_NativeArguments args) {
  Dart_ThrowException(
      DartUtils::NewDartArgumentError("Sockets unsupported on this platform"));
}

void FUNCTION_NAME(NetworkInterface_ListSupported)(Dart_NativeArguments args) {
  Dart_ThrowException(
      DartUtils::NewDartArgumentError("Sockets unsupported on this platform"));
}

void FUNCTION_NAME(SocketBase_IsBindError)(Dart_NativeArguments args) {
  Dart_ThrowException(
      DartUtils::NewDartArgumentError("Sockets unsupported on this platform"));
}

}  // namespace bin
}  // namespace dart

#endif  // defined(DART_IO_DISABLED)
