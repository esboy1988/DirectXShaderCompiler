//===--- HlslTypes.cpp  - Type system for HLSL                 ----*- C++
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HlslTypes.cpp                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///
/// \file                                                                    //
/// \brief Defines the HLSL type system interface.                           //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/CanonicalType.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/Type.h"
#include "clang/Sema/AttributeList.h" // conceptually ParsedAttributes
#include "clang/AST/ASTContext.h"

using namespace clang;

namespace hlsl {

/// Checks whether the pAttributes indicate a parameter is inout or out; if
/// inout, pIsIn will be set to true.
bool IsParamAttributedAsOut(_In_opt_ clang::AttributeList *pAttributes,
  _Out_opt_ bool *pIsIn);

/// <summary>Gets the type with structural information (elements and shape) for
/// the given type.</summary>
/// <remarks>This function will strip lvalue/rvalue references, attributes and
/// qualifiers.</remarks>
QualType GetStructuralForm(QualType type) {
  if (type.isNull()) {
    return type;
  }

  const ReferenceType *RefType = nullptr;
  const AttributedType *AttrType = nullptr;
  while ((RefType = dyn_cast<ReferenceType>(type)) ||
         (AttrType = dyn_cast<AttributedType>(type))) {
    type = RefType ? RefType->getPointeeType() : AttrType->getEquivalentType();
  }

  return type->getCanonicalTypeUnqualified();
}

uint32_t GetElementCount(clang::QualType type) {
  uint32_t rowCount, colCount;
  GetRowsAndColsForAny(type, rowCount, colCount);
  return rowCount * colCount;
}

/// <summary>Returns the number of elements in the specified array
/// type.</summary>
uint32_t GetArraySize(clang::QualType type) {
  assert(type->isArrayType() && "otherwise caller shouldn't be invoking this");

  if (type->isConstantArrayType()) {
    const ConstantArrayType *arrayType =
        (const ConstantArrayType *)type->getAsArrayTypeUnsafe();
    return arrayType->getSize().getLimitedValue();
  } else {
    return 0;
  }
}

/// <summary>Returns the number of elements in the specified vector
/// type.</summary>
uint32_t GetHLSLVecSize(clang::QualType type) {
  type = GetStructuralForm(type);

  const Type *Ty = type.getCanonicalType().getTypePtr();
  const RecordType *RT = dyn_cast<RecordType>(Ty);
  assert(RT != nullptr && "otherwise caller shouldn't be invoking this");
  const ClassTemplateSpecializationDecl *templateDecl =
      dyn_cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  assert(templateDecl != nullptr &&
         "otherwise caller shouldn't be invoking this");
  assert(templateDecl->getName() == "vector" &&
         "otherwise caller shouldn't be invoking this");

  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  const TemplateArgument &arg0 = argList[0];
  const TemplateArgument &arg1 = argList[1];
  QualType elemTy = arg0.getAsType();
  llvm::APSInt vecSize = arg1.getAsIntegral();
  return vecSize.getLimitedValue();
}

void GetRowsAndCols(clang::QualType type, uint32_t &rowCount,
                    uint32_t &colCount) {
  type = GetStructuralForm(type);

  const Type *Ty = type.getCanonicalType().getTypePtr();
  const RecordType *RT = dyn_cast<RecordType>(Ty);
  assert(RT != nullptr && "otherwise caller shouldn't be invoking this");
  const ClassTemplateSpecializationDecl *templateDecl =
      dyn_cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  assert(templateDecl != nullptr &&
         "otherwise caller shouldn't be invoking this");
  assert(templateDecl->getName() == "matrix" &&
         "otherwise caller shouldn't be invoking this");

  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  const TemplateArgument &arg0 = argList[0];
  const TemplateArgument &arg1 = argList[1];
  const TemplateArgument &arg2 = argList[2];
  QualType elemTy = arg0.getAsType();
  llvm::APSInt rowSize = arg1.getAsIntegral();
  llvm::APSInt colSize = arg2.getAsIntegral();
  rowCount = rowSize.getLimitedValue();
  colCount = colSize.getLimitedValue();
}

void GetRowsAndColsForAny(QualType type, uint32_t &rowCount,
                          uint32_t &colCount) {
  assert(!type.isNull());

  type = GetStructuralForm(type);
  rowCount = 1;
  colCount = 1;
  const Type *Ty = type.getCanonicalType().getTypePtr();
  if (type->isArrayType()) {
    if (type->isConstantArrayType()) {
      const ConstantArrayType *arrayType =
          (const ConstantArrayType *)type->getAsArrayTypeUnsafe();
      colCount = arrayType->getSize().getLimitedValue();
    } else {
      colCount = 0;
    }
  } else if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "matrix") {
        const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
        const TemplateArgument &arg1 = argList[1];
        const TemplateArgument &arg2 = argList[2];
        llvm::APSInt rowSize = arg1.getAsIntegral();
        llvm::APSInt colSize = arg2.getAsIntegral();
        rowCount = rowSize.getLimitedValue();
        colCount = colSize.getLimitedValue();
      } else if (templateDecl->getName() == "vector") {
        const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
        const TemplateArgument &arg1 = argList[1];
        llvm::APSInt rowSize = arg1.getAsIntegral();
        colCount = rowSize.getLimitedValue();
      }
    }
  }
}

void GetHLSLMatRowColCount(clang::QualType type, unsigned int &row,
                           unsigned int &col) {
  GetRowsAndColsForAny(type, row, col);
}
clang::QualType GetHLSLVecElementType(clang::QualType type) {
  type = GetStructuralForm(type);

  const Type *Ty = type.getCanonicalType().getTypePtr();
  const RecordType *RT = dyn_cast<RecordType>(Ty);
  assert(RT != nullptr && "otherwise caller shouldn't be invoking this");
  const ClassTemplateSpecializationDecl *templateDecl =
      dyn_cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  assert(templateDecl != nullptr &&
         "otherwise caller shouldn't be invoking this");
  assert(templateDecl->getName() == "vector" &&
         "otherwise caller shouldn't be invoking this");

  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  const TemplateArgument &arg0 = argList[0];
  QualType elemTy = arg0.getAsType();
  return elemTy;
}
clang::QualType GetHLSLMatElementType(clang::QualType type) {
  type = GetStructuralForm(type);

  const Type *Ty = type.getCanonicalType().getTypePtr();
  const RecordType *RT = dyn_cast<RecordType>(Ty);
  assert(RT != nullptr && "otherwise caller shouldn't be invoking this");
  const ClassTemplateSpecializationDecl *templateDecl =
      dyn_cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  assert(templateDecl != nullptr &&
         "otherwise caller shouldn't be invoking this");
  assert(templateDecl->getName() == "matrix" &&
         "otherwise caller shouldn't be invoking this");

  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  const TemplateArgument &arg0 = argList[0];
  QualType elemTy = arg0.getAsType();
  return elemTy;
}
// TODO: Add type cache to ASTContext.
bool IsHLSLInputPatchType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "InputPatch") {
        return true;
      }
    }
  }
  return false;
}
bool IsHLSLOutputPatchType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "OutputPatch") {
        return true;
      }
    }
  }
  return false;
}
bool IsHLSLPointStreamType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "PointStream")
        return true;
    }
  }
  return false;
}
bool IsHLSLLineStreamType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "LineStream")
        return true;
    }
  }
  return false;
}
bool IsHLSLTriangleStreamType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "TriangleStream")
        return true;
    }
  }
  return false;
}
bool IsHLSLStreamOutputType(QualType type) {
  type = type.getCanonicalType();
  if (const RecordType *RT = dyn_cast<RecordType>(type)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(
                RT->getAsCXXRecordDecl())) {
      if (templateDecl->getName() == "PointStream")
        return true;
      if (templateDecl->getName() == "LineStream")
        return true;
      if (templateDecl->getName() == "TriangleStream")
        return true;
    }
  }
  return false;
}
bool IsHLSLResouceType(clang::QualType type) {
  if (const RecordType *RT = type->getAs<RecordType>()) {
    StringRef name = RT->getDecl()->getName();
    if (name == "Texture1D" || name == "RWTexture1D")
      return true;
    if (name == "Texture2D" || name == "RWTexture2D")
      return true;
    if (name == "Texture2DMS" || name == "RWTexture2DMS")
      return true;
    if (name == "Texture3D" || name == "RWTexture3D")
      return true;
    if (name == "TextureCube" || name == "RWTextureCube")
      return true;

    if (name == "Texture1DArray" || name == "RWTexture1DArray")
      return true;
    if (name == "Texture2DArray" || name == "RWTexture2DArray")
      return true;
    if (name == "Texture2DMSArray" || name == "RWTexture2DMSArray")
      return true;
    if (name == "TextureCubeArray" || name == "RWTextureCubeArray")
      return true;

    if (name == "ByteAddressBuffer" || name == "RWByteAddressBuffer")
      return true;

    if (name == "StructuredBuffer" || name == "RWStructuredBuffer")
      return true;

    if (name == "AppendStructuredBuffer" || name == "ConsumeStructuredBuffer")
      return true;

    if (name == "Buffer" || name == "RWBuffer")
      return true;

    if (name == "SamplerState" || name == "SamplerComparisonState")
      return true;

    if (name == "ConstantBuffer")
      return true;
  }
  return false;
}
QualType GetHLSLResourceResultType(QualType type) {
  type = type.getCanonicalType();
  const RecordType *RT = cast<RecordType>(type);
  StringRef name = RT->getDecl()->getName();

  if (name == "ByteAddressBuffer" || name == "RWByteAddressBuffer") {
    RecordDecl *RD = RT->getDecl();
    QualType resultTy = RD->field_begin()->getType();
    return resultTy;
  }
  const ClassTemplateSpecializationDecl *templateDecl =
      cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  return argList[0].getAsType();
}
bool IsIncompleteHLSLResourceArrayType(clang::ASTContext &context,
                                       clang::QualType type) {
  if (type->isIncompleteArrayType()) {
    const IncompleteArrayType *IAT = context.getAsIncompleteArrayType(type);
    QualType EltTy = IAT->getElementType();
    if (IsHLSLResouceType(EltTy))
      return true;
  }
  return false;
}
QualType GetHLSLInputPatchElementType(QualType type) {
  type = type.getCanonicalType();
  const RecordType *RT = cast<RecordType>(type);
  const ClassTemplateSpecializationDecl *templateDecl =
      cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  return argList[0].getAsType();
}
unsigned GetHLSLInputPatchCount(QualType type) {
  type = type.getCanonicalType();
  const RecordType *RT = cast<RecordType>(type);
  const ClassTemplateSpecializationDecl *templateDecl =
      cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  return argList[1].getAsIntegral().getLimitedValue();
}
clang::QualType GetHLSLOutputPatchElementType(QualType type) {
  type = type.getCanonicalType();
  const RecordType *RT = cast<RecordType>(type);
  const ClassTemplateSpecializationDecl *templateDecl =
      cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  return argList[0].getAsType();
}
unsigned GetHLSLOutputPatchCount(QualType type) {
  type = type.getCanonicalType();
  const RecordType *RT = cast<RecordType>(type);
  const ClassTemplateSpecializationDecl *templateDecl =
      cast<ClassTemplateSpecializationDecl>(RT->getAsCXXRecordDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  return argList[1].getAsIntegral().getLimitedValue();
}

_Use_decl_annotations_
bool IsParamAttributedAsOut(clang::AttributeList *pAttributes, bool *pIsIn) {
  bool anyFound = false;
  bool inFound = false;
  bool outFound = false;
  while (pAttributes != nullptr) {
    switch (pAttributes->getKind()) {
    case AttributeList::AT_HLSLIn:
      anyFound = true;
      inFound = true;
      break;
    case AttributeList::AT_HLSLOut:
      anyFound = true;
      outFound = true;
      break;
    case AttributeList::AT_HLSLInOut:
      anyFound = true;
      outFound = true;
      inFound = true;
      break;
    }
    pAttributes = pAttributes->getNext();
  }
  if (pIsIn)
    *pIsIn = inFound || anyFound == false;
  return outFound;
}

_Use_decl_annotations_
hlsl::ParameterModifier ParamModFromAttributeList(clang::AttributeList *pAttributes) {
  bool isIn, isOut;
  isOut = IsParamAttributedAsOut(pAttributes, &isIn);
  return ParameterModifier::FromInOut(isIn, isOut);
}

hlsl::ParameterModifier ParamModFromAttrs(llvm::ArrayRef<InheritableAttr *> attributes) {
  bool isIn = false, isOut = false;
  for (InheritableAttr * attr : attributes) {
    if (isa<HLSLInAttr>(attr))
      isIn = true;
    else if (isa<HLSLOutAttr>(attr))
      isOut = true;
    else if (isa<HLSLInOutAttr>(attr))
      isIn = isOut = true;
  }
  // Without any specifications, default to in.
  if (!isIn && !isOut) {
    isIn = true;
  }
  return ParameterModifier::FromInOut(isIn, isOut);
}

}
