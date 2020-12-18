// Copyright 2019 Autodesk, Inc.
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
#include "write_arnold_type.h"

#include <ai.h>

#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

//-*************************************************************************

PXR_NAMESPACE_USING_DIRECTIVE

/**
 *    Write out any Arnold node to a generic "typed"  USD primitive (eg
 *ArnoldSetParameter, ArnoldDriverExr, etc...). In this write function, we need
 *to create the USD primitive, then loop over the Arnold node attributes, and
 *write them to the USD file. Note that we could (should) use the schemas to do
 *this, but since the conversion is simple, for now we're hardcoding it here.
 *For now the attributes are prefixed with "arnold:" as this is what is done in
 *the schemas. But this is something that we could remove in the future, as it's
 *not strictly needed.
 **/
void UsdArnoldWriteArnoldType::Write(const AtNode *node, UsdArnoldWriter &writer)
{
     // get the output name of this USD primitive 
    std::string nodeName = GetArnoldNodeName(node, writer);
    UsdStageRefPtr stage = writer.GetUsdStage();    // get the current stage defined in the writer
    SdfPath objPath(nodeName);

    UsdPrim prim = stage->GetPrimAtPath(objPath);
    if (prim && prim.IsActive()) {
        // This primitive was already written, let's early out
        return;
    }
    prim = stage->DefinePrim(objPath, TfToken(_usdName));

    int nodeEntryType = AiNodeEntryGetType(AiNodeGetNodeEntry(node));
    // For arnold nodes that have a transform matrix, we read it as in a 
    // UsdGeomXformable
    if (nodeEntryType == AI_NODE_SHAPE 
        || nodeEntryType == AI_NODE_CAMERA || nodeEntryType == AI_NODE_LIGHT)
    {
        UsdGeomXformable xformable(prim);
        _WriteMatrix(xformable, node, writer);
        // If this arnold node is a shape, let's write the material bindings
        if (nodeEntryType == AI_NODE_SHAPE)
            _WriteMaterialBinding(node, prim, writer);
    }

    _WriteArnoldParameters(node, writer, prim, "arnold");
}

void UsdArnoldWriteGinstance::_ProcessInstanceAttribute(
    UsdPrim &prim, const AtNode *node, const AtNode *target, const char *attrName, int attrType)
{
    if (AiNodeEntryLookUpParameter(AiNodeGetNodeEntry(target), attrName) == nullptr)
        return; // the attribute doesn't exist in the instanced node

    // Now compare the values between the ginstance and the target node. If the value
    // is different we'll want to write it even though it's the default value
    bool writeValue = false;
    SdfValueTypeName usdType;
    if (attrType == AI_TYPE_BOOLEAN) {
        writeValue = (AiNodeGetBool(node, attrName) != AiNodeGetBool(target, attrName));
        usdType = SdfValueTypeNames->Bool;
    } else if (attrType == AI_TYPE_BYTE) {
        writeValue = (AiNodeGetByte(node, attrName) != AiNodeGetByte(target, attrName));
        usdType = SdfValueTypeNames->UChar;
    } else
        return;

    if (writeValue) {
        UsdAttribute attr = prim.CreateAttribute(TfToken(attrName), usdType, false);
        if (attrType == AI_TYPE_BOOLEAN)
            attr.Set(AiNodeGetBool(node, attrName));
        else if (attrType == AI_TYPE_BYTE)
            attr.Set(AiNodeGetByte(node, attrName));
    }
    _exportedAttrs.insert(attrName);
}

void UsdArnoldWriteGinstance::Write(const AtNode *node, UsdArnoldWriter &writer)
{
    // get the output name of this USD primitive
    std::string nodeName = GetArnoldNodeName(node, writer);
    UsdStageRefPtr stage = writer.GetUsdStage();    // get the current stage defined in the writer
    SdfPath objPath(nodeName);

    UsdPrim prim = stage->GetPrimAtPath(objPath);
    if (prim && prim.IsActive()) {
        // This primitive was already written, let's early out
        return;
    }
    prim = stage->DefinePrim(objPath, TfToken(_usdName));

    AtNode *target = (AtNode *)AiNodeGetPtr(node, "node");
    if (target) {
        _ProcessInstanceAttribute(prim, node, target, "visibility", AI_TYPE_BYTE);
        _ProcessInstanceAttribute(prim, node, target, "sidedness", AI_TYPE_BYTE);
        _ProcessInstanceAttribute(prim, node, target, "matte", AI_TYPE_BOOLEAN);
        _ProcessInstanceAttribute(prim, node, target, "receive_shadows", AI_TYPE_BOOLEAN);
        _ProcessInstanceAttribute(prim, node, target, "invert_normals", AI_TYPE_BOOLEAN);
        _ProcessInstanceAttribute(prim, node, target, "self_shadows", AI_TYPE_BOOLEAN);
    }
    UsdGeomXformable xformable(prim);
    _WriteMatrix(xformable, node, writer);
    _WriteMaterialBinding(node, prim, writer);

    _WriteArnoldParameters(node, writer, prim, "arnold");
}
