/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 Sony Mobile Communications AB
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ArrayBuffer.h"
#include "Uint8ClampedArray.h"

#include "V8Binding.h"
#include "V8ArrayBuffer.h"
#include "V8ArrayBufferViewCustom.h"
#include "V8Uint8ClampedArray.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8Uint8ClampedArray::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Uint8ClampedArray.Contructor");

    return constructWebGLArray<Uint8ClampedArray, unsigned char>(args, &info, v8::kExternalPixelArray);
}

v8::Handle<v8::Value> V8Uint8ClampedArray::setCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.Uint8ClampedArray.set()");
    return setWebGLArrayHelper<Uint8ClampedArray, V8Uint8ClampedArray>(args);
}

v8::Handle<v8::Value> toV8(Uint8ClampedArray* impl)
{
    if (!impl)
        return v8::Null();
    v8::Handle<v8::Object> wrapper = V8Uint8ClampedArray::wrap(impl);
    if (!wrapper.IsEmpty())
        wrapper->SetIndexedPropertiesToExternalArrayData(impl->baseAddress(), v8::kExternalPixelArray, impl->length());
    return wrapper;
}

} // namespace WebCore
