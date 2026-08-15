// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "gtest/gtest.h"

#include "core/graph/model.h"
#include "core/optimizer/graph_transformer_mgr.h"
#include "core/optimizer/reshape_fusion.h"
#include "test/optimizer/graph_transform_test_fixture.h"
#include "test/unittest_util/framework_test_utils.h"

using namespace ONNX_NAMESPACE;

namespace onnxruntime {
namespace test {

// A Reshape output used as another Reshape's shape input is not a contiguous
// data-reshape chain. Fusing it would replace the second Reshape's float data
// input with the first Reshape's int64 data input and create an invalid graph.
// See https://github.com/microsoft/onnxruntime/issues/32105.
TEST_F(GraphTransformationTests, ReshapeFusionOutputUsedAsShapeInput) {
  std::unordered_map<std::string, int> domain_to_version;
  domain_to_version[kOnnxDomain] = 21;
  Model model("ReshapeFusionOutputUsedAsShapeInput", false, ModelMetaData(),
              PathString(), IOnnxRuntimeOpSchemaRegistryList(), domain_to_version,
              std::vector<ONNX_NAMESPACE::FunctionProto>(), *logger_);
  auto& graph = model.MainGraph();

  TypeProto shape_source_type;
  shape_source_type.mutable_tensor_type()->set_elem_type(TensorProto_DataType_INT64);
  shape_source_type.mutable_tensor_type()->mutable_shape()->add_dim()->set_dim_value(1);

  TypeProto data_type;
  data_type.mutable_tensor_type()->set_elem_type(TensorProto_DataType_FLOAT);
  data_type.mutable_tensor_type()->mutable_shape()->add_dim()->set_dim_value(2);

  TypeProto reshaped_shape_type;
  reshaped_shape_type.mutable_tensor_type()->set_elem_type(TensorProto_DataType_INT64);
  reshaped_shape_type.mutable_tensor_type()->mutable_shape()->add_dim()->set_dim_value(1);

  TypeProto output_type;
  output_type.mutable_tensor_type()->set_elem_type(TensorProto_DataType_FLOAT);
  output_type.mutable_tensor_type()->mutable_shape()->add_dim()->set_dim_value(2);

  auto& shape_source = graph.GetOrCreateNodeArg("shape_source", &shape_source_type);
  auto& data = graph.GetOrCreateNodeArg("data", &data_type);
  auto& reshaped_shape = graph.GetOrCreateNodeArg("reshaped_shape", &reshaped_shape_type);
  auto& output = graph.GetOrCreateNodeArg("output", &output_type);

  TensorProto first_shape_proto;
  first_shape_proto.set_name("first_shape");
  first_shape_proto.set_data_type(TensorProto_DataType_INT64);
  first_shape_proto.add_dims(1);
  first_shape_proto.add_int64_data(1);
  graph.AddInitializedTensor(first_shape_proto);
  auto& first_shape = graph.GetOrCreateNodeArg("first_shape", nullptr);

  graph.AddNode("reshape_shape", "Reshape", "reshape shape tensor",
                {&shape_source, &first_shape}, {&reshaped_shape});
  graph.AddNode("reshape_data", "Reshape", "reshape data using computed shape",
                {&data, &reshaped_shape}, {&output});
  graph.SetInputs({&shape_source, &data});
  graph.SetOutputs({&output});

  ASSERT_STATUS_OK(graph.Resolve());
  ASSERT_EQ(OpCount(CountOpsInGraph(graph), "Reshape"), 2);

  GraphTransformerManager graph_transformation_mgr{5};
  ASSERT_STATUS_OK(graph_transformation_mgr.Register(std::make_unique<ReshapeFusion>(),
                                                     TransformerLevel::Level1));
  ASSERT_STATUS_OK(graph_transformation_mgr.ApplyTransformers(graph, TransformerLevel::Level1, *logger_));

  EXPECT_EQ(OpCount(CountOpsInGraph(graph), "Reshape"), 2);
}

}  // namespace test
}  // namespace onnxruntime
