/*
 * SPDX-License-Identifier: Apache-2.0
 */

//====------ ConvertONNXToTOSA.cpp - ONNX dialects to TOSA lowering -------===//
//
// Copyright (c) 2022 Arm Limited.
//
// =============================================================================
//
// This file implements the lowering of frontend operations to the TOSA dialect.
//
//===----------------------------------------------------------------------===//

#include "src/Conversion/ONNXToTOSA/ONNXToTOSACommon.hpp"

using namespace mlir;

namespace onnx_mlir {

void populateONNXToTOSAConversionPattern(ConversionTarget &target,
    RewritePatternSet &patterns, TypeConverter &typeConverter,
    MLIRContext *ctx) {
  // Math
  populateLoweringONNXElementwiseOpToTOSAPattern(
      target, patterns, typeConverter, ctx);
  populateLoweringONNXGemmOpToTOSAPattern(target, patterns, typeConverter, ctx);
  populateLoweringONNXSoftmaxOpToTOSAPattern(
      target, patterns, typeConverter, ctx);
  populateLoweringONNXConvOpToTOSAPattern(target, patterns, typeConverter, ctx);
  // NN
  populateLoweringONNXMaxPoolSingleOutOpToTOSAPattern(
      target, patterns, typeConverter, ctx);
  // Tensor
  populateLoweringONNXConstOpToTOSAPattern(
      target, patterns, typeConverter, ctx);
}

// Performs lowering to TOSA dialect
struct FrontendToTosaLoweringPass
    : public PassWrapper<FrontendToTosaLoweringPass, OperationPass<ModuleOp>> {
  StringRef getArgument() const override { return "convert-onnx-to-tosa"; }

  StringRef getDescription() const override {
    return "Lower frontend ops to TOSA dialect.";
  }

  FrontendToTosaLoweringPass() = default;
  FrontendToTosaLoweringPass(const FrontendToTosaLoweringPass &pass)
      : PassWrapper<FrontendToTosaLoweringPass, OperationPass<ModuleOp>>() {}

  void runOnOperation() final;
};

void FrontendToTosaLoweringPass::runOnOperation() {
  ModuleOp module = getOperation();
  // Define final conversion target
  MLIRContext *context = &getContext();
  RewritePatternSet patterns(context);
  ConversionTarget target(*context);

  // We use the type converter to legalize types before any conversion patterns
  // are executed. This ensures that we do not need to trigger separate
  // conversion failures. Quantized types are not supported right now.
  TypeConverter typeConverter;
  typeConverter.addConversion([](Type type) -> Optional<Type> {
    if (isTOSASignedInt(type) || isTOSAFloat(type) || type.isa<NoneType>())
      return type;
    return std::nullopt;
  });
  typeConverter.addConversion([&](TensorType type) -> Optional<Type> {
    if (typeConverter.isLegal(type.getElementType()))
      return type;
    return std::nullopt;
  });

  // Define legal dialects and operations
  target.addLegalDialect<tosa::TosaDialect, func::FuncDialect,
      mlir::arith::ArithDialect>();

  // Define patterns
  populateONNXToTOSAConversionPattern(target, patterns, typeConverter, context);

  if (failed(applyPartialConversion(module, target, std::move(patterns)))) {
    signalPassFailure();
  }
}

std::unique_ptr<Pass> createConvertONNXToTOSAPass() {
  return std::make_unique<FrontendToTosaLoweringPass>();
}

} // namespace onnx_mlir
