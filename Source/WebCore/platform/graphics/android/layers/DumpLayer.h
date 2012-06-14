/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef DumpLayer_h
#define DumpLayer_h

#include "FixedPositioning.h"
#include "IntPoint.h"
#include "LayerAndroid.h"
#include "SkPoint.h"
#include "SkRect.h"
#include "SkSize.h"
#include "TransformationMatrix.h"

// Debug tools : dump the layers tree in a file.
// The format is simple:
// properties have the form: key = value;
// all statements are finished with a semi-colon.
// value can be:
// - int
// - float
// - array of elements
// - composed type
// a composed type enclose properties in { and }
// an array enclose composed types in { }, separated with a comma.
// exemple:
// {
//   x = 3;
//   y = 4;
//   value = {
//     x = 3;
//     y = 4;
//   };
//   anarray = [
//     { x = 3; },
//     { y = 4; }
//   ];
// }

namespace WebCore {

class LayerDumper {
public:
    LayerDumper(int initialIndentLevel = 0)
        : m_indentLevel(initialIndentLevel)
    {}
    virtual ~LayerDumper() {}

    virtual void beginLayer(const char* className, const LayerAndroid* layerPtr) {}

    virtual void endLayer() {}

    virtual void beginChildren(int childCount) {
        m_indentLevel++;
    }
    virtual void endChildren() {
        m_indentLevel--;
    }

    void writeIntVal(const char* label, int value);
    void writeHexVal(const char* label, int value);
    void writeFloatVal(const char* label, float value);
    void writePoint(const char* label, SkPoint value);
    void writeIntPoint(const char* label, IntPoint value);
    void writeSize(const char* label, SkSize value);
    void writeRect(const char* label, SkRect value);
    void writeMatrix(const char* label, const TransformationMatrix& value);
    void writeLength(const char* label, SkLength value);

protected:
    virtual void writeEntry(const char* label, const char* value) = 0;

    int m_indentLevel;

private:
    static const int BUF_SIZE = 4096;
    char m_valueBuffer[BUF_SIZE];
};

class FileLayerDumper : public LayerDumper {
public:
    FileLayerDumper(FILE* file)
        : m_file(file)
    {}

    virtual void beginLayer(const char* className, const LayerAndroid* layerPtr);
    virtual void endLayer();
protected:
    virtual void writeEntry(const char* label, const char* value);

private:
    void writeLine(const char* str);
    FILE* m_file;
};

}

#endif // DumpLayer_h
