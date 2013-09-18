// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

import 'package:expect/expect.dart';
import "package:async_helper/async_helper.dart";
import 'compiler_helper.dart';
import 'parser_helper.dart' show buildSourceString;

const String TEST = """
returnInt1() {
  var a = 42;
  var f = () {
    return a;
  };
  return a;
}

returnDyn1() {
  var a = 42;
  var f = () {
    a = {};
  };
  return a;
}

returnInt2() {
  var a = 42;
  var f = () {
    a = 54;
  };
  return a;
}

returnDyn2() {
  var a = 42;
  var f = () {
    a = 54;
  };
  var g = () {
    a = {};
  };
  return a;
}

returnInt3() {
  var a = 42;
  if (a == 53) {
    var f = () {
      return a;
    };
  }
  return a;
}

returnDyn3() {
  var a = 42;
  if (a == 53) {
    var f = () {
      a = {};
    };
  }
  return a;
}

returnInt4() {
  var a = 42;
  g() { return a; }
  return g();
}

returnNum1() {
  var a = 42.5;
  try {
    g() {
      var b = {};
      b = 42;
      return b;
    }
    a = g();
  } finally {
  }
  return a;
}

class A {
  foo() {
    f() => this;
    return f();
  }
}

main() {
  returnInt1();
  returnDyn1();
  returnInt2();
  returnDyn2();
  returnInt3();
  returnDyn3();
  returnInt4();
  returnNum1();
  new A().foo();
}
""";


void main() {
  Uri uri = new Uri(scheme: 'source');
  var compiler = compilerFor(TEST, uri);
  asyncTest(() => compiler.runCompiler(uri).then((_) {
    var typesInferrer = compiler.typesTask.typesInferrer;

    checkReturn(String name, type) {
      var element = findElement(compiler, name);
      Expect.equals(type,
          typesInferrer.getReturnTypeOfElement(element).simplify(compiler));
    }

    checkReturn('returnInt1', compiler.typesTask.intType);
    checkReturn('returnInt2', compiler.typesTask.intType);
    checkReturn('returnInt3', compiler.typesTask.intType);
    checkReturn('returnInt4', compiler.typesTask.intType);

    checkReturn('returnDyn1', compiler.typesTask.dynamicType.nonNullable());
    checkReturn('returnDyn2', compiler.typesTask.dynamicType.nonNullable());
    checkReturn('returnDyn3', compiler.typesTask.dynamicType.nonNullable());
    checkReturn('returnNum1', compiler.typesTask.numType);

    checkReturnInClass(String className, String methodName, type) {
      var cls = findElement(compiler, className);
      var element = cls.lookupLocalMember(buildSourceString(methodName));
      Expect.equals(type,
          typesInferrer.getReturnTypeOfElement(element).simplify(compiler));
    }
    var cls = findElement(compiler, 'A');
    checkReturnInClass('A', 'foo', new TypeMask.nonNullExact(cls.rawType));
  }));
}
