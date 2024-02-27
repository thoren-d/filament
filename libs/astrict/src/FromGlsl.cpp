/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <astrict/FromGlsl.h>

#include <astrict/CommonTypes.h>
#include <astrict/DebugGlsl.h>
#include <utils/Log.h>
#include <utils/Panic.h>
#include <optional>
#include <set>

#include "Types.h"
#include "glslang/MachineIndependent/localintermediate.h"
#include "intermediate.h"

namespace astrict {

std::string textureFunctionNameForSampler(
        const char* suffix, const char* arbOrExt, int version, const glslang::TType* arg1Type) {
    using namespace glslang;
    if (version > 100) {
        return std::string("texture") + suffix;
    }
    ASSERT_PRECONDITION(arg1Type, "First argument to texture function must not be null");
    auto sampler = arg1Type->getSampler();
    std::string result = sampler.isShadow() ? "shadow" : "texture";
    switch (sampler.dim) {
        case Esd1D: result += "1D"; break;
        case Esd2D: result += "2D"; break;
        case Esd3D: result += "3D"; break;
        case EsdCube: result += "Cube"; break;
        case EsdRect: result += "2DRect"; break;
        default:
            PANIC_PRECONDITION("Unhandled sampler dimension: %d",
                    sampler.dim);
    }
    result += suffix;
    result += arbOrExt;
    return result;
}

std::string constructorFunctionNameForType(const char* name, const glslang::TType& returnType) {
    if (!returnType.isArray()) {
        return name;
    }
    std::string result = name;
    const int numDims = returnType.getArraySizes()->getNumDims();
    for (int i = 0; i < numDims; i++) {
        result += "[]";
    }
    return result;
}

std::variant<RValueOperator, std::string> glslangOperatorToRValueOperator(
        glslang::TOperator op, int version,
        const glslang::TType& returnType, const glslang::TType* arg1Type) {
    using namespace glslang;
    switch (op) {
        case EOpNegative: return RValueOperator::Negative;
        case EOpLogicalNot: return RValueOperator::LogicalNot;
        case EOpVectorLogicalNot: return "not";
        case EOpBitwiseNot: return RValueOperator::BitwiseNot;
        case EOpPostIncrement: return RValueOperator::PostIncrement;
        case EOpPostDecrement: return RValueOperator::PostDecrement;
        case EOpPreIncrement: return RValueOperator::PreIncrement;
        case EOpPreDecrement: return RValueOperator::PreDecrement;
        case EOpConvIntToBool:
        case EOpConvUintToBool:
        case EOpConvFloatToBool:
        case EOpConvDoubleToBool:
            return "bool";
        case EOpConvBoolToInt:
        case EOpConvUintToInt:
        case EOpConvFloatToInt:
        case EOpConvDoubleToInt:
            return "int";
        case EOpConvBoolToFloat:
        case EOpConvIntToFloat:
        case EOpConvUintToFloat:
        case EOpConvDoubleToFloat:
            return "float";
        case EOpConvBoolToDouble:
        case EOpConvIntToDouble:
        case EOpConvUintToDouble:
        case EOpConvFloatToDouble:
            return "double";
        case EOpConvBoolToUint:
        case EOpConvIntToUint:
        case EOpConvFloatToUint:
        case EOpConvDoubleToUint:
            return "uint";
        case EOpAdd: return RValueOperator::Add;
        case EOpSub: return RValueOperator::Sub;
        case EOpMul:
        case EOpVectorTimesScalar:
        case EOpVectorTimesMatrix:
        case EOpMatrixTimesVector:
        case EOpMatrixTimesScalar:
            return RValueOperator::Mul;
        case EOpDiv: return RValueOperator::Div;
        case EOpMod: return RValueOperator::Mod;
        case EOpRightShift: return RValueOperator::RightShift;
        case EOpLeftShift: return RValueOperator::LeftShift;
        case EOpAnd: return RValueOperator::And;
        case EOpInclusiveOr: return RValueOperator::InclusiveOr;
        case EOpExclusiveOr: return RValueOperator::ExclusiveOr;
        case EOpEqual: return RValueOperator::Equal;
        case EOpNotEqual: return RValueOperator::NotEqual;
        case EOpVectorEqual: return "equal";
        case EOpVectorNotEqual: return "notEqual";
        case EOpLessThan: return RValueOperator::LessThan;
        case EOpGreaterThan: return RValueOperator::GreaterThan;
        case EOpLessThanEqual: return RValueOperator::LessThanEqual;
        case EOpGreaterThanEqual: return RValueOperator::GreaterThanEqual;
        case EOpComma: return RValueOperator::Comma;
        case EOpLogicalOr: return RValueOperator::LogicalOr;
        case EOpLogicalXor: return RValueOperator::LogicalXor;
        case EOpLogicalAnd: return RValueOperator::LogicalAnd;
        case EOpIndexDirect:
        case EOpIndexIndirect:
            return RValueOperator::Index;
        case EOpIndexDirectStruct: return RValueOperator::IndexStruct;
        case EOpVectorSwizzle: return RValueOperator::VectorSwizzle;
        case EOpRadians: return "radians";
        case EOpDegrees: return "degrees";
        case EOpSin: return "sin";
        case EOpCos: return "cos";
        case EOpTan: return "tan";
        case EOpAsin: return "asin";
        case EOpAcos: return "acos";
        case EOpAtan: return "atan";
        case EOpSinh: return "sinh";
        case EOpCosh: return "cosh";
        case EOpTanh: return "tanh";
        case EOpAsinh: return "asinh";
        case EOpAcosh: return "acosh";
        case EOpAtanh: return "atanh";
        case EOpPow: return "pow";
        case EOpExp: return "exp";
        case EOpLog: return "log";
        case EOpExp2: return "exp2";
        case EOpLog2: return "log2";
        case EOpSqrt: return "sqrt";
        case EOpInverseSqrt: return "inversesqrt";
        case EOpAbs: return "abs";
        case EOpSign: return "sign";
        case EOpFloor: return "floor";
        case EOpTrunc: return "trunc";
        case EOpRound: return "round";
        case EOpRoundEven: return "roundEven";
        case EOpCeil: return "ceil";
        case EOpFract: return "fract";
        case EOpModf: return "modf";
        case EOpMin: return "min";
        case EOpMax: return "max";
        case EOpClamp: return "clamp";
        case EOpMix: return "mix";
        case EOpStep: return "step";
        case EOpSmoothStep: return "smoothstep";
        case EOpIsNan: return "isnan";
        case EOpIsInf: return "isinf";
        case EOpFma: return "fma";
        case EOpFrexp: return "frexp";
        case EOpLdexp: return "ldexp";
        case EOpFloatBitsToInt: return "floatBitsToInt";
        case EOpFloatBitsToUint: return "floatBitsToUint";
        case EOpIntBitsToFloat: return "intBitsToFloat";
        case EOpUintBitsToFloat: return "uintBitsToFloat";
        case EOpPackSnorm2x16: return "packSnorm2x16";
        case EOpUnpackSnorm2x16: return "unpackSnorm2x16";
        case EOpPackUnorm2x16: return "packUnorm2x16";
        case EOpUnpackUnorm2x16: return "unpackUnorm2x16";
        case EOpPackSnorm4x8: return "packSnorm4x8";
        case EOpUnpackSnorm4x8: return "unpackSnorm4x8";
        case EOpPackUnorm4x8: return "packUnorm4x8";
        case EOpUnpackUnorm4x8: return "unpackUnorm4x8";
        case EOpPackHalf2x16: return "packHalf2x16";
        case EOpUnpackHalf2x16: return "unpackHalf2x16";
        case EOpPackDouble2x32: return "packDouble2x32";
        case EOpUnpackDouble2x32: return "unpackDouble2x32";
        case EOpPackInt2x32: return "packInt2x32";
        case EOpUnpackInt2x32: return "unpackInt2x32";
        case EOpPackUint2x32: return "packUint2x32";
        case EOpUnpackUint2x32: return "unpackUint2x32";
        case EOpPackFloat2x16: return "packFloat2x16";
        case EOpUnpackFloat2x16: return "unpackFloat2x16";
        case EOpPackInt2x16: return "packInt2x16";
        case EOpUnpackInt2x16: return "unpackInt2x16";
        case EOpPackUint2x16: return "packUint2x16";
        case EOpUnpackUint2x16: return "unpackUint2x16";
        case EOpPackInt4x16: return "packInt4x16";
        case EOpUnpackInt4x16: return "unpackInt4x16";
        case EOpPackUint4x16: return "packUint4x16";
        case EOpUnpackUint4x16: return "unpackUint4x16";
        case EOpPack16: return "pack16";
        case EOpPack32: return "pack32";
        case EOpPack64: return "pack64";
        case EOpUnpack32: return "unpack32";
        case EOpUnpack16: return "unpack16";
        case EOpUnpack8: return "unpack8";
        case EOpLength: return "length";
        case EOpDistance: return "distance";
        case EOpDot: return "dot";
        case EOpCross: return "cross";
        case EOpNormalize: return "normalize";
        case EOpFaceForward: return "faceforward";
        case EOpReflect: return "reflect";
        case EOpRefract: return "refract";
        case EOpMin3: return "min3";
        case EOpMax3: return "max3";
        case EOpMid3: return "mid3";
        case EOpDPdx: return "dFdx";
        case EOpDPdy: return "dFdy";
        case EOpFwidth: return "fwidth";
        case EOpDPdxFine: return "dFdxFine";
        case EOpDPdyFine: return "dFdyFine";
        case EOpFwidthFine: return "fwidthFine";
        case EOpDPdxCoarse: return "dFdxCoarse";
        case EOpDPdyCoarse: return "dFdyCoarse";
        case EOpFwidthCoarse: return "fwidthCoarse";
        case EOpInterpolateAtCentroid: return "interpolateAtCentroid";
        case EOpInterpolateAtSample: return "interpolateAtSample";
        case EOpInterpolateAtOffset: return "interpolateAtOffset";
        case EOpInterpolateAtVertex: return "interpolateAtVertexAMD";
        case EOpOuterProduct: return "outerProduct";
        case EOpDeterminant: return "determinant";
        case EOpMatrixInverse: return "inverse";
        case EOpTranspose: return "transpose";
        case EOpFtransform: return "ftransform";
        case EOpEmitVertex: return "EmitVertex";
        case EOpEndPrimitive: return "EndPrimitive";
        case EOpEmitStreamVertex: return "EmitStreamVertex";
        case EOpEndStreamPrimitive: return "EndStreamPrimitive";
        case EOpBarrier: return "barrier";
        case EOpMemoryBarrier: return "memoryBarrier";
        case EOpMemoryBarrierAtomicCounter: return "memoryBarrierAtomicCounter";
        case EOpMemoryBarrierBuffer: return "memoryBarrierBuffer";
        case EOpMemoryBarrierImage: return "memoryBarrierImage";
        case EOpMemoryBarrierShared: return "memoryBarrierShared";
        case EOpGroupMemoryBarrier: return "groupMemoryBarrier";
        case EOpBallot: return "ballotARB";
        case EOpReadInvocation: return "readInvocationARB";
        case EOpReadFirstInvocation: return "readFirstInvocationARB";
        case EOpAnyInvocation: return version >= 460 ? "anyInvocation" : "anyInvocationARB";
        case EOpAllInvocations: return version >= 460 ? "allInvocations" : "allInvocationsARB";
        case EOpAllInvocationsEqual: return version >= 460 ? "allInvocationsEqual" : "allInvocationsEqualARB";
        case EOpSubgroupBarrier: return "subgroupBarrier";
        case EOpSubgroupMemoryBarrier: return "subgroupMemoryBarrier";
        case EOpSubgroupMemoryBarrierBuffer: return "subgroupMemoryBarrierBuffer";
        case EOpSubgroupMemoryBarrierImage: return "subgroupMemoryBarrierImage";
        case EOpSubgroupMemoryBarrierShared: return "subgroupMemoryBarrierShared";
        case EOpSubgroupElect: return "subgroupElect";
        case EOpSubgroupAll: return "subgroupAll";
        case EOpSubgroupAny: return "subgroupAny";
        case EOpSubgroupAllEqual: return "subgroupAllEqual";
        case EOpSubgroupBroadcast: return "subgroupBroadcast";
        case EOpSubgroupBroadcastFirst: return "subgroupBroadcastFirst";
        case EOpSubgroupBallot: return "subgroupBallot";
        case EOpSubgroupInverseBallot: return "subgroupInverseBallot";
        case EOpSubgroupBallotBitExtract: return "subgroupBallotBitExtract";
        case EOpSubgroupBallotBitCount: return "subgroupBallotBitCount";
        case EOpSubgroupBallotInclusiveBitCount: return "subgroupBallotInclusiveBitCount";
        case EOpSubgroupBallotExclusiveBitCount: return "subgroupBallotExclusiveBitCount";
        case EOpSubgroupBallotFindLSB: return "subgroupBallotFindLSB";
        case EOpSubgroupBallotFindMSB: return "subgroupBallotFindMSB";
        case EOpSubgroupShuffle: return "subgroupShuffle";
        case EOpSubgroupShuffleXor: return "subgroupShuffleXor";
        case EOpSubgroupShuffleUp: return "subgroupShuffleUp";
        case EOpSubgroupShuffleDown: return "subgroupShuffleDown";
        case EOpSubgroupAdd: return "subgroupAdd";
        case EOpSubgroupMul: return "subgroupMul";
        case EOpSubgroupMin: return "subgroupMin";
        case EOpSubgroupMax: return "subgroupMax";
        case EOpSubgroupAnd: return "subgroupAnd";
        case EOpSubgroupOr: return "subgroupOr";
        case EOpSubgroupXor: return "subgroupXor";
        case EOpSubgroupInclusiveAdd: return "subgroupInclusiveAdd";
        case EOpSubgroupInclusiveMul: return "subgroupInclusiveMul";
        case EOpSubgroupInclusiveMin: return "subgroupInclusiveMin";
        case EOpSubgroupInclusiveMax: return "subgroupInclusiveMax";
        case EOpSubgroupInclusiveAnd: return "subgroupInclusiveAnd";
        case EOpSubgroupInclusiveOr: return "subgroupInclusiveOr";
        case EOpSubgroupInclusiveXor: return "subgroupInclusiveXor";
        case EOpSubgroupExclusiveAdd: return "subgroupExclusiveAdd";
        case EOpSubgroupExclusiveMul: return "subgroupExclusiveMul";
        case EOpSubgroupExclusiveMin: return "subgroupExclusiveMin";
        case EOpSubgroupExclusiveMax: return "subgroupExclusiveMax";
        case EOpSubgroupExclusiveAnd: return "subgroupExclusiveAnd";
        case EOpSubgroupExclusiveOr: return "subgroupExclusiveOr";
        case EOpSubgroupExclusiveXor: return "subgroupExclusiveXor";
        case EOpSubgroupClusteredAdd: return "subgroupClusteredAdd";
        case EOpSubgroupClusteredMul: return "subgroupClusteredMul";
        case EOpSubgroupClusteredMin: return "subgroupClusteredMin";
        case EOpSubgroupClusteredMax: return "subgroupClusteredMax";
        case EOpSubgroupClusteredAnd: return "subgroupClusteredAnd";
        case EOpSubgroupClusteredOr: return "subgroupClusteredOr";
        case EOpSubgroupClusteredXor: return "subgroupClusteredXor";
        case EOpSubgroupQuadBroadcast: return "subgroupQuadBroadcast";
        case EOpSubgroupQuadSwapHorizontal: return "subgroupQuadSwapHorizontal";
        case EOpSubgroupQuadSwapVertical: return "subgroupQuadSwapVertical";
        case EOpSubgroupQuadSwapDiagonal: return "subgroupQuadSwapDiagonal";
        case EOpSubgroupPartition: return "subgroupPartitionNV";
        case EOpSubgroupPartitionedAdd: return "subgroupPartitionedAddNV";
        case EOpSubgroupPartitionedMul: return "subgroupPartitionedMulNV";
        case EOpSubgroupPartitionedMin: return "subgroupPartitionedMinNV";
        case EOpSubgroupPartitionedMax: return "subgroupPartitionedMaxNV";
        case EOpSubgroupPartitionedAnd: return "subgroupPartitionedAndNV";
        case EOpSubgroupPartitionedOr: return "subgroupPartitionedOrNV";
        case EOpSubgroupPartitionedXor: return "subgroupPartitionedXorNV";
        case EOpSubgroupPartitionedInclusiveAdd: return "subgroupPartitionedInclusiveAddNV";
        case EOpSubgroupPartitionedInclusiveMul: return "subgroupPartitionedInclusiveMulNV";
        case EOpSubgroupPartitionedInclusiveMin: return "subgroupPartitionedInclusiveMinNV";
        case EOpSubgroupPartitionedInclusiveMax: return "subgroupPartitionedInclusiveMaxNV";
        case EOpSubgroupPartitionedInclusiveAnd: return "subgroupPartitionedInclusiveAndNV";
        case EOpSubgroupPartitionedInclusiveOr: return "subgroupPartitionedInclusiveOrNV";
        case EOpSubgroupPartitionedInclusiveXor: return "subgroupPartitionedInclusiveXorNV";
        case EOpSubgroupPartitionedExclusiveAdd: return "subgroupPartitionedExclusiveAddNV";
        case EOpSubgroupPartitionedExclusiveMul: return "subgroupPartitionedExclusiveMulNV";
        case EOpSubgroupPartitionedExclusiveMin: return "subgroupPartitionedExclusiveMinNV";
        case EOpSubgroupPartitionedExclusiveMax: return "subgroupPartitionedExclusiveMaxNV";
        case EOpSubgroupPartitionedExclusiveAnd: return "subgroupPartitionedExclusiveAndNV";
        case EOpSubgroupPartitionedExclusiveOr: return "subgroupPartitionedExclusiveOrNV";
        case EOpSubgroupPartitionedExclusiveXor: return "subgroupPartitionedExclusiveXorNV";
        case EOpMinInvocations: return "minInvocationsAMD";
        case EOpMaxInvocations: return "maxInvocationsAMD";
        case EOpAddInvocations: return "addInvocationsAMD";
        case EOpMinInvocationsNonUniform: return "minInvocationsNonUniformAMD";
        case EOpMaxInvocationsNonUniform: return "maxInvocationsNonUniformAMD";
        case EOpAddInvocationsNonUniform: return "addInvocationsNonUniformAMD";
        case EOpMinInvocationsInclusiveScan: return "minInvocationsInclusiveScanAMD";
        case EOpMaxInvocationsInclusiveScan: return "maxInvocationsInclusiveScanAMD";
        case EOpAddInvocationsInclusiveScan: return "addInvocationsInclusiveScanAMD";
        case EOpMinInvocationsInclusiveScanNonUniform: return "minInvocationsInclusiveScanNonUniformAMD";
        case EOpMaxInvocationsInclusiveScanNonUniform: return "maxInvocationsInclusiveScanNonUniformAMD";
        case EOpAddInvocationsInclusiveScanNonUniform: return "addInvocationsInclusiveScanNonUniformAMD";
        case EOpMinInvocationsExclusiveScan: return "minInvocationsExclusiveScanAMD";
        case EOpMaxInvocationsExclusiveScan: return "maxInvocationsExclusiveScanAMD";
        case EOpAddInvocationsExclusiveScan: return "addInvocationsExclusiveScanAMD";
        case EOpMinInvocationsExclusiveScanNonUniform: return "minInvocationsExclusiveScanNonUniformAMD";
        case EOpMaxInvocationsExclusiveScanNonUniform: return "maxInvocationsExclusiveScanNonUniformAMD";
        case EOpAddInvocationsExclusiveScanNonUniform: return "addInvocationsExclusiveScanNonUniformAMD";
        case EOpSwizzleInvocations: return "swizzleInvocationsAMD";
        case EOpSwizzleInvocationsMasked: return "swizzleInvocationsMaskedAMD";
        case EOpWriteInvocation: return "writeInvocationAMD";
        case EOpMbcnt: return "mbcntAMD";
        case EOpCubeFaceIndex: return "cubeFaceIndexAMD";
        case EOpCubeFaceCoord: return "cubeFaceCoordAMD";
        case EOpTime: return "timeAMD";
        case EOpAtomicAdd: return "atomicAdd";
        case EOpAtomicMin: return "atomicMin";
        case EOpAtomicMax: return "atomicMax";
        case EOpAtomicAnd: return "atomicAnd";
        case EOpAtomicOr: return "atomicOr";
        case EOpAtomicXor: return "atomicXor";
        case EOpAtomicExchange: return "atomicExchange";
        case EOpAtomicCompSwap: return "atomicCompSwap";
        case EOpAtomicLoad: return "atomicLoad";
        case EOpAtomicStore: return "atomicStore";
        case EOpAtomicCounterIncrement: return "atomicCounterIncrement";
        case EOpAtomicCounterDecrement: return "atomicCounterDecrement";
        case EOpAtomicCounter: return "atomicCounter";
        case EOpAtomicCounterAdd: return version >= 460 ? "atomicCounterAdd" : "atomicCounterAddARB";
        case EOpAtomicCounterSubtract: return version >= 460 ? "atomicCounterSubtract" : "atomicCounterSubtractARB";
        case EOpAtomicCounterMin: return version >= 460 ? "atomicCounterMin" : "atomicCounterMinARB";
        case EOpAtomicCounterMax: return version >= 460 ? "atomicCounterMax" : "atomicCounterMaxARB";
        case EOpAtomicCounterAnd: return version >= 460 ? "atomicCounterAnd" : "atomicCounterAndARB";
        case EOpAtomicCounterOr: return version >= 460 ? "atomicCounterOr" : "atomicCounterOrARB";
        case EOpAtomicCounterXor: return version >= 460 ? "atomicCounterXor" : "atomicCounterXorARB";
        case EOpAtomicCounterExchange: return version >= 460 ? "atomicCounterExchange" : "atomicCounterExchangeARB";
        case EOpAtomicCounterCompSwap: return version >= 460 ? "atomicCounterCompSwap" : "atomicCounterCompSwapARB";
        case EOpAny: return "any";
        case EOpAll: return "all";
        case EOpCooperativeMatrixLoad: return "coopMatLoad";
        case EOpCooperativeMatrixStore: return "coopMatStore";
        case EOpCooperativeMatrixMulAdd: return "coopMatMulAdd";
        case EOpCooperativeMatrixLoadNV: return "coopMatLoadNV";
        case EOpCooperativeMatrixStoreNV: return "coopMatStoreNV";
        case EOpCooperativeMatrixMulAddNV: return "coopMatMulAddNV";
        case EOpBeginInvocationInterlock: return "beginInvocationInterlockARB";
        case EOpEndInvocationInterlock: return "endInvocationInterlockARB";
        case EOpIsHelperInvocation: return "helperInvocationEXT";
        case EOpDebugPrintf: return "debugPrintfEXT";
        case EOpConstructInt: return constructorFunctionNameForType("int", returnType);
        case EOpConstructUint: return constructorFunctionNameForType("uint", returnType);
        case EOpConstructInt8: return constructorFunctionNameForType("int8", returnType);
        case EOpConstructUint8: return constructorFunctionNameForType("uint8", returnType);
        case EOpConstructInt16: return constructorFunctionNameForType("int16", returnType);
        case EOpConstructUint16: return constructorFunctionNameForType("uint16", returnType);
        case EOpConstructInt64: return constructorFunctionNameForType("int64", returnType);
        case EOpConstructUint64: return constructorFunctionNameForType("uint64", returnType);
        case EOpConstructBool: return constructorFunctionNameForType("bool", returnType);
        case EOpConstructFloat: return constructorFunctionNameForType("float", returnType);
        case EOpConstructDouble: return constructorFunctionNameForType("double", returnType);
        case EOpConstructVec2: return constructorFunctionNameForType("vec2", returnType);
        case EOpConstructVec3: return constructorFunctionNameForType("vec3", returnType);
        case EOpConstructVec4: return constructorFunctionNameForType("vec4", returnType);
        case EOpConstructMat2x2: return constructorFunctionNameForType("mat2x2", returnType);
        case EOpConstructMat2x3: return constructorFunctionNameForType("mat2x3", returnType);
        case EOpConstructMat2x4: return constructorFunctionNameForType("mat2x4", returnType);
        case EOpConstructMat3x2: return constructorFunctionNameForType("mat3x2", returnType);
        case EOpConstructMat3x3: return constructorFunctionNameForType("mat3x3", returnType);
        case EOpConstructMat3x4: return constructorFunctionNameForType("mat3x4", returnType);
        case EOpConstructMat4x2: return constructorFunctionNameForType("mat4x2", returnType);
        case EOpConstructMat4x3: return constructorFunctionNameForType("mat4x3", returnType);
        case EOpConstructMat4x4: return constructorFunctionNameForType("mat4x4", returnType);
        case EOpConstructDVec2: return constructorFunctionNameForType("dvec2", returnType);
        case EOpConstructDVec3: return constructorFunctionNameForType("dvec3", returnType);
        case EOpConstructDVec4: return constructorFunctionNameForType("dvec4", returnType);
        case EOpConstructBVec2: return constructorFunctionNameForType("bvec2", returnType);
        case EOpConstructBVec3: return constructorFunctionNameForType("bvec3", returnType);
        case EOpConstructBVec4: return constructorFunctionNameForType("bvec4", returnType);
        case EOpConstructI8Vec2: return constructorFunctionNameForType("i8vec2", returnType);
        case EOpConstructI8Vec3: return constructorFunctionNameForType("i8vec3", returnType);
        case EOpConstructI8Vec4: return constructorFunctionNameForType("i8vec4", returnType);
        case EOpConstructU8Vec2: return constructorFunctionNameForType("u8vec2", returnType);
        case EOpConstructU8Vec3: return constructorFunctionNameForType("u8vec3", returnType);
        case EOpConstructU8Vec4: return constructorFunctionNameForType("u8vec4", returnType);
        case EOpConstructI16Vec2: return constructorFunctionNameForType("i16vec2", returnType);
        case EOpConstructI16Vec3: return constructorFunctionNameForType("i16vec3", returnType);
        case EOpConstructI16Vec4: return constructorFunctionNameForType("i16vec4", returnType);
        case EOpConstructU16Vec2: return constructorFunctionNameForType("u16vec2", returnType);
        case EOpConstructU16Vec3: return constructorFunctionNameForType("u16vec3", returnType);
        case EOpConstructU16Vec4: return constructorFunctionNameForType("u16vec4", returnType);
        case EOpConstructIVec2: return constructorFunctionNameForType("ivec2", returnType);
        case EOpConstructIVec3: return constructorFunctionNameForType("ivec3", returnType);
        case EOpConstructIVec4: return constructorFunctionNameForType("ivec4", returnType);
        case EOpConstructUVec2: return constructorFunctionNameForType("uvec2", returnType);
        case EOpConstructUVec3: return constructorFunctionNameForType("uvec3", returnType);
        case EOpConstructUVec4: return constructorFunctionNameForType("uvec4", returnType);
        case EOpConstructI64Vec2: return constructorFunctionNameForType("i64vec2", returnType);
        case EOpConstructI64Vec3: return constructorFunctionNameForType("i64vec3", returnType);
        case EOpConstructI64Vec4: return constructorFunctionNameForType("i64vec4", returnType);
        case EOpConstructU64Vec2: return constructorFunctionNameForType("u64vec2", returnType);
        case EOpConstructU64Vec3: return constructorFunctionNameForType("u64vec3", returnType);
        case EOpConstructU64Vec4: return constructorFunctionNameForType("u64vec4", returnType);
        case EOpConstructDMat2x2: return constructorFunctionNameForType("dmat2x2", returnType);
        case EOpConstructDMat2x3: return constructorFunctionNameForType("dmat2x3", returnType);
        case EOpConstructDMat2x4: return constructorFunctionNameForType("dmat2x4", returnType);
        case EOpConstructDMat3x2: return constructorFunctionNameForType("dmat3x2", returnType);
        case EOpConstructDMat3x3: return constructorFunctionNameForType("dmat3x3", returnType);
        case EOpConstructDMat3x4: return constructorFunctionNameForType("dmat3x4", returnType);
        case EOpConstructDMat4x2: return constructorFunctionNameForType("dmat4x2", returnType);
        case EOpConstructDMat4x3: return constructorFunctionNameForType("dmat4x3", returnType);
        case EOpConstructDMat4x4: return constructorFunctionNameForType("dmat4x4", returnType);
        case EOpConstructIMat2x2: return constructorFunctionNameForType("imat2x2", returnType);
        case EOpConstructIMat2x3: return constructorFunctionNameForType("imat2x3", returnType);
        case EOpConstructIMat2x4: return constructorFunctionNameForType("imat2x4", returnType);
        case EOpConstructIMat3x2: return constructorFunctionNameForType("imat3x2", returnType);
        case EOpConstructIMat3x3: return constructorFunctionNameForType("imat3x3", returnType);
        case EOpConstructIMat3x4: return constructorFunctionNameForType("imat3x4", returnType);
        case EOpConstructIMat4x2: return constructorFunctionNameForType("imat4x2", returnType);
        case EOpConstructIMat4x3: return constructorFunctionNameForType("imat4x3", returnType);
        case EOpConstructIMat4x4: return constructorFunctionNameForType("imat4x4", returnType);
        case EOpConstructUMat2x2: return constructorFunctionNameForType("umat2x2", returnType);
        case EOpConstructUMat2x3: return constructorFunctionNameForType("umat2x3", returnType);
        case EOpConstructUMat2x4: return constructorFunctionNameForType("umat2x4", returnType);
        case EOpConstructUMat3x2: return constructorFunctionNameForType("umat3x2", returnType);
        case EOpConstructUMat3x3: return constructorFunctionNameForType("umat3x3", returnType);
        case EOpConstructUMat3x4: return constructorFunctionNameForType("umat3x4", returnType);
        case EOpConstructUMat4x2: return constructorFunctionNameForType("umat4x2", returnType);
        case EOpConstructUMat4x3: return constructorFunctionNameForType("umat4x3", returnType);
        case EOpConstructUMat4x4: return constructorFunctionNameForType("umat4x4", returnType);
        case EOpConstructBMat2x2: return constructorFunctionNameForType("bmat2x2", returnType);
        case EOpConstructBMat2x3: return constructorFunctionNameForType("bmat2x3", returnType);
        case EOpConstructBMat2x4: return constructorFunctionNameForType("bmat2x4", returnType);
        case EOpConstructBMat3x2: return constructorFunctionNameForType("bmat3x2", returnType);
        case EOpConstructBMat3x3: return constructorFunctionNameForType("bmat3x3", returnType);
        case EOpConstructBMat3x4: return constructorFunctionNameForType("bmat3x4", returnType);
        case EOpConstructBMat4x2: return constructorFunctionNameForType("bmat4x2", returnType);
        case EOpConstructBMat4x3: return constructorFunctionNameForType("bmat4x3", returnType);
        case EOpConstructBMat4x4: return constructorFunctionNameForType("bmat4x4", returnType);
        case EOpConstructFloat16: return constructorFunctionNameForType("float16", returnType);
        case EOpConstructF16Vec2: return constructorFunctionNameForType("f16vec2", returnType);
        case EOpConstructF16Vec3: return constructorFunctionNameForType("f16vec3", returnType);
        case EOpConstructF16Vec4: return constructorFunctionNameForType("f16vec4", returnType);
        case EOpConstructF16Mat2x2: return constructorFunctionNameForType("f16mat2x2", returnType);
        case EOpConstructF16Mat2x3: return constructorFunctionNameForType("f16mat2x3", returnType);
        case EOpConstructF16Mat2x4: return constructorFunctionNameForType("f16mat2x4", returnType);
        case EOpConstructF16Mat3x2: return constructorFunctionNameForType("f16mat3x2", returnType);
        case EOpConstructF16Mat3x3: return constructorFunctionNameForType("f16mat3x3", returnType);
        case EOpConstructF16Mat3x4: return constructorFunctionNameForType("f16mat3x4", returnType);
        case EOpConstructF16Mat4x2: return constructorFunctionNameForType("f16mat4x2", returnType);
        case EOpConstructF16Mat4x3: return constructorFunctionNameForType("f16mat4x3", returnType);
        case EOpConstructF16Mat4x4: return constructorFunctionNameForType("f16mat4x4", returnType);
        case EOpConstructStruct: return RValueOperator::ConstructStruct;
        case EOpConstructTextureSampler: return "textureSampler";
        case EOpConstructNonuniform: return "nonuniform";
        case EOpConstructReference: return "reference";
        case EOpConstructCooperativeMatrixNV: return "cooperativeMatrixNV";
        case EOpConstructCooperativeMatrixKHR: return "cooperativeMatrixKHR";
        case EOpAssign: return RValueOperator::Assign;
        case EOpAddAssign: return RValueOperator::AddAssign;
        case EOpSubAssign: return RValueOperator::SubAssign;
        case EOpMulAssign:
        case EOpVectorTimesMatrixAssign:
        case EOpVectorTimesScalarAssign:
        case EOpMatrixTimesScalarAssign:
        case EOpMatrixTimesMatrixAssign:
            return RValueOperator::MulAssign;
        case EOpDivAssign: return RValueOperator::DivAssign;
        case EOpModAssign: return RValueOperator::ModAssign;
        case EOpAndAssign: return RValueOperator::AndAssign;
        case EOpInclusiveOrAssign: return RValueOperator::InclusiveOrAssign;
        case EOpExclusiveOrAssign: return RValueOperator::ExclusiveOrAssign;
        case EOpLeftShiftAssign: return RValueOperator::LeftShiftAssign;
        case EOpRightShiftAssign: return RValueOperator::RightShiftAssign;
        case EOpArrayLength: return RValueOperator::ArrayLength;
        case EOpImageQuerySize: return "imageSize";
        case EOpImageQuerySamples: return "imageSamples";
        case EOpImageLoad: return "imageLoad";
        case EOpImageStore: return "imageStore";
        case EOpImageLoadLod: return "imageLoadLodAMD";
        case EOpImageStoreLod: return "imageStoreLodAMD";
        case EOpImageAtomicAdd: return "imageAtomicAdd";
        case EOpImageAtomicMin: return "imageAtomicMin";
        case EOpImageAtomicMax: return "imageAtomicMax";
        case EOpImageAtomicAnd: return "imageAtomicAnd";
        case EOpImageAtomicOr: return "imageAtomicOr";
        case EOpImageAtomicXor: return "imageAtomicXor";
        case EOpImageAtomicExchange: return "imageAtomicExchange";
        case EOpImageAtomicCompSwap: return "imageAtomicCompSwap";
        case EOpImageAtomicLoad: return "imageAtomicLoad";
        case EOpImageAtomicStore: return "imageAtomicStore";
        case EOpSubpassLoad: return "subpassLoad";
        case EOpSubpassLoadMS: return "subpassLoadMS";
        case EOpSparseImageLoad: return "sparseImageLoadARB";
        case EOpSparseImageLoadLod: return "sparseImageLoadLodAMD";
        case EOpColorAttachmentReadEXT: return "colorAttachmentReadEXT";
        case EOpTextureQuerySize: return "textureSize";
        case EOpTextureQueryLod: return version >= 400 ? "textureQueryLod" : "textureQueryLOD";
        case EOpTextureQueryLevels: return "textureQueryLevels";
        case EOpTextureQuerySamples: return "textureSamples";
        case EOpTexture: return textureFunctionNameForSampler("", "", version, arg1Type);
        case EOpTextureProj: return textureFunctionNameForSampler("Proj", "", version, arg1Type);
        case EOpTextureLod: return textureFunctionNameForSampler("Lod", "", version, arg1Type);
        case EOpTextureOffset: return "textureOffset";
        case EOpTextureFetch: return "texelFetch";
        case EOpTextureFetchOffset: return "texelFetchOffset";
        case EOpTextureProjOffset: return "textureProjOffset";
        case EOpTextureLodOffset: return "textureLodOffset";
        case EOpTextureProjLod: return textureFunctionNameForSampler("ProjLod", "", version, arg1Type);
        case EOpTextureProjLodOffset: return "textureProjLodOffset";
        case EOpTextureGrad: return textureFunctionNameForSampler("Grad", "ARB", version, arg1Type);
        case EOpTextureGradOffset: return "textureGradOffset";
        case EOpTextureProjGrad: return textureFunctionNameForSampler("ProjGrad", "ARB", version, arg1Type);
        case EOpTextureProjGradOffset: return "textureProjGradOffset";
        case EOpTextureGather: return "textureGather";
        case EOpTextureGatherOffset: return "textureGatherOffset";
        case EOpTextureGatherOffsets: return "textureGatherOffsets";
        case EOpTextureClamp: return "textureClampARB";
        case EOpTextureOffsetClamp: return "textureOffsetClampARB";
        case EOpTextureGradClamp: return "textureGradClampARB";
        case EOpTextureGradOffsetClamp: return "textureGradOffsetClampARB";
        case EOpTextureGatherLod: return "textureGatherLodAMD";
        case EOpTextureGatherLodOffset: return "textureGatherLodOffsetAMD";
        case EOpTextureGatherLodOffsets: return "textureGatherLodOffsetsAMD";
        case EOpFragmentMaskFetch: return "fragmentMaskFetchAMD";
        case EOpFragmentFetch: return "fragmentFetchAMD";
        case EOpSparseTexture: return "sparseTextureARB";
        case EOpSparseTextureLod: return "sparseTextureLodARB";
        case EOpSparseTextureOffset: return "sparseTextureOffsetARB";
        case EOpSparseTextureFetch: return "sparseTexelFetchARB";
        case EOpSparseTextureFetchOffset: return "sparseTexelFetchOffsetARB";
        case EOpSparseTextureLodOffset: return "sparseTextureLodOffsetARB";
        case EOpSparseTextureGrad: return "sparseTextureGradARB";
        case EOpSparseTextureGradOffset: return "sparseTextureGradOffsetARB";
        case EOpSparseTextureGather: return "sparseTextureGatherARB";
        case EOpSparseTextureGatherOffset: return "sparseTextureGatherOffsetARB";
        case EOpSparseTextureGatherOffsets: return "sparseTextureGatherOffsetsARB";
        case EOpSparseTexelsResident: return "sparseTexelsResidentARB";
        case EOpSparseTextureClamp: return "sparseTextureClampARB";
        case EOpSparseTextureOffsetClamp: return "sparseTextureOffsetClampARB";
        case EOpSparseTextureGradClamp: return "sparseTextureGradClampARB";
        case EOpSparseTextureGradOffsetClamp: return "sparseTextureGradOffsetClampARB";
        case EOpSparseTextureGatherLod: return "sparseTextureGatherLodAMD";
        case EOpSparseTextureGatherLodOffset: return "sparseTextureGatherLodOffsetAMD";
        case EOpSparseTextureGatherLodOffsets: return "sparseTextureGatherLodOffsetsAMD";
        case EOpImageSampleFootprintNV: return "textureFootprintNV";
        case EOpImageSampleFootprintClampNV: return "textureFootprintClampNV";
        case EOpImageSampleFootprintLodNV: return "textureFootprintLodNV";
        case EOpImageSampleFootprintGradNV: return "textureFootprintGradNV";
        case EOpImageSampleFootprintGradClampNV: return "textureFootprintGradClampNV";
        case EOpAddCarry: return "uaddCarry";
        case EOpSubBorrow: return "usubBorrow";
        case EOpUMulExtended: return "umulExtended";
        case EOpIMulExtended: return "imulExtended";
        case EOpBitfieldExtract: return "bitfieldExtract";
        case EOpBitfieldInsert: return "bitfieldInsert";
        case EOpBitFieldReverse: return "bitfieldReverse";
        case EOpBitCount: return "bitCount";
        case EOpFindLSB: return "findLSB";
        case EOpFindMSB: return "findMSB";
        case EOpCountLeadingZeros: return "countLeadingZeros";
        case EOpCountTrailingZeros: return "countTrailingZeros";
        case EOpAbsDifference: return "absoluteDifference";
        case EOpAddSaturate: return "addSaturate";
        case EOpSubSaturate: return "subtractSaturate";
        case EOpAverage: return "average";
        case EOpAverageRounded: return "averageRounded";
        case EOpMul32x16: return "multiply32x16";
        case EOpTraceNV: return "traceNV";
        case EOpTraceRayMotionNV: return "traceRayMotionNV";
        case EOpTraceKHR: return "traceRayEXT";
        case EOpReportIntersection: return "reportIntersectionEXT";
        case EOpIgnoreIntersectionNV: return "ignoreIntersectionNV";
        case EOpTerminateRayNV: return "terminateRayNV";
        case EOpExecuteCallableNV: return "executeCallableNV";
        case EOpExecuteCallableKHR: return "executeCallableEXT";
        case EOpWritePackedPrimitiveIndices4x8NV: return "writePackedPrimitiveIndices4x8NV";
        case EOpEmitMeshTasksEXT: return "EmitMeshTasksEXT";
        case EOpSetMeshOutputsEXT: return "SetMeshOutputsEXT";
        case EOpRayQueryInitialize: return "rayQueryInitializeEXT";
        case EOpRayQueryTerminate: return "rayQueryTerminateEXT";
        case EOpRayQueryGenerateIntersection: return "rayQueryGenerateIntersectionEXT";
        case EOpRayQueryConfirmIntersection: return "rayQueryConfirmIntersectionEXT";
        case EOpRayQueryProceed: return "rayQueryProceedEXT";
        case EOpRayQueryGetIntersectionType: return "rayQueryGetIntersectionTypeEXT";
        case EOpRayQueryGetRayTMin: return "rayQueryGetRayTMinEXT";
        case EOpRayQueryGetRayFlags: return "rayQueryGetRayFlagsEXT";
        case EOpRayQueryGetIntersectionT: return "rayQueryGetIntersectionTEXT";
        case EOpRayQueryGetIntersectionInstanceCustomIndex: return "rayQueryGetIntersectionInstanceCustomIndexEXT";
        case EOpRayQueryGetIntersectionInstanceId: return "rayQueryGetIntersectionInstanceIdEXT";
        case EOpRayQueryGetIntersectionInstanceShaderBindingTableRecordOffset: return "rayQueryGetIntersectionInstanceShaderBindingTableRecordOffsetEXT";
        case EOpRayQueryGetIntersectionGeometryIndex: return "rayQueryGetIntersectionGeometryIndexEXT";
        case EOpRayQueryGetIntersectionPrimitiveIndex: return "rayQueryGetIntersectionPrimitiveIndexEXT";
        case EOpRayQueryGetIntersectionBarycentrics: return "rayQueryGetIntersectionBarycentricsEXT";
        case EOpRayQueryGetIntersectionFrontFace: return "rayQueryGetIntersectionFrontFaceEXT";
        case EOpRayQueryGetIntersectionCandidateAABBOpaque: return "rayQueryGetIntersectionCandidateAABBOpaqueEXT";
        case EOpRayQueryGetIntersectionObjectRayDirection: return "rayQueryGetIntersectionObjectRayDirectionEXT";
        case EOpRayQueryGetIntersectionObjectRayOrigin: return "rayQueryGetIntersectionObjectRayOriginEXT";
        case EOpRayQueryGetWorldRayDirection: return "rayQueryGetWorldRayDirectionEXT";
        case EOpRayQueryGetWorldRayOrigin: return "rayQueryGetWorldRayOriginEXT";
        case EOpRayQueryGetIntersectionObjectToWorld: return "rayQueryGetIntersectionObjectToWorldEXT";
        case EOpRayQueryGetIntersectionWorldToObject: return "rayQueryGetIntersectionWorldToObjectEXT";
        case EOpHitObjectTraceRayNV: return "hitObjectTraceRayNV";
        case EOpHitObjectTraceRayMotionNV: return "hitObjectTraceRayMotionNV";
        case EOpHitObjectRecordHitNV: return "hitObjectRecordHitNV";
        case EOpHitObjectRecordHitMotionNV: return "hitObjectRecordHitMotionNV";
        case EOpHitObjectRecordHitWithIndexNV: return "hitObjectRecordHitWithIndexNV";
        case EOpHitObjectRecordHitWithIndexMotionNV: return "hitObjectRecordHitWithIndexMotionNV";
        case EOpHitObjectRecordMissNV: return "hitObjectRecordMissNV";
        case EOpHitObjectRecordMissMotionNV: return "hitObjectRecordMissMotionNV";
        case EOpHitObjectRecordEmptyNV: return "hitObjectRecordEmptyNV";
        case EOpHitObjectExecuteShaderNV: return "hitObjectExecuteShaderNV";
        case EOpHitObjectIsEmptyNV: return "hitObjectIsEmptyNV";
        case EOpHitObjectIsMissNV: return "hitObjectIsMissNV";
        case EOpHitObjectIsHitNV: return "hitObjectIsHitNV";
        case EOpHitObjectGetRayTMinNV: return "hitObjectGetRayTMinNV";
        case EOpHitObjectGetRayTMaxNV: return "hitObjectGetRayTMaxNV";
        case EOpHitObjectGetObjectRayOriginNV: return "hitObjectGetObjectRayOriginNV";
        case EOpHitObjectGetObjectRayDirectionNV: return "hitObjectGetObjectRayDirectionNV";
        case EOpHitObjectGetWorldRayOriginNV: return "hitObjectGetWorldRayOriginNV";
        case EOpHitObjectGetWorldRayDirectionNV: return "hitObjectGetWorldRayDirectionNV";
        case EOpHitObjectGetWorldToObjectNV: return "hitObjectGetWorldToObjectNV";
        case EOpHitObjectGetObjectToWorldNV: return "hitObjectGetObjectToWorldNV";
        case EOpHitObjectGetInstanceCustomIndexNV: return "hitObjectGetInstanceCustomIndexNV";
        case EOpHitObjectGetInstanceIdNV: return "hitObjectGetInstanceIdNV";
        case EOpHitObjectGetGeometryIndexNV: return "hitObjectGetGeometryIndexNV";
        case EOpHitObjectGetPrimitiveIndexNV: return "hitObjectGetPrimitiveIndexNV";
        case EOpHitObjectGetHitKindNV: return "hitObjectGetHitKindNV";
        case EOpHitObjectGetShaderBindingTableRecordIndexNV: return "hitObjectGetShaderBindingTableRecordIndexNV";
        case EOpHitObjectGetShaderRecordBufferHandleNV: return "hitObjectGetShaderRecordBufferHandleNV";
        case EOpHitObjectGetAttributesNV: return "hitObjectGetAttributesNV";
        case EOpHitObjectGetCurrentTimeNV: return "hitObjectGetCurrentTimeNV";
        case EOpReorderThreadNV: return "reorderThreadNV";
        case EOpFetchMicroTriangleVertexPositionNV: return "fetchMicroTriangleVertexPositionNV";
        case EOpFetchMicroTriangleVertexBarycentricNV: return "fetchMicroTriangleVertexBarycentricNV";
        case EOpReadClockSubgroupKHR:  return "clock2x32ARB"; // clockARB unsupported
        case EOpReadClockDeviceKHR: return "clockRealtime2x32EXT"; // clockRealtimeEXT unsupported
        case EOpRayQueryGetIntersectionTriangleVertexPositionsEXT: return "rayQueryGetIntersectionTriangleVertexPositionsEXT";
        case EOpStencilAttachmentReadEXT: return "stencilAttachmentReadEXT";
        case EOpDepthAttachmentReadEXT: return "depthAttachmentReadEXT";
        case EOpImageSampleWeightedQCOM: return "textureWeightedQCOM";
        case EOpImageBoxFilterQCOM: return "textureBoxFilterQCOM";
        case EOpImageBlockMatchSADQCOM: return "textureBlockMatchSADQCOM";
        case EOpImageBlockMatchSSDQCOM: return "textureBlockMatchSSDQCOM";
        default:
            utils::slog.e << "Cannot convert operator " << glslangOperatorToString(op) << " to RValue operator.";
            // TODO: abort here
            return RValueOperator::Ternary;
    }
}

BranchOperator glslangOperatorToBranchOperator(glslang::TOperator op) {
    using namespace glslang;
    switch (op) {
        case EOpKill: return BranchOperator::Discard;
        case EOpTerminateInvocation: return BranchOperator::TerminateInvocation;
        case EOpDemote: return BranchOperator::Demote;
        case EOpTerminateRayKHR: return BranchOperator::TerminateRayEXT;
        case EOpIgnoreIntersectionKHR: return BranchOperator::IgnoreIntersectionEXT;
        case EOpReturn: return BranchOperator::Return;
        case EOpBreak: return BranchOperator::Break;
        case EOpContinue: return BranchOperator::Continue;
        case EOpCase: return BranchOperator::Case;
        case EOpDefault: return BranchOperator::Default;
        default:
            PANIC_PRECONDITION("Cannot convert operator %s to BranchOperator",
                    glslangOperatorToString(op));
    }
}

std::string_view expandTypeNameToVector(
        const char* const* typeNames, int vectorSize) {
    ASSERT_PRECONDITION(vectorSize >= 1 && vectorSize <= 4,
            "vectorSize must be between 1 and 4");
    return typeNames[vectorSize - 1];
}

std::string_view expandTypeNameToVectorOrMatrix(
        const char* const* typeNames, bool isMatrix,
        int vectorSize, int matrixCols, int matrixRows) {
    if (isMatrix) {
        ASSERT_PRECONDITION(matrixCols >= 2 && matrixCols <= 4,
                "matrixCols must be between 2 and 4");
        ASSERT_PRECONDITION(matrixRows >= 2 && matrixRows <= 4,
                "matrixRows must be between 2 and 4");
        return typeNames[
                4 // 4 to skip vector
                + (matrixCols - 2) * 3
                + (matrixRows - 2)
                ];
    }
    return expandTypeNameToVector(typeNames, vectorSize);
}

LiteralRValue constUnionToLiteralRValue(const glslang::TConstUnion& value) {
    switch (value.getType()) {
        case glslang::EbtInt8: return LiteralRValue{value.getI8Const()};
        case glslang::EbtUint8: return LiteralRValue{value.getU8Const()};
        case glslang::EbtInt16: return LiteralRValue{value.getI16Const()};
        case glslang::EbtUint16: return LiteralRValue{value.getU16Const()};
        case glslang::EbtInt: return LiteralRValue{value.getIConst()};
        case glslang::EbtUint: return LiteralRValue{value.getUConst()};
        case glslang::EbtInt64: PANIC_PRECONDITION("Unsupported type: Int64");
        case glslang::EbtUint64: PANIC_PRECONDITION("Unsupported type: Uint64");
        case glslang::EbtDouble: return LiteralRValue{value.getDConst()};
        case glslang::EbtBool: return LiteralRValue{value.getBConst()};
        case glslang::EbtString: PANIC_PRECONDITION("Unsupported type: String");
        default: PANIC_PRECONDITION("Unsupported type: %d", value.getType());
    }
}

template<typename Id, typename Value>
class IdStoreByValue {
public:
    // Inserts if non-existent.
    Id insert(Value value) {
        auto it = mMap.find(value);
        if (it == mMap.end()) {
            Id id = Id {++mLastId};
            mMap[value] = id;
            return id;
        }
        return it->second;
    }

    std::unordered_map<Id, Value> getFinal() {
        std::unordered_map<Id, Value> r;
        for (const auto& pair : mMap) {
            r[pair.second] = pair.first;
        }
        return r;
    }

private:
    int mLastId = 0;
    std::unordered_map<Value, Id> mMap;
};

template<typename Id, typename Value, typename Key>
class IdStoreByKey {
public:
    // Inserts if non-existent.
    Id insert(Key key, Value value) {
        auto it = mMap.find(key);
        if (it == mMap.end()) {
            Id id = Id {++mLastId};
            mMap[key] = std::pair(id, value);
            return id;
        }
        return it->second.first;
    }

    // Gets if extant.
    std::optional<Id> get(Key key) {
        auto it = mMap.find(key);
        if (it == mMap.end()) {
            return std::nullopt;
        }
        return it->second.first;
    }

    std::unordered_map<Id, Value> getFinal() {
        std::unordered_map<Id, Value> r;
        for (const auto& pair : mMap) {
            r[pair.second.first] = pair.second.second;
        }
        return r;
    }

    bool empty() const {
        return mMap.empty();
    }

private:
    int mLastId = 0;
    std::unordered_map<Key, std::pair<Id, Value>> mMap;
};

using LocalSymbols = IdStoreByKey<LocalSymbolId, Symbol, long long>;

class Slurper {
public:
    Slurper(const glslang::TIntermediate& intermediate) {
        mVersion = intermediate.getVersion();
        slurpFromRoot(intermediate.getTreeRoot()->getAsAggregate());
    }

    PackFromGlsl intoPack() {
        return PackFromGlsl {
                mVersion,
                mStrings.getFinal(),
                mTypes.getFinal(),
                mGlobalSymbols.getFinal(),
                mRValues.getFinal(),
                mFunctionNames.getFinal(),
                mStatementBlocks.getFinal(),
                std::move(mFunctionDefinitions),
                std::move(mFunctionPrototypes),
                std::move(mGlobalSymbolDefinitionsInOrder),
                std::move(mFunctionDefinitionsInOrder),
        };
    }

private:
    int mVersion;
    IdStoreByValue<StringId, std::string> mStrings;
    IdStoreByValue<TypeId, Type> mTypes;
    IdStoreByKey<StructId, Symbol, long long> mStructs;
    IdStoreByKey<GlobalSymbolId, Symbol, long long> mGlobalSymbols;
    IdStoreByValue<RValueId, RValue> mRValues;
    IdStoreByValue<FunctionId, std::string> mFunctionNames;
    IdStoreByValue<StatementBlockId, std::vector<Statement>> mStatementBlocks;
    std::unordered_map<FunctionId, FunctionDefinition> mFunctionDefinitions;
    std::set<FunctionId> mFunctionPrototypes;
    std::vector<std::tuple<GlobalSymbolId, ValueId>> mGlobalSymbolDefinitionsInOrder;
    std::vector<FunctionId> mFunctionDefinitionsInOrder;

    void slurpFromRoot(glslang::TIntermAggregate* node) {
        ASSERT_PRECONDITION(node != nullptr, "Node must not be null");
        ASSERT_PRECONDITION(node->getOp() == glslang::EOpSequence, "Node must be a sequence");

        std::vector<glslang::TIntermAggregate*> linkerObjectNodes;
        std::vector<glslang::TIntermAggregate*> sequenceNodes;
        std::vector<glslang::TIntermAggregate*> functionNodes;

        // Sort children into categories to be processed in order.
        for (TIntermNode* child : node->getSequence()) {
            if (auto childAsAggregate = child->getAsAggregate()) {
                switch (childAsAggregate->getOp()) {
                    case glslang::EOpLinkerObjects:
                        linkerObjectNodes.push_back(childAsAggregate);
                        continue;
                    case glslang::EOpSequence:
                        sequenceNodes.push_back(childAsAggregate);
                        continue;
                    case glslang::EOpFunction:
                        functionNodes.push_back(childAsAggregate);
                        continue;
                    default:
                        // Fall through.
                        break;
                }
            }
            PANIC_PRECONDITION("Unhandled child of root node: %s, parent = %s",
                    glslangNodeToStringWithLoc(child).c_str(),
                    glslangNodeToStringWithLoc(node).c_str());
        }

        // Linker objects contain a list of global symbols.
        for (auto linkerObject : linkerObjectNodes) {
            for (auto child : linkerObject->getSequence()) {
                auto childAsSymbol = child->getAsSymbolNode();
                ASSERT_PRECONDITION(childAsSymbol,
                        "Unhandled child of LinkerObjects node: %s, parent = %s",
                        glslangNodeToStringWithLoc(child).c_str(),
                        glslangNodeToStringWithLoc(linkerObject).c_str());
                slurpGlobalSymbol(childAsSymbol);
            }
        }
        // Sequence nodes contain assignment operations.
        LocalSymbols emptyLocalSymbols;
        for (auto sequence : sequenceNodes) {
            for (auto child : sequence->getSequence()) {
                auto childAsBinary = child->getAsBinaryNode();
                ASSERT_PRECONDITION(childAsBinary && childAsBinary->getOp() == glslang::EOpAssign,
                        "Unhandled child of LinkerObjects node: %s, parent = %s",
                        glslangNodeToStringWithLoc(child).c_str(),
                        glslangNodeToStringWithLoc(sequence).c_str());
                auto leftAsSymbol = childAsBinary->getLeft()->getAsSymbolNode();
                ASSERT_PRECONDITION(leftAsSymbol,
                        "Left-hand side of global variable definition must be symbol: "
                        "%s, parent = %s",
                        glslangNodeToStringWithLoc(child).c_str(),
                        glslangNodeToStringWithLoc(sequence).c_str());
                auto globalSymbolId = slurpGlobalSymbol(leftAsSymbol);
                auto initialValue = slurpValue(
                        childAsBinary->getRight(), sequence, emptyLocalSymbols);
                ASSERT_PRECONDITION(emptyLocalSymbols.empty(),
                        "Global symbol definition must not touch local symbols: "
                        "%s, parent = %s",
                        glslangNodeToStringWithLoc(child).c_str(),
                        glslangNodeToStringWithLoc(sequence).c_str());
                mGlobalSymbolDefinitionsInOrder.push_back({globalSymbolId, initialValue});
            }
        }
        // Function definitions are the meat of the AST.
        for (auto child : functionNodes) {
            slurpFunctionDefinition(child, node);
        }
    }

    GlobalSymbolId slurpGlobalSymbol(const glslang::TIntermSymbol* node) {
        auto typeId = slurpType(node->getType());
        auto nameId = mStrings.insert(std::string(node->getAccessName()));
        return mGlobalSymbols.insert(node->getId(), Symbol{nameId, typeId});
    }

    // StructId slurpStruct(const glslang::TTypeList* typeList) {
    // }

    std::optional<StringId> slurpQualifiers(const glslang::TQualifier& qualifier) {
        using namespace glslang;
        std::string s;
        if (qualifier.invariant) {
            s += "invariant ";
        }
        if (qualifier.flat) {
            s += "flat ";
        }
        if (qualifier.nopersp) {
            s += "noperspective ";
        }
        if (qualifier.smooth) {
            s += "smooth ";
        }
        if (qualifier.hasLayout()) {
            s += "layout(";
            // TODO
            s += ") ";
        }
        if (qualifier.isConstant()) {
            s += "const ";
        }
        switch (qualifier.precision) {
            case EpqLow: s += "lowp "; break;
            case EpqMedium: s += "mediump "; break;
            case EpqHigh: s += "highp "; break;
            case EpqNone:
            default:
                break;
        }
        if (!s.empty()) {
            return mStrings.insert(std::move(s));
        }
        return std::nullopt;
    }

    TypeId slurpType(const glslang::TType& type) {
        using namespace glslang;

        static const char* const FLOAT_TYPE_NAMES[] = {
                "float",
                "vec2",
                "vec3",
                "vec4",
                "mat2",
                "mat2x3",
                "mat2x4",
                "mat3x2",
                "mat3",
                "mat3x4",
                "mat4x2",
                "mat4x3",
                "mat4",
        };

        static const char* const DOUBLE_TYPE_NAMES[] = {
                "double",
                "dvec2",
                "dvec3",
                "dvec4",
                "dmat2",
                "dmat2x3",
                "dmat2x4",
                "dmat3x2",
                "dmat3",
                "dmat3x4",
                "dmat4x2",
                "dmat4x3",
                "dmat4",
        };

        static const char* const INT_TYPE_NAMES[] = {
                "int",
                "ivec2",
                "ivec3",
                "ivec4",
        };

        static const char* const UINT_TYPE_NAMES[] = {
                "uint",
                "uvec2",
                "uvec3",
                "uvec4",
        };

        static const char* const BOOL_TYPE_NAMES[] = {
                "bool",
                "bvec2",
                "bvec3",
                "bvec4",
        };

        auto typeArraySizes = type.getArraySizes();
        std::vector<std::size_t> arraySizes(typeArraySizes ? typeArraySizes->getNumDims() : 0);
        for (int i = 0; i < arraySizes.size(); ++i) {
            arraySizes[i] = typeArraySizes->getDimSize(i);
        }

        std::string_view typeName;
        switch (type.getBasicType()) {
            case EbtVoid:
                typeName = "void";
                break;
            case EbtFloat:
                typeName = expandTypeNameToVectorOrMatrix(
                        FLOAT_TYPE_NAMES,
                        type.isMatrix(),
                        type.getVectorSize(),
                        type.getMatrixCols(),
                        type.getMatrixRows());
                break;
            case EbtDouble:
                typeName = expandTypeNameToVectorOrMatrix(
                        DOUBLE_TYPE_NAMES,
                        type.isMatrix(),
                        type.getVectorSize(),
                        type.getMatrixCols(),
                        type.getMatrixRows());
                break;
            case EbtInt:
                typeName = expandTypeNameToVector(
                        INT_TYPE_NAMES,
                        type.getVectorSize());
                break;
            case EbtUint:
                typeName = expandTypeNameToVector(
                        UINT_TYPE_NAMES,
                        type.getVectorSize());
                break;
            case EbtBool:
                typeName = expandTypeNameToVector(
                        BOOL_TYPE_NAMES,
                        type.getVectorSize());
                break;
            case EbtAtomicUint:
                typeName = "atomic_uint";
                break;
            case EbtSampler:
                typeName = type.getSampler().getString();
                break;
            case EbtStruct:
            case EbtBlock:
                typeName = type.getTypeName();
                break;
            default:
                PANIC_PRECONDITION("Cannot convert glslang type `%s' to Type",
                        type.getCompleteString().c_str());
        };
        auto nameId = mStrings.insert(std::string(typeName));
        auto qualifiersId = slurpQualifiers(type.getQualifier());
        return mTypes.insert(Type{nameId, qualifiersId, std::move(arraySizes)});
    }

    StatementBlockId slurpStatementBlock(
            TIntermNode* node, TIntermNode* parent, LocalSymbols& localSymbols) {
        std::vector<Statement> statements;
        auto nodeAsAggregate = node->getAsAggregate();
        if (nodeAsAggregate != nullptr && nodeAsAggregate->getOp() == glslang::EOpSequence) {
            // Read all children into this statement block.
            for (TIntermNode* child : nodeAsAggregate->getSequence()) {
                nodeToStatements(child, node, localSymbols, statements);
            }
        } else {
            // Wrap whatever this is into a new statement block.
            nodeToStatements(node, parent, localSymbols, statements);
        }
        return mStatementBlocks.insert(statements);
    }

    void slurpFunctionDefinition(
            glslang::TIntermAggregate* node, TIntermNode* parent) {
        ASSERT_PRECONDITION(node->getOp() == glslang::EOpFunction,
                "Node must be a function");
        auto& sequence = node->getSequence();
        ASSERT_PRECONDITION(sequence.size() == 1 || sequence.size() == 2,
                "Sequence must be of length 1 or 2");
        auto parametersNode = sequence[0]->getAsAggregate();
        ASSERT_PRECONDITION(parametersNode != nullptr,
                "Function parameters must be an aggregate node");

        auto functionId = mFunctionNames.insert(std::string(node->getName()));

        if (sequence.size() == 1) {
            // This is just a prototype. Make a record of it.
            mFunctionPrototypes.insert(functionId);
            return;
        }

        auto returnTypeId = slurpType(node->getType());

        LocalSymbols localSymbols;
        std::vector<LocalSymbolId> parameters;
        for (const auto parameter : parametersNode->getSequence()) {
            auto parameterAsSymbol = parameter->getAsSymbolNode();
            ASSERT_PRECONDITION(parameterAsSymbol != nullptr,
                    "Function parameter must be symbol: %s, definition = %s, parent = %s",
                    glslangNodeToStringWithLoc(parameter).c_str(),
                    glslangNodeToStringWithLoc(node).c_str(),
                    glslangNodeToStringWithLoc(parent).c_str());
            auto nameId = mStrings.insert(std::string(parameterAsSymbol->getName()));
            auto typeId = slurpType(parameterAsSymbol->getType());
            auto symbolId = localSymbols.insert(parameterAsSymbol->getId(), Symbol{nameId, typeId});
            parameters.push_back(symbolId);
        }

        auto bodyId = slurpStatementBlock(sequence[1], node, localSymbols);
        mFunctionDefinitions[functionId] = FunctionDefinition{
                functionId, returnTypeId, std::move(parameters), bodyId,
                localSymbols.getFinal()};
        mFunctionDefinitionsInOrder.push_back(functionId);
    }

    // Turn a non-root node into one or more statements.
    void nodeToStatements(TIntermNode* node, TIntermNode* parent, LocalSymbols& localSymbols,
            std::vector<Statement> &output) {
        if (auto nodeAsLoopNode = node->getAsLoopNode()) {
            auto conditionId = slurpValue(nodeAsLoopNode->getTest(), parent, localSymbols);
            std::optional<RValueId> terminalId;
            if (auto terminal = nodeAsLoopNode->getTerminal()) {
                // Ignore random stray symbols and literals for the terminal since they don't do
                // anything.
                if (!terminal->getAsSymbolNode() && !terminal->getAsConstantUnion()) {
                    auto terminalIdAsValueId = slurpValue(
                            nodeAsLoopNode->getTerminal(), parent, localSymbols);
                    if (auto* terminalIdAsRValueId = std::get_if<RValueId>(&terminalIdAsValueId)) {
                        terminalId = *terminalIdAsRValueId;
                    } else {
                        PANIC_PRECONDITION("Encountered non-RValue in Loop terminal: %s, parent = %s",
                                glslangNodeToStringWithLoc(terminal).c_str(),
                                glslangNodeToStringWithLoc(node).c_str());
                    }
                }
            }
            bool testFirst = nodeAsLoopNode->testFirst();
            auto bodyId = slurpStatementBlock(nodeAsLoopNode->getBody(), parent, localSymbols);
            output.push_back(LoopStatement{conditionId, terminalId, testFirst, bodyId});
            return;
        }
        if (auto nodeAsBranchNode = node->getAsBranchNode()) {
            auto op = glslangOperatorToBranchOperator(nodeAsBranchNode->getFlowOp());
            std::optional<ValueId> operandId;
            if (auto operand = nodeAsBranchNode->getExpression()) {
                operandId = slurpValue(operand, node, localSymbols);
            }
            output.push_back(BranchStatement{op, operandId});
            return;
        }
        if (auto nodeAsSwitchNode = node->getAsSwitchNode()) {
            if (auto conditionAsTyped = nodeAsSwitchNode->getCondition()->getAsTyped()) {
                auto conditionId = slurpValue(conditionAsTyped, parent, localSymbols);
                auto bodyId = slurpStatementBlock(
                        nodeAsSwitchNode->getBody(), parent, localSymbols);
                output.push_back(SwitchStatement{conditionId, bodyId});
            } else {
                PANIC_PRECONDITION("Switch node condition was not typed: %s, parent = %s",
                        glslangNodeToStringWithLoc(nodeAsSwitchNode->getCondition()).c_str(),
                        glslangNodeToString(parent).c_str());
            }
            return;
        }
        if (auto nodeAsSelectionNode = node->getAsSelectionNode()) {
            auto conditionId = slurpValue(nodeAsSelectionNode->getCondition(), parent, localSymbols);
            auto trueId = slurpStatementBlock(
                    nodeAsSelectionNode->getTrueBlock(), parent, localSymbols);
            std::optional<StatementBlockId> falseId;
            if (nodeAsSelectionNode->getFalseBlock()) {
                falseId = slurpStatementBlock(
                        nodeAsSelectionNode->getFalseBlock(), parent, localSymbols);
            }
            output.push_back(IfStatement{conditionId, trueId, falseId});
            return;
        }
        if (auto nodeAsAggregate = node->getAsAggregate()) {
            switch (nodeAsAggregate->getOp()) {
                case glslang::EOpSequence:
                    // Flatten this.
                    for (auto child : nodeAsAggregate->getSequence()) {
                        nodeToStatements(child, node, localSymbols, output);
                    }
                    return;
                default:
                    // Fall through and interpret the node as a value instead of an expression.
                    break;
            }
        }
        if (auto nodeAsTyped = node->getAsTyped()) {
            // Ignore random stray symbols and literals as standalone statements since they don't do
            // anything.
            if (!node->getAsSymbolNode() && !node->getAsConstantUnion()) {
                auto valueId = slurpValue(nodeAsTyped, parent, localSymbols);
                if (auto* rValueId = std::get_if<RValueId>(&valueId)) {
                    output.push_back(*rValueId);
                } else {
                    PANIC_PRECONDITION("Encountered non-RValue as statement: %s, parent = %s",
                            glslangNodeToStringWithLoc(node).c_str(),
                            glslangNodeToStringWithLoc(parent).c_str());
                }
            }
            return;
        }
        PANIC_PRECONDITION("Cannot convert to statement: %s, parent = %s",
                glslangNodeToStringWithLoc(node).c_str(),
                glslangNodeToStringWithLoc(parent).c_str());
    }

    std::variant<RValueOperator, FunctionId> slurpOperator(
            glslang::TOperator op,
            const glslang::TType& returnType, const glslang::TType* arg1Type) {
        auto opOrFunctionName = glslangOperatorToRValueOperator(op, mVersion, returnType, arg1Type);
        return std::visit([&](auto&& op) {
            using T = std::decay_t<decltype(op)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return std::variant<RValueOperator, FunctionId>(
                        mFunctionNames.insert(std::move(op)));
            } else if constexpr (std::is_same_v<T, RValueOperator>) {
                return std::variant<RValueOperator, FunctionId>(op);
            } else {
                static_assert(always_false_v<T>, "unreachable");
            }
        }, opOrFunctionName);
    }

    ValueId slurpValueFromConstantUnion(
            glslang::TIntermConstantUnion* node, TIntermNode* parent, LocalSymbols& localSymbols) {
        const auto& constArray = node->getConstArray();
        ASSERT_PRECONDITION(!constArray.empty(),
                "ConstantUnion's value array must not be empty: %s, parent = %s",
                glslangNodeToStringWithLoc(node).c_str(),
                glslangNodeToStringWithLoc(parent).c_str());
        if (constArray.size() == 1) {
            return mRValues.insert(constUnionToLiteralRValue(constArray[0]));
        }
        // Encode this as a constructor function call for now. Maybe encode it as a literal
        // down the line?
        ASSERT_PRECONDITION(node->isVector(),
                "ConstantUnion with multiple values must be a vector: %s, parent = %s",
                glslangNodeToStringWithLoc(node).c_str(),
                glslangNodeToStringWithLoc(parent).c_str());
        const char* functionName;
        switch (constArray.size()) {
            case 2: functionName = "vec2"; break;
            case 3: functionName = "vec3"; break;
            case 4: functionName = "vec4"; break;
            default:
                PANIC_PRECONDITION("Unsupporeted ConstArray size of %d: %s, parent = %s",
                        constArray.size(),
                        glslangNodeToStringWithLoc(node).c_str(),
                        glslangNodeToStringWithLoc(parent).c_str());
        }
        auto functionId = mFunctionNames.insert(functionName);
        std::vector<ValueId> args(constArray.size());
        for (int i = 0; i < constArray.size(); i++) {
            args[i] = mRValues.insert(constUnionToLiteralRValue(constArray[i]));
        }
        return mRValues.insert(
                EvaluableRValue{functionId, std::move(args)});

    }

    ValueId slurpValueFromSymbol(
            glslang::TIntermSymbol* node, LocalSymbols& localSymbols) {
        long long id = node->getId();
        if (auto globalId = mGlobalSymbols.get(id)) {
            return globalId.value();
        }
        auto nameId = mStrings.insert(std::string(node->getAccessName()));
        if (node->getType().isBuiltIn()) {
            return mGlobalSymbols.insert(id, Symbol{nameId, std::nullopt});
        }
        auto typeId = slurpType(node->getType());
        return localSymbols.insert(id, Symbol{nameId, typeId});
    }

    ValueId slurpValueFromUnary(glslang::TIntermUnary*& node, LocalSymbols& localSymbols) {
        auto operandId = slurpValue(node->getOperand(), node, localSymbols);
        auto op = slurpOperator(node->getOp(), node->getType(), &node->getOperand()->getType());
        return mRValues.insert(EvaluableRValue{op, {operandId}});
    }

    ValueId slurpValueFromBinary(
            glslang::TIntermBinary* node, TIntermNode* parent, LocalSymbols& localSymbols) {
        switch (node->getOp()) {
            case glslang::EOpVectorSwizzle: {
                // TODO: swizzle it up
                auto swizzle = node->getRight()->getAsAggregate();
                ASSERT_PRECONDITION(swizzle != nullptr,
                        "Swizzle node must be an aggregate: %s, parent = %s",
                        glslangNodeToStringWithLoc(node).c_str(),
                        glslangNodeToStringWithLoc(parent).c_str());
                ASSERT_PRECONDITION(swizzle->getOp() == glslang::EOpSequence,
                        "Swizzle node must be a sequence: %s, parent = %s",
                        glslangNodeToStringWithLoc(node).c_str(),
                        glslangNodeToStringWithLoc(parent).c_str());
                return mRValues.insert(
                        EvaluableRValue{RValueOperator::VectorSwizzle});
            }
            default: {
                auto lhsId =
                        slurpValue(node->getLeft(), node, localSymbols);
                auto rhsId =
                        slurpValue(node->getRight(), node, localSymbols);
                auto op = slurpOperator(
                        node->getOp(), node->getType(), &node->getLeft()->getType());
                return mRValues.insert(EvaluableRValue{op, {lhsId, rhsId}});
            }
        }
    }

    ValueId slurpValueFromSelection(
            glslang::TIntermSelection* node, TIntermNode* parent, LocalSymbols& localSymbols) {
        // A "selection" as interpreted as an expression is a ternary.
        auto conditionId =
                slurpValue(node->getCondition(), parent, localSymbols);
        auto trueNodeAsTyped = node->getTrueBlock()->getAsTyped();
        auto falseNodeAsTyped = node->getFalseBlock()->getAsTyped();
        if (trueNodeAsTyped && falseNodeAsTyped) {
            auto trueId = slurpValue(trueNodeAsTyped, parent, localSymbols);
            auto falseId = slurpValue(falseNodeAsTyped, parent, localSymbols);
            return mRValues.insert(EvaluableRValue{
                RValueOperator::Ternary, {conditionId, trueId, falseId}});
        } else {
            PANIC_PRECONDITION(
                    "A selection node branch was not typed: true = %s, false = %s, "
                    "parent = %s %s",
                    glslangNodeToStringWithLoc(node->getTrueBlock()).c_str(),
                    glslangNodeToStringWithLoc(node->getFalseBlock()).c_str(),
                    glslangNodeToStringWithLoc(parent).c_str());
        }
    }

    ValueId slurpValueFromFunctionCall(
            glslang::TIntermAggregate* node, TIntermNode* parent, LocalSymbols& localSymbols) {
        auto& sequence = node->getSequence();
        auto functionId = mFunctionNames.insert(std::string(node->getName()));
        std::vector<ValueId> args;
        for (TIntermNode* arg : sequence) {
            if (auto argAsTyped = arg->getAsTyped()) {
                args.push_back(slurpValue(argAsTyped, node, localSymbols));
            } else {
                PANIC_PRECONDITION(
                        "Function call argument was not typed: arg = %s, function "
                        "= %s, parent = %s %s",
                        glslangNodeToStringWithLoc(arg).c_str(),
                        glslangNodeToStringWithLoc(node).c_str(),
                        glslangNodeToStringWithLoc(parent).c_str());
            }
        }
        return mRValues.insert(
                EvaluableRValue{functionId, std::move(args)});
    }

    ValueId slurpValueFromAggregate(
            glslang::TIntermAggregate* node, TIntermNode* parent, LocalSymbols& localSymbols) {
        auto& sequence = node->getSequence();
        std::vector<ValueId> args;
        for (TIntermNode* arg : sequence) {
            if (auto argAsTyped = arg->getAsTyped()) {
                args.push_back(slurpValue(argAsTyped, node, localSymbols));
            } else {
                PANIC_PRECONDITION(
                        "Operator argument was not typed: arg = %s, function = %s, "
                        "parent = %s %s",
                        glslangNodeToStringWithLoc(arg).c_str(),
                        glslangNodeToStringWithLoc(node).c_str(),
                        glslangNodeToStringWithLoc(parent).c_str());
            }
        }
        const glslang::TType* firstArgType = nullptr;
        if (!sequence.empty()) {
            firstArgType = &sequence[0]->getAsTyped()->getType();
        }
        auto op = slurpOperator(node->getOp(), node->getType(), firstArgType);
        return mRValues.insert(EvaluableRValue{op, std::move(args)});
    }

    ValueId slurpValue(glslang::TIntermTyped* node, TIntermNode* parent,
            LocalSymbols& localSymbols) {
        if (auto nodeAsConstantUnion = node->getAsConstantUnion()) {
            return slurpValueFromConstantUnion(
                    nodeAsConstantUnion, parent, localSymbols);
        }
        if (auto nodeAsSymbol = node->getAsSymbolNode()) {
            return slurpValueFromSymbol(nodeAsSymbol, localSymbols);
        }
        if (auto nodeAsUnary = node->getAsUnaryNode()) {
            return slurpValueFromUnary(nodeAsUnary, localSymbols);
        }
        if (auto nodeAsBinary = node->getAsBinaryNode()) {
            return slurpValueFromBinary(nodeAsBinary, parent, localSymbols);
        }
        if (auto nodeAsSelection = node->getAsSelectionNode()) {
            return slurpValueFromSelection(nodeAsSelection, parent, localSymbols);
        }
        if (auto nodeAsAggregate = node->getAsAggregate()) {
            switch (nodeAsAggregate->getOp()) {
                case glslang::EOpFunction:
                case glslang::EOpLinkerObjects:
                case glslang::EOpParameters:
                case glslang::EOpSequence:
                    // Explicitly ban these from becoming RValues, since we probably
                    // made a mistake somewhere...
                    break;
                case glslang::EOpFunctionCall:
                    return slurpValueFromFunctionCall(nodeAsAggregate, parent, localSymbols);
                default:
                    return slurpValueFromAggregate(nodeAsAggregate, parent, localSymbols);
            }
        }
        PANIC_PRECONDITION("Cannot convert to statement: %s, parent = %s",
                glslangNodeToStringWithLoc(node).c_str(),
                glslangNodeToStringWithLoc(parent).c_str());
    }
};

PackFromGlsl fromGlsl(const glslang::TIntermediate& intermediate) {
    Slurper slurper(intermediate);
    return slurper.intoPack();
}

} // namespace astrict