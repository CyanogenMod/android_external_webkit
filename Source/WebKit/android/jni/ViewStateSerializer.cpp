/*
 * Copyright 2011, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LOG_TAG "ViewStateSerializer"
#define LOG_NDEBUG 1

#include "config.h"

#include "BaseLayerAndroid.h"
#include "CreateJavaOutputStreamAdaptor.h"
#include "DumpLayer.h"
#include "FixedPositioning.h"
#include "ImagesManager.h"
#include "IFrameContentLayerAndroid.h"
#include "IFrameLayerAndroid.h"
#include "Layer.h"
#include "LayerAndroid.h"
#include "LayerContent.h"
#include "PictureLayerContent.h"
#include "ScrollableLayerAndroid.h"
#include "SkFlattenable.h"
#include "SkPicture.h"
#include "TilesManager.h"

#include <JNIUtility.h>
#include <JNIHelp.h>
#include <jni.h>

namespace android {

enum LayerTypes {
    LTNone = 0,
    LTLayerAndroid = 1,
    LTScrollableLayerAndroid = 2,
    LTFixedLayerAndroid = 3
};

#define ID "mID"
#define LEFT "layout:mLeft"
#define TOP "layout:mTop"
#define WIDTH "layout:getWidth()"
#define HEIGHT "layout:getHeight()"

class HierarchyLayerDumper : public LayerDumper {
public:
    HierarchyLayerDumper(SkWStream* stream, int level)
        : LayerDumper(level)
        , m_stream(stream)
    {}

    virtual void beginLayer(const char* className, const LayerAndroid* layerPtr) {
        LayerDumper::beginLayer(className, layerPtr);
        for (int i = 0; i < m_indentLevel; i++) {
            m_stream->writeText(" ");
        }
        m_stream->writeText(className);
        m_stream->writeText("@");
        m_stream->writeHexAsText(layerPtr->uniqueId());
        m_stream->writeText(" ");

        writeHexVal(ID, (int) layerPtr);
        writeIntVal(LEFT, layerPtr->getPosition().fX);
        writeIntVal(TOP, layerPtr->getPosition().fY);
        writeIntVal(WIDTH, layerPtr->getWidth());
        writeIntVal(HEIGHT, layerPtr->getHeight());
    }

    virtual void beginChildren(int childCount) {
        m_stream->writeText("\n");
        LayerDumper::beginChildren(childCount);
    }

protected:
    virtual void writeEntry(const char* label, const char* value) {
        m_stream->writeText(label);
        m_stream->writeText("=");
        int len = strlen(value);
        m_stream->writeDecAsText(len);
        m_stream->writeText(",");
        m_stream->writeText(value);
        m_stream->writeText(" ");
    }

private:
    SkWStream* m_stream;
};

static void nativeDumpLayerHierarchy(JNIEnv* env, jobject, jint jbaseLayer, jint level,
                                     jobject jstream, jbyteArray jstorage)
{
    SkWStream *stream = CreateJavaOutputStreamAdaptor(env, jstream, jstorage);
    BaseLayerAndroid* baseLayer = reinterpret_cast<BaseLayerAndroid*>(jbaseLayer);
    SkSafeRef(baseLayer);
    HierarchyLayerDumper dumper(stream, level);
    baseLayer->dumpLayers(&dumper);
    SkSafeUnref(baseLayer);
    delete stream;
}

static bool nativeSerializeViewState(JNIEnv* env, jobject, jint jbaseLayer,
                                     jobject jstream, jbyteArray jstorage)
{
    BaseLayerAndroid* baseLayer = (BaseLayerAndroid*) jbaseLayer;
    if (!baseLayer)
        return false;

    SkWStream *stream = CreateJavaOutputStreamAdaptor(env, jstream, jstorage);
#if USE(ACCELERATED_COMPOSITING)
    stream->write32(baseLayer->getBackgroundColor().rgb());
#else
    stream->write32(0);
#endif
    if (!stream)
        return false;
    if (baseLayer->content())
        baseLayer->content()->serialize(stream);
    else
        return false;
    int childCount = baseLayer->countChildren();
    ALOGV("BaseLayer has %d child(ren)", childCount);
    stream->write32(childCount);
    for (int i = 0; i < childCount; i++) {
        LayerAndroid* layer = static_cast<LayerAndroid*>(baseLayer->getChild(i));
        serializeLayer(layer, stream);
    }
    delete stream;
    return true;
}

static BaseLayerAndroid* nativeDeserializeViewState(JNIEnv* env, jobject, jint version,
                                                    jobject jstream, jbyteArray jstorage)
{
    SkStream* stream = CreateJavaInputStreamAdaptor(env, jstream, jstorage);
    if (!stream)
        return 0;
    Color color = stream->readU32();
    SkPicture* picture = new SkPicture(stream);
    PictureLayerContent* content = new PictureLayerContent(picture);

    BaseLayerAndroid* layer = new BaseLayerAndroid(content);
    layer->setBackgroundColor(color);

    SkRegion dirtyRegion;
    dirtyRegion.setRect(0, 0, content->width(), content->height());
    layer->markAsDirty(dirtyRegion);

    SkSafeUnref(content);
    SkSafeUnref(picture);
    int childCount = stream->readS32();
    for (int i = 0; i < childCount; i++) {
        LayerAndroid* childLayer = deserializeLayer(version, stream);
        if (childLayer)
            layer->addChild(childLayer);
    }
    delete stream;
    return layer;
}

// Serialization helpers

void writeMatrix(SkWStream *stream, const SkMatrix& matrix)
{
    for (int i = 0; i < 9; i++)
        stream->writeScalar(matrix[i]);
}

SkMatrix readMatrix(SkStream *stream)
{
    SkMatrix matrix;
    for (int i = 0; i < 9; i++)
        matrix.set(i, stream->readScalar());
    return matrix;
}

void writeSkLength(SkWStream *stream, SkLength length)
{
    stream->write32(length.type);
    stream->writeScalar(length.value);
}

SkLength readSkLength(SkStream *stream)
{
    SkLength len;
    len.type = (SkLength::SkLengthType) stream->readU32();
    len.value = stream->readScalar();
    return len;
}

void writeSkRect(SkWStream *stream, SkRect rect)
{
    stream->writeScalar(rect.fLeft);
    stream->writeScalar(rect.fTop);
    stream->writeScalar(rect.fRight);
    stream->writeScalar(rect.fBottom);
}

SkRect readSkRect(SkStream *stream)
{
    SkRect rect;
    rect.fLeft = stream->readScalar();
    rect.fTop = stream->readScalar();
    rect.fRight = stream->readScalar();
    rect.fBottom = stream->readScalar();
    return rect;
}

void writeTransformationMatrix(SkWStream *stream, TransformationMatrix& matrix)
{
    double value;
    int dsize = sizeof(double);
    value = matrix.m11();
    stream->write(&value, dsize);
    value = matrix.m12();
    stream->write(&value, dsize);
    value = matrix.m13();
    stream->write(&value, dsize);
    value = matrix.m14();
    stream->write(&value, dsize);
    value = matrix.m21();
    stream->write(&value, dsize);
    value = matrix.m22();
    stream->write(&value, dsize);
    value = matrix.m23();
    stream->write(&value, dsize);
    value = matrix.m24();
    stream->write(&value, dsize);
    value = matrix.m31();
    stream->write(&value, dsize);
    value = matrix.m32();
    stream->write(&value, dsize);
    value = matrix.m33();
    stream->write(&value, dsize);
    value = matrix.m34();
    stream->write(&value, dsize);
    value = matrix.m41();
    stream->write(&value, dsize);
    value = matrix.m42();
    stream->write(&value, dsize);
    value = matrix.m43();
    stream->write(&value, dsize);
    value = matrix.m44();
    stream->write(&value, dsize);
}

void readTransformationMatrix(SkStream *stream, TransformationMatrix& matrix)
{
    double value;
    int dsize = sizeof(double);
    stream->read(&value, dsize);
    matrix.setM11(value);
    stream->read(&value, dsize);
    matrix.setM12(value);
    stream->read(&value, dsize);
    matrix.setM13(value);
    stream->read(&value, dsize);
    matrix.setM14(value);
    stream->read(&value, dsize);
    matrix.setM21(value);
    stream->read(&value, dsize);
    matrix.setM22(value);
    stream->read(&value, dsize);
    matrix.setM23(value);
    stream->read(&value, dsize);
    matrix.setM24(value);
    stream->read(&value, dsize);
    matrix.setM31(value);
    stream->read(&value, dsize);
    matrix.setM32(value);
    stream->read(&value, dsize);
    matrix.setM33(value);
    stream->read(&value, dsize);
    matrix.setM34(value);
    stream->read(&value, dsize);
    matrix.setM41(value);
    stream->read(&value, dsize);
    matrix.setM42(value);
    stream->read(&value, dsize);
    matrix.setM43(value);
    stream->read(&value, dsize);
    matrix.setM44(value);
}

void serializeLayer(LayerAndroid* layer, SkWStream* stream)
{
    if (!layer) {
        ALOGV("NULL layer!");
        stream->write8(LTNone);
        return;
    }
    if (layer->isMedia() || layer->isVideo()) {
        ALOGV("Layer isn't supported for serialization: isMedia: %s, isVideo: %s",
             layer->isMedia() ? "true" : "false",
             layer->isVideo() ? "true" : "false");
        stream->write8(LTNone);
        return;
    }
    LayerTypes type = LTLayerAndroid;
    if (layer->contentIsScrollable())
        type = LTScrollableLayerAndroid;
    stream->write8(type);

    // Start with Layer fields
    stream->writeBool(layer->shouldInheritFromRootTransform());
    stream->writeScalar(layer->getOpacity());
    stream->writeScalar(layer->getSize().width());
    stream->writeScalar(layer->getSize().height());
    stream->writeScalar(layer->getPosition().x());
    stream->writeScalar(layer->getPosition().y());
    stream->writeScalar(layer->getAnchorPoint().x());
    stream->writeScalar(layer->getAnchorPoint().y());
    writeMatrix(stream, layer->getMatrix());
    writeMatrix(stream, layer->getChildrenMatrix());

    // Next up, LayerAndroid fields
    stream->writeBool(layer->m_haveClip);
    stream->writeBool(layer->isPositionFixed());
    stream->writeBool(layer->m_backgroundColorSet);
    stream->writeBool(layer->isIFrame());

    // With the current LayerAndroid hierarchy, LayerAndroid doesn't have
    // those fields anymore. Let's keep the current serialization format for
    // now and output blank fields... not great, but probably better than
    // dealing with multiple versions.
    if (layer->fixedPosition()) {
        FixedPositioning* fixedPosition = layer->fixedPosition();
        writeSkLength(stream, fixedPosition->m_fixedLeft);
        writeSkLength(stream, fixedPosition->m_fixedTop);
        writeSkLength(stream, fixedPosition->m_fixedRight);
        writeSkLength(stream, fixedPosition->m_fixedBottom);
        writeSkLength(stream, fixedPosition->m_fixedMarginLeft);
        writeSkLength(stream, fixedPosition->m_fixedMarginTop);
        writeSkLength(stream, fixedPosition->m_fixedMarginRight);
        writeSkLength(stream, fixedPosition->m_fixedMarginBottom);
        writeSkRect(stream, fixedPosition->m_fixedRect);
        stream->write32(fixedPosition->m_renderLayerPos.x());
        stream->write32(fixedPosition->m_renderLayerPos.y());
    } else {
        SkLength length;
        SkRect rect;
        writeSkLength(stream, length); // fixedLeft
        writeSkLength(stream, length); // fixedTop
        writeSkLength(stream, length); // fixedRight
        writeSkLength(stream, length); // fixedBottom
        writeSkLength(stream, length); // fixedMarginLeft
        writeSkLength(stream, length); // fixedMarginTop
        writeSkLength(stream, length); // fixedMarginRight
        writeSkLength(stream, length); // fixedMarginBottom
        writeSkRect(stream, rect);     // fixedRect
        stream->write32(0);            // renderLayerPos.x()
        stream->write32(0);            // renderLayerPos.y()
    }

    stream->writeBool(layer->m_backfaceVisibility);
    stream->writeBool(layer->m_visible);
    stream->write32(layer->m_backgroundColor);
    stream->writeBool(layer->m_preserves3D);
    stream->writeScalar(layer->m_anchorPointZ);
    stream->writeScalar(layer->m_drawOpacity);
    bool hasContentsImage = layer->m_imageCRC != 0;
    stream->writeBool(hasContentsImage);
    if (hasContentsImage) {
        SkFlattenableWriteBuffer buffer(1024);
        buffer.setFlags(SkFlattenableWriteBuffer::kCrossProcess_Flag);
        ImageTexture* imagetexture =
                ImagesManager::instance()->retainImage(layer->m_imageCRC);
        if (imagetexture && imagetexture->bitmap())
            imagetexture->bitmap()->flatten(buffer);
        ImagesManager::instance()->releaseImage(layer->m_imageCRC);
        stream->write32(buffer.size());
        buffer.writeToStream(stream);
    }
    bool hasRecordingPicture = layer->m_content != 0 && !layer->m_content->isEmpty();
    stream->writeBool(hasRecordingPicture);
    if (hasRecordingPicture)
        layer->m_content->serialize(stream);
    // TODO: support m_animations (maybe?)
    stream->write32(0); // placeholder for m_animations.size();
    writeTransformationMatrix(stream, layer->m_transform);
    writeTransformationMatrix(stream, layer->m_childrenTransform);
    if (type == LTScrollableLayerAndroid) {
        ScrollableLayerAndroid* scrollableLayer =
                static_cast<ScrollableLayerAndroid*>(layer);
        stream->writeScalar(scrollableLayer->m_scrollLimits.fLeft);
        stream->writeScalar(scrollableLayer->m_scrollLimits.fTop);
        stream->writeScalar(scrollableLayer->m_scrollLimits.width());
        stream->writeScalar(scrollableLayer->m_scrollLimits.height());
    }
    int childCount = layer->countChildren();
    stream->write32(childCount);
    for (int i = 0; i < childCount; i++)
        serializeLayer(layer->getChild(i), stream);
}

LayerAndroid* deserializeLayer(int version, SkStream* stream)
{
    int type = stream->readU8();
    if (type == LTNone)
        return 0;
    // Cast is to disambiguate between ctors.
    LayerAndroid *layer;
    if (type == LTLayerAndroid)
        layer = new LayerAndroid((RenderLayer*) 0);
    else if (type == LTScrollableLayerAndroid)
        layer = new ScrollableLayerAndroid((RenderLayer*) 0);
    else {
        ALOGV("Unexpected layer type: %d, aborting!", type);
        return 0;
    }

    // Layer fields
    layer->setShouldInheritFromRootTransform(stream->readBool());
    layer->setOpacity(stream->readScalar());
    layer->setSize(stream->readScalar(), stream->readScalar());
    layer->setPosition(stream->readScalar(), stream->readScalar());
    layer->setAnchorPoint(stream->readScalar(), stream->readScalar());
    layer->setMatrix(readMatrix(stream));
    layer->setChildrenMatrix(readMatrix(stream));

    // LayerAndroid fields
    layer->m_haveClip = stream->readBool();

    // Keep the legacy serialization/deserialization format...
    bool isFixed = stream->readBool();

    layer->m_backgroundColorSet = stream->readBool();

    bool isIframe = stream->readBool();
    // If we are a scrollable layer android, we are an iframe content
    if (isIframe && type == LTScrollableLayerAndroid) {
         IFrameContentLayerAndroid* iframeContent = new IFrameContentLayerAndroid(*layer);
         layer->unref();
         layer = iframeContent;
    } else if (isIframe) { // otherwise we are just the iframe (we use it to compute offset)
         IFrameLayerAndroid* iframe = new IFrameLayerAndroid(*layer);
         layer->unref();
         layer = iframe;
    }

    if (isFixed) {
        FixedPositioning* fixedPosition = new FixedPositioning(layer);

        fixedPosition->m_fixedLeft = readSkLength(stream);
        fixedPosition->m_fixedTop = readSkLength(stream);
        fixedPosition->m_fixedRight = readSkLength(stream);
        fixedPosition->m_fixedBottom = readSkLength(stream);
        fixedPosition->m_fixedMarginLeft = readSkLength(stream);
        fixedPosition->m_fixedMarginTop = readSkLength(stream);
        fixedPosition->m_fixedMarginRight = readSkLength(stream);
        fixedPosition->m_fixedMarginBottom = readSkLength(stream);
        fixedPosition->m_fixedRect = readSkRect(stream);
        fixedPosition->m_renderLayerPos.setX(stream->readS32());
        fixedPosition->m_renderLayerPos.setY(stream->readS32());

        layer->setFixedPosition(fixedPosition);
    } else {
        // Not a fixed element, bypass the values in the stream
        readSkLength(stream); // fixedLeft
        readSkLength(stream); // fixedTop
        readSkLength(stream); // fixedRight
        readSkLength(stream); // fixedBottom
        readSkLength(stream); // fixedMarginLeft
        readSkLength(stream); // fixedMarginTop
        readSkLength(stream); // fixedMarginRight
        readSkLength(stream); // fixedMarginBottom
        readSkRect(stream);   // fixedRect
        stream->readS32();    // renderLayerPos.x()
        stream->readS32();    // renderLayerPos.y()
    }

    layer->m_backfaceVisibility = stream->readBool();
    layer->m_visible = stream->readBool();
    layer->m_backgroundColor = stream->readU32();
    layer->m_preserves3D = stream->readBool();
    layer->m_anchorPointZ = stream->readScalar();
    layer->m_drawOpacity = stream->readScalar();
    bool hasContentsImage = stream->readBool();
    if (hasContentsImage) {
        int size = stream->readU32();
        SkAutoMalloc storage(size);
        stream->read(storage.get(), size);
        SkFlattenableReadBuffer buffer(storage.get(), size);
        SkBitmap contentsImage;
        contentsImage.unflatten(buffer);
        SkBitmapRef* imageRef = new SkBitmapRef(contentsImage);
        layer->setContentsImage(imageRef);
        delete imageRef;
    }
    bool hasRecordingPicture = stream->readBool();
    if (hasRecordingPicture) {
        SkPicture* picture = new SkPicture(stream);
        PictureLayerContent* content = new PictureLayerContent(picture);
        layer->setContent(content);
        SkSafeUnref(content);
        SkSafeUnref(picture);
    }
    int animationCount = stream->readU32(); // TODO: Support (maybe?)
    readTransformationMatrix(stream, layer->m_transform);
    readTransformationMatrix(stream, layer->m_childrenTransform);
    if (type == LTScrollableLayerAndroid) {
        ScrollableLayerAndroid* scrollableLayer =
                static_cast<ScrollableLayerAndroid*>(layer);
        scrollableLayer->m_scrollLimits.set(
                stream->readScalar(),
                stream->readScalar(),
                stream->readScalar(),
                stream->readScalar());
    }
    int childCount = stream->readU32();
    for (int i = 0; i < childCount; i++) {
        LayerAndroid *childLayer = deserializeLayer(version, stream);
        if (childLayer)
            layer->addChild(childLayer);
    }
    ALOGV("Created layer with id %d", layer->uniqueId());
    return layer;
}

/*
 * JNI registration
 */
static JNINativeMethod gSerializerMethods[] = {
    { "nativeDumpLayerHierarchy", "(IILjava/io/OutputStream;[B)V",
        (void*) nativeDumpLayerHierarchy },
    { "nativeSerializeViewState", "(ILjava/io/OutputStream;[B)Z",
        (void*) nativeSerializeViewState },
    { "nativeDeserializeViewState", "(ILjava/io/InputStream;[B)I",
        (void*) nativeDeserializeViewState },
};

int registerViewStateSerializer(JNIEnv* env)
{
    return jniRegisterNativeMethods(env, "android/webkit/ViewStateSerializer",
                                    gSerializerMethods, NELEM(gSerializerMethods));
}

}
