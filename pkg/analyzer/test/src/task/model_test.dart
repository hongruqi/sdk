// Copyright (c) 2015, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

library test.src.task.model_test;

import 'package:analyzer/src/generated/engine.dart' hide AnalysisTask;
import 'package:analyzer/src/generated/java_engine.dart';
import 'package:analyzer/src/task/model.dart';
import 'package:analyzer/task/model.dart';
import 'package:unittest/unittest.dart';

import '../../generated/test_support.dart';
import '../../reflective_tests.dart';
import 'test_support.dart';

main() {
  groupSep = ' | ';
  runReflectiveTests(AnalysisTaskTest);
  runReflectiveTests(ContributionPointImplTest);
  runReflectiveTests(ResultDescriptorImplTest);
  runReflectiveTests(SimpleResultCachingPolicyTest);
  runReflectiveTests(TaskDescriptorImplTest);
}

@reflectiveTest
class AnalysisTaskTest extends EngineTestCase {
  test_getRequiredInput_missingKey() {
    AnalysisTarget target = new TestSource();
    AnalysisTask task = new TestAnalysisTask(null, target);
    task.inputs = {'a': 'b'};
    expect(() => task.getRequiredInput('c'),
        throwsA(new isInstanceOf<AnalysisException>()));
  }

  test_getRequiredInput_noInputs() {
    AnalysisTarget target = new TestSource();
    AnalysisTask task = new TestAnalysisTask(null, target);
    expect(() => task.getRequiredInput('x'),
        throwsA(new isInstanceOf<AnalysisException>()));
  }

  test_getRequiredInput_valid() {
    AnalysisTarget target = new TestSource();
    AnalysisTask task = new TestAnalysisTask(null, target);
    String key = 'a';
    String value = 'b';
    task.inputs = {key: value};
    expect(task.getRequiredInput(key), value);
  }

  test_getRequiredSource() {
    AnalysisTarget target = new TestSource();
    AnalysisTask task = new TestAnalysisTask(null, target);
    expect(task.getRequiredSource(), target);
  }
}

@reflectiveTest
class ContributionPointImplTest extends EngineTestCase {
  test_contributors_empty() {
    CompositeResultDescriptorImpl point =
        new CompositeResultDescriptorImpl('point');
    List<ResultDescriptor> contributors = point.contributors;
    expect(contributors, isEmpty);
  }

  test_contributors_nonEmpty() {
    ResultDescriptorImpl result1 = new ResultDescriptorImpl('result1', null);
    ResultDescriptorImpl result2 = new ResultDescriptorImpl('result2', null);
    CompositeResultDescriptorImpl point =
        new CompositeResultDescriptorImpl('point');
    point.recordContributor(result1);
    point.recordContributor(result2);
    List<ResultDescriptor> contributors = point.contributors;
    expect(contributors, isNotNull);
    expect(contributors, hasLength(2));
    if (!(contributors[0] == result1 && contributors[1] == result2) ||
        (contributors[0] == result2 && contributors[1] == result1)) {
      fail("Invalid contributors: $contributors");
    }
  }

  test_create() {
    expect(new CompositeResultDescriptorImpl('name'), isNotNull);
  }

  test_name() {
    String name = 'point';
    CompositeResultDescriptorImpl point =
        new CompositeResultDescriptorImpl(name);
    expect(point.name, name);
  }
}

@reflectiveTest
class ResultDescriptorImplTest extends EngineTestCase {
  test_create_withCachingPolicy() {
    ResultCachingPolicy policy = new SimpleResultCachingPolicy(128, 16);
    ResultDescriptorImpl result =
        new ResultDescriptorImpl('result', null, cachingPolicy: policy);
    expect(result.cachingPolicy, same(policy));
  }

  test_create_withContribution() {
    CompositeResultDescriptorImpl point =
        new CompositeResultDescriptorImpl('point');
    ResultDescriptorImpl result =
        new ResultDescriptorImpl('result', null, contributesTo: point);
    expect(result, isNotNull);
    List<ResultDescriptor> contributors = point.contributors;
    expect(contributors, unorderedEquals([result]));
  }

  test_create_withoutCachingPolicy() {
    ResultDescriptorImpl result = new ResultDescriptorImpl('result', null);
    ResultCachingPolicy cachingPolicy = result.cachingPolicy;
    expect(cachingPolicy, isNotNull);
    expect(cachingPolicy.maxActiveSize, -1);
    expect(cachingPolicy.maxIdleSize, -1);
  }

  test_create_withoutContribution() {
    expect(new ResultDescriptorImpl('name', null), isNotNull);
  }

  test_inputFor() {
    AnalysisTarget target = new TestSource();
    ResultDescriptorImpl result = new ResultDescriptorImpl('result', null);
    TaskInput input = result.of(target);
    expect(input, isNotNull);
  }

  test_name() {
    String name = 'result';
    ResultDescriptorImpl result = new ResultDescriptorImpl(name, null);
    expect(result.name, name);
  }
}

@reflectiveTest
class SimpleResultCachingPolicyTest extends EngineTestCase {
  test_create() {
    ResultCachingPolicy policy = new SimpleResultCachingPolicy(256, 32);
    expect(policy.maxActiveSize, 256);
    expect(policy.maxIdleSize, 32);
    expect(policy.measure(null), 1);
  }
}

@reflectiveTest
class TaskDescriptorImplTest extends EngineTestCase {
  test_create() {
    String name = 'name';
    BuildTask buildTask = (context, target) {};
    CreateTaskInputs createTaskInputs = (target) {};
    List<ResultDescriptor> results = <ResultDescriptor>[];
    TaskDescriptorImpl descriptor =
        new TaskDescriptorImpl(name, buildTask, createTaskInputs, results);
    expect(descriptor, isNotNull);
    expect(descriptor.name, name);
    expect(descriptor.buildTask, equals(buildTask));
    expect(descriptor.createTaskInputs, equals(createTaskInputs));
    expect(descriptor.results, results);
  }

  test_createTask() {
    BuildTask buildTask =
        (context, target) => new TestAnalysisTask(context, target);
    CreateTaskInputs createTaskInputs = (target) {};
    List<ResultDescriptor> results = <ResultDescriptor>[];
    TaskDescriptorImpl descriptor =
        new TaskDescriptorImpl('name', buildTask, createTaskInputs, results);
    AnalysisContext context = null;
    AnalysisTarget target = new TestSource();
    Map<String, dynamic> inputs = {};
    String inputMemento = 'main() {}';
    AnalysisTask createTask =
        descriptor.createTask(context, target, inputs, inputMemento);
    expect(createTask, isNotNull);
    expect(createTask.context, context);
    expect(createTask.inputs, inputs);
    expect(createTask.inputMemento, inputMemento);
    expect(createTask.target, target);
  }
}
