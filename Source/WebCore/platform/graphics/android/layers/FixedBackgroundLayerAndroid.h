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

#ifndef FixedBackgroundLayerAndroid_h
#define FixedBackgroundLayerAndroid_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerAndroid.h"

namespace WebCore {

// TODO: the hierarchy manipulation in GraphicsLayerAndroid should
// (at least partly) be handled in this class
class FixedBackgroundLayerAndroid : public LayerAndroid {
public:
    FixedBackgroundLayerAndroid(RenderLayer* owner)
        : LayerAndroid(owner) {}
    FixedBackgroundLayerAndroid(const FixedBackgroundLayerAndroid& layer)
        : LayerAndroid(layer) {}
    FixedBackgroundLayerAndroid(const LayerAndroid& layer)
        : LayerAndroid(layer) {}
    virtual ~FixedBackgroundLayerAndroid() {};

    virtual LayerAndroid* copy() const { return new FixedBackgroundLayerAndroid(*this); }

    virtual bool isFixedBackground() const { return true; }

    virtual SubclassType subclassType() const { return LayerAndroid::FixedBackgroundLayer; }
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // FixedBackgroundLayerAndroid_h
