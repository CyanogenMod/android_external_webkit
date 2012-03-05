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

#include "IntPoint.h"
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

void lwrite(FILE* file, const char* str);
void writeIndent(FILE* file, int indentLevel);
void writeln(FILE* file, int indentLevel, const char* str);
void writeIntVal(FILE* file, int indentLevel, const char* str, int value);
void writeHexVal(FILE* file, int indentLevel, const char* str, int value);
void writeFloatVal(FILE* file, int indentLevel, const char* str, float value);
void writePoint(FILE* file, int indentLevel, const char* str, SkPoint point);
void writeIntPoint(FILE* file, int indentLevel, const char* str, IntPoint point);
void writeSize(FILE* file, int indentLevel, const char* str, SkSize size);
void writeRect(FILE* file, int indentLevel, const char* str, SkRect rect);
void writeMatrix(FILE* file, int indentLevel, const char* str, const TransformationMatrix& matrix);

}

#endif // DumpLayer_h
