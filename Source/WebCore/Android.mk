##
## Copyright 2009, The Android Open Source Project
## Copyright (C) 2011, 2012, Sony Ericsson Mobile Communications AB
## Copyright (C) 2012 Sony Mobile Communications AB
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##

LOCAL_SRC_FILES := \
	bindings/generic/ActiveDOMCallback.cpp \
	bindings/generic/BindingSecurityBase.cpp \
	bindings/generic/RuntimeEnabledFeatures.cpp \
	\
	css/CSSBorderImageValue.cpp \
	css/CSSCanvasValue.cpp \
	css/CSSCharsetRule.cpp \
	css/CSSComputedStyleDeclaration.cpp \
	css/CSSCursorImageValue.cpp \
	css/CSSFontFace.cpp \
	css/CSSFontFaceRule.cpp \
	css/CSSFontFaceSource.cpp \
	css/CSSFontFaceSrcValue.cpp \
	css/CSSFontSelector.cpp \
	css/CSSFunctionValue.cpp \
	css/CSSGradientValue.cpp \
	css/CSSImageGeneratorValue.cpp \
	css/CSSImageValue.cpp \
	css/CSSImportRule.cpp \
	css/CSSInheritedValue.cpp \
	css/CSSInitialValue.cpp \
	css/CSSLineBoxContainValue.cpp \
	css/CSSMediaRule.cpp \
	css/CSSMutableStyleDeclaration.cpp \
	css/CSSOMUtils.cpp \
	css/CSSPageRule.cpp \
	css/CSSParser.cpp \
	css/CSSParserValues.cpp \
	css/CSSPrimitiveValue.cpp \
	css/CSSPrimitiveValueCache.cpp \
	css/CSSProperty.cpp \
	css/CSSPropertyLonghand.cpp \
	css/CSSPropertySourceData.cpp \
	css/CSSReflectValue.cpp \
	css/CSSRule.cpp \
	css/CSSRuleList.cpp \
	css/CSSSegmentedFontFace.cpp \
	css/CSSSelector.cpp \
	css/CSSSelectorList.cpp \
	css/CSSStyleApplyProperty.cpp \
	css/CSSStyleDeclaration.cpp \
	css/CSSStyleRule.cpp \
	css/CSSStyleSelector.cpp \
	css/CSSStyleSheet.cpp \
	css/CSSTimingFunctionValue.cpp \
	css/CSSUnicodeRangeValue.cpp \
	css/CSSValueList.cpp \
	css/FontFamilyValue.cpp \
	css/FontValue.cpp \
	css/MediaFeatureNames.cpp \
	css/MediaList.cpp \
	css/MediaQuery.cpp \
	css/MediaQueryEvaluator.cpp \
	css/MediaQueryExp.cpp \
	css/MediaQueryList.cpp \
	css/MediaQueryListListener.cpp \
	css/MediaQueryMatcher.cpp \
	css/RGBColor.cpp \

ifeq ($(ENABLE_SVG), true)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	css/SVGCSSComputedStyleDeclaration.cpp \
	css/SVGCSSParser.cpp \
	css/SVGCSSStyleSelector.cpp
endif

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	css/ShadowValue.cpp \
	css/StyleBase.cpp \
	css/StyleList.cpp \
	css/StyleMedia.cpp \
	css/StyleSheet.cpp \
	css/StyleSheetList.cpp \
	css/WebKitCSSKeyframeRule.cpp \
	css/WebKitCSSKeyframesRule.cpp \
	css/WebKitCSSMatrix.cpp \
	css/WebKitCSSTransformValue.cpp \
	\
	dom/ActiveDOMObject.cpp \
	dom/Attr.cpp \
	dom/Attribute.cpp \
	dom/BeforeTextInsertedEvent.cpp \
	dom/BeforeUnloadEvent.cpp \
	dom/CDATASection.cpp \
	dom/CSSMappedAttributeDeclaration.cpp \
	dom/CharacterData.cpp \
	dom/CheckedRadioButtons.cpp \
	dom/ChildNodeList.cpp \
	dom/ClassNodeList.cpp \
	dom/ClientRect.cpp \
	dom/ClientRectList.cpp \
	dom/Clipboard.cpp \
	dom/ClipboardEvent.cpp \
	dom/Comment.cpp \
	dom/CompositionEvent.cpp \
	dom/ContainerNode.cpp \
	dom/CustomEvent.cpp \
	dom/DOMImplementation.cpp \
	dom/DOMStringList.cpp \
	dom/DOMStringMap.cpp \
	dom/DOMTextContentWalker.cpp \
	dom/DatasetDOMStringMap.cpp \
	dom/DecodedDataDocumentParser.cpp \
	dom/DeviceMotionController.cpp \
	dom/DeviceMotionData.cpp \
	dom/DeviceMotionEvent.cpp \
	dom/Document.cpp \
	dom/DocumentFragment.cpp \
	dom/DocumentMarkerController.cpp \
	dom/DocumentParser.cpp \
	dom/DocumentOrderedMap.cpp \
	dom/DocumentType.cpp \
	dom/DynamicNodeList.cpp \
	dom/EditingText.cpp \
	dom/Element.cpp \
	dom/EntityReference.cpp \
	dom/ErrorEvent.cpp \
	dom/Event.cpp \
	dom/EventContext.cpp \
	dom/EventDispatcher.cpp \
	dom/EventNames.cpp \
	dom/EventTarget.cpp \
	dom/EventQueue.cpp \
	dom/ExceptionBase.cpp \
	dom/ExceptionCode.cpp \
	dom/InputElement.cpp \
	dom/KeyboardEvent.cpp \
	dom/MessageChannel.cpp \
	dom/MessageEvent.cpp \
	dom/MessagePort.cpp \
	dom/MessagePortChannel.cpp \
	dom/MouseEvent.cpp \
	dom/MouseRelatedEvent.cpp \
	dom/MutationEvent.cpp \
	dom/NameNodeList.cpp \
	dom/NamedNodeMap.cpp \
	dom/Node.cpp \
	dom/NodeFilter.cpp \
	dom/NodeFilterCondition.cpp \
	dom/NodeIterator.cpp \
	dom/Notation.cpp \
	dom/OptionElement.cpp \
	dom/OptionGroupElement.cpp \
	dom/DeviceOrientation.cpp \
	dom/DeviceOrientationController.cpp \
	dom/DeviceOrientationEvent.cpp \
	dom/OverflowEvent.cpp \
	dom/PageTransitionEvent.cpp \
	dom/PendingScript.cpp \
	dom/PopStateEvent.cpp \
	dom/Position.cpp \
	dom/PositionIterator.cpp \
	dom/ProcessingInstruction.cpp \
	dom/ProgressEvent.cpp \
	dom/QualifiedName.cpp \
	dom/Range.cpp \
	dom/RegisteredEventListener.cpp \
	dom/ScopedEventQueue.cpp \
	dom/ScriptedAnimationController.cpp \
	dom/ScriptableDocumentParser.cpp \
	dom/ScriptElement.cpp \
	dom/ScriptExecutionContext.cpp \
	dom/ScriptRunner.cpp \
	dom/SelectElement.cpp \
	dom/SelectorNodeList.cpp \
	dom/ShadowRoot.cpp \
	dom/SpaceSplitString.cpp \
	dom/StaticHashSetNodeList.cpp \
	dom/StaticNodeList.cpp \
	dom/StringCallback.cpp \
	dom/StyleElement.cpp \
	dom/StyledElement.cpp \
	dom/TagNodeList.cpp \
	dom/Text.cpp \
	dom/TextEvent.cpp \
	dom/Touch.cpp \
	dom/TouchEvent.cpp \
	dom/TouchList.cpp \
	dom/Traversal.cpp \
	dom/TreeScope.cpp \
	dom/TreeWalker.cpp \
	dom/UIEvent.cpp \
	dom/UIEventWithKeyState.cpp \
	dom/UserGestureIndicator.cpp \
	dom/UserTypingGestureIndicator.cpp \
	dom/ViewportArguments.cpp \
	dom/WebKitAnimationEvent.cpp \
	dom/WebKitTransitionEvent.cpp \
	dom/WheelEvent.cpp \
	dom/WindowEventContext.cpp \
	dom/XMLDocumentParser.cpp \
	dom/XMLDocumentParserLibxml2.cpp \
	dom/XMLDocumentParserScope.cpp \
	dom/default/PlatformMessagePortChannel.cpp \
	\
	editing/AppendNodeCommand.cpp \
	editing/ApplyBlockElementCommand.cpp \
	editing/ApplyStyleCommand.cpp \
	editing/BreakBlockquoteCommand.cpp \
	editing/CompositeEditCommand.cpp \
	editing/CreateLinkCommand.cpp \
	editing/DeleteButton.cpp \
	editing/DeleteButtonController.cpp \
	editing/DeleteFromTextNodeCommand.cpp \
	editing/DeleteSelectionCommand.cpp \
	editing/EditCommand.cpp \
	editing/EditingStyle.cpp \
	editing/Editor.cpp \
	editing/EditorCommand.cpp \
	editing/FormatBlockCommand.cpp \
	editing/HTMLInterchange.cpp \
	editing/IndentOutdentCommand.cpp \
	editing/InsertIntoTextNodeCommand.cpp \
	editing/InsertLineBreakCommand.cpp \
	editing/InsertListCommand.cpp \
	editing/InsertNodeBeforeCommand.cpp \
	editing/InsertParagraphSeparatorCommand.cpp \
	editing/InsertTextCommand.cpp \
	editing/JoinTextNodesCommand.cpp \
	editing/MarkupAccumulator.cpp \
	editing/MergeIdenticalElementsCommand.cpp \
	editing/ModifySelectionListLevel.cpp \
	editing/MoveSelectionCommand.cpp \
	editing/RemoveCSSPropertyCommand.cpp \
	editing/RemoveFormatCommand.cpp \
	editing/RemoveNodeCommand.cpp \
	editing/RemoveNodePreservingChildrenCommand.cpp \
	editing/ReplaceNodeWithSpanCommand.cpp \
	editing/ReplaceSelectionCommand.cpp \
	editing/SelectionController.cpp \
	editing/SetNodeAttributeCommand.cpp \
	editing/SetSelectionCommand.cpp \
	editing/SpellChecker.cpp \
	editing/SpellingCorrectionCommand.cpp \
	editing/SplitElementCommand.cpp \
	editing/SplitTextNodeCommand.cpp \
	editing/SplitTextNodeContainingElementCommand.cpp \
	editing/TextCheckingHelper.cpp \
	editing/TextIterator.cpp \
	editing/TypingCommand.cpp \
	editing/UnlinkCommand.cpp \
	editing/VisiblePosition.cpp \
	editing/VisibleSelection.cpp \
	editing/WrapContentsInDummySpanCommand.cpp \
	\
	editing/android/EditorAndroid.cpp \
	editing/htmlediting.cpp \
	editing/markup.cpp \
	editing/visible_units.cpp \
	\
	fileapi/Blob.cpp \
	fileapi/BlobURL.cpp \
	fileapi/DOMFileSystemBase.cpp \
	fileapi/DOMFileSystemSync.cpp \
	fileapi/DirectoryEntrySync.cpp \
	fileapi/DirectoryReaderSync.cpp \
	fileapi/EntryArraySync.cpp \
	fileapi/EntryBase.cpp \
	fileapi/EntrySync.cpp \
	fileapi/File.cpp \
	fileapi/FileEntrySync.cpp \
	fileapi/FileList.cpp \
	fileapi/FileReader.cpp \
	fileapi/FileReaderLoader.cpp \
	fileapi/FileStreamProxy.cpp \
	fileapi/FileThread.cpp \
	fileapi/ThreadableBlobRegistry.cpp \
	fileapi/WebKitBlobBuilder.cpp \
	\
	history/BackForwardController.cpp \
	history/BackForwardListImpl.cpp \
	history/CachedFrame.cpp \
	history/CachedPage.cpp \
	history/HistoryItem.cpp \
	history/PageCache.cpp \
	\
	history/android/HistoryItemAndroid.cpp \
	\
	html/AsyncImageResizer.cpp \
	html/BaseButtonInputType.cpp \
	html/BaseCheckableInputType.cpp \
	html/BaseDateAndTimeInputType.cpp \
	html/BaseTextInputType.cpp \
	html/ButtonInputType.cpp \
	html/CheckboxInputType.cpp \
	html/ClassList.cpp \
	html/CollectionCache.cpp \
	html/parser/CSSPreloadScanner.cpp \
	html/ColorInputType.cpp \
	html/DOMFormData.cpp \
	html/DOMSettableTokenList.cpp \
	html/DOMTokenList.cpp \
	html/DOMURL.cpp \
	html/DateComponents.cpp \
	html/DateInputType.cpp \
	html/DateTimeInputType.cpp \
	html/DateTimeLocalInputType.cpp \
	html/EmailInputType.cpp \
	html/FileInputType.cpp \
	html/FormAssociatedElement.cpp \
	html/FormDataList.cpp \
	html/HTMLAllCollection.cpp \
	html/HTMLCollection.cpp \
	html/HTMLDataListElement.cpp \
	html/HTMLDetailsElement.cpp \
	html/HTMLDocument.cpp \
	html/HTMLElementsAllInOne.cpp \
	html/HTMLFormCollection.cpp \
	html/HTMLImageLoader.cpp \
	html/HTMLNameCollection.cpp \
	html/HTMLOptionsCollection.cpp \
	html/HTMLOutputElement.cpp \
	html/HTMLParserErrorCodes.cpp \
	html/HTMLTableRowsCollection.cpp \
	html/HTMLViewSourceDocument.cpp \
	html/HTMLWbrElement.cpp \
	html/HiddenInputType.cpp \
	html/ImageData.cpp \
	html/ImageDocument.cpp \
	html/ImageInputType.cpp \
	html/ImageResizerThread.cpp \
	html/InputType.cpp \
	html/IsIndexInputType.cpp \
	html/LabelsNodeList.cpp \
	html/MediaDocument.cpp \
	html/MonthInputType.cpp \
	html/NumberInputType.cpp \
	html/PasswordInputType.cpp \
	html/PluginDocument.cpp \
	html/RadioInputType.cpp \
	html/RangeInputType.cpp \
	html/ResetInputType.cpp \
	html/SearchInputType.cpp \
	html/StepRange.cpp \
	html/SubmitInputType.cpp \
	html/TelephoneInputType.cpp \
	html/TextDocument.cpp \
	html/TextFieldInputType.cpp \
	html/TextInputType.cpp \
	html/TimeInputType.cpp \
	html/TimeRanges.cpp \
	html/URLInputType.cpp \
	html/ValidationMessage.cpp \
	html/ValidityState.cpp \
	html/WeekInputType.cpp \
	\
	html/canvas/ArrayBuffer.cpp \
	html/canvas/ArrayBufferView.cpp \
	html/canvas/CanvasGradient.cpp \
	html/canvas/CanvasPattern.cpp \
	html/canvas/CanvasPixelArray.cpp \
	html/canvas/CanvasRenderingContext.cpp \
	html/canvas/CanvasRenderingContext2D.cpp \
	html/canvas/CanvasStyle.cpp \
	html/canvas/DataView.cpp \
	html/canvas/Float32Array.cpp \
	html/canvas/Float64Array.cpp \
	html/canvas/Int16Array.cpp \
	html/canvas/Int32Array.cpp \
	html/canvas/Int8Array.cpp \
	html/canvas/OESTextureFloat.cpp \
	html/canvas/OESVertexArrayObject.cpp \
	html/canvas/Uint16Array.cpp \
	html/canvas/Uint32Array.cpp \
	html/canvas/Uint8Array.cpp \
	html/canvas/Uint8ClampedArray.cpp \
	html/canvas/WebGLExtension.cpp \
	html/canvas/WebGLObject.cpp \
	html/canvas/WebGLVertexArrayObjectOES.cpp

ifeq ($(ENABLE_WEBGL), true)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	html/canvas/CanvasContextAttributes.cpp \
	html/canvas/OESStandardDerivatives.cpp \
	html/canvas/WebGLBuffer.cpp \
	html/canvas/WebGLContextAttributes.cpp \
	html/canvas/WebGLContextEvent.cpp \
	html/canvas/WebGLFramebuffer.cpp \
	html/canvas/WebGLGetInfo.cpp \
	html/canvas/WebGLProgram.cpp \
	html/canvas/WebGLRenderbuffer.cpp \
	html/canvas/WebGLRenderingContext.cpp \
	html/canvas/WebGLShader.cpp \
	html/canvas/WebGLTexture.cpp \
	html/canvas/WebGLUniformLocation.cpp \
	html/canvas/WebKitLoseContext.cpp
endif

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	html/parser/HTMLConstructionSite.cpp \
	html/parser/HTMLDocumentParser.cpp \
	html/parser/HTMLElementStack.cpp \
	html/parser/HTMLEntityParser.cpp \
	html/parser/HTMLEntitySearch.cpp \
	html/parser/HTMLFormattingElementList.cpp \
	html/parser/HTMLMetaCharsetParser.cpp \
	html/parser/HTMLParserIdioms.cpp \
	html/parser/HTMLParserScheduler.cpp \
	html/parser/HTMLPreloadScanner.cpp \
	html/parser/HTMLScriptRunner.cpp \
	html/parser/HTMLSourceTracker.cpp \
	html/parser/HTMLTokenizer.cpp \
	html/parser/HTMLTreeBuilder.cpp \
	html/parser/HTMLViewSourceParser.cpp \
	html/parser/TextDocumentParser.cpp \
	html/parser/TextViewSourceParser.cpp \
	html/parser/XSSFilter.cpp \
	\
	html/shadow/DetailsMarkerControl.cpp \
	html/shadow/MediaControls.cpp \
	html/shadow/MediaControlElements.cpp \
	html/shadow/MediaControlRootElement.cpp \
	html/shadow/MeterShadowElement.cpp \
	html/shadow/ProgressShadowElement.cpp \
	html/shadow/SliderThumbElement.cpp \
	html/shadow/TextControlInnerElements.cpp \
	\
	inspector/ScriptArguments.cpp \
	inspector/ScriptCallStack.cpp \
	inspector/ScriptCallFrame.cpp \
	\
	loader/cache/CachedCSSStyleSheet.cpp \
	loader/cache/CachedFont.cpp \
	loader/cache/CachedImage.cpp \
	loader/cache/CachedResource.cpp \
	loader/cache/CachedResourceClientWalker.cpp \
	loader/cache/CachedResourceHandle.cpp \
	loader/cache/CachedResourceLoader.cpp \
	loader/cache/CachedResourceRequest.cpp \
	loader/cache/CachedScript.cpp \
	loader/CrossOriginAccessControl.cpp \
	loader/CrossOriginPreflightResultCache.cpp \
	loader/DocumentLoader.cpp \
	loader/DocumentThreadableLoader.cpp \
	loader/DocumentWriter.cpp \
	loader/FormState.cpp \
	loader/FormSubmission.cpp \
	loader/FrameLoader.cpp \
	loader/FrameLoaderStateMachine.cpp \
	loader/HistoryController.cpp \
	loader/ImageLoader.cpp \
	loader/MainResourceLoader.cpp \
	loader/NavigationAction.cpp \
	loader/NavigationScheduler.cpp \
	loader/NetscapePlugInStreamLoader.cpp \
	loader/PingLoader.cpp \
	loader/PlaceholderDocument.cpp \
	loader/PolicyCallback.cpp \
	loader/PolicyChecker.cpp \
	loader/ProgressTracker.cpp \
	loader/ResourceLoadNotifier.cpp \
	loader/ResourceLoadScheduler.cpp \
	loader/ResourceLoader.cpp \
	loader/SinkDocument.cpp \
	loader/SubframeLoader.cpp \
	loader/SubresourceLoader.cpp \
	loader/TextResourceDecoder.cpp \
	loader/ThreadableLoader.cpp \
	loader/WorkerThreadableLoader.cpp \
	loader/appcache/ApplicationCache.cpp \
	loader/appcache/ApplicationCacheGroup.cpp \
	loader/appcache/ApplicationCacheHost.cpp \
	loader/appcache/ApplicationCacheResource.cpp \
	loader/appcache/ApplicationCacheStorage.cpp \
	loader/appcache/DOMApplicationCache.cpp \
	loader/appcache/ManifestParser.cpp \
	\
	loader/cache/MemoryCache.cpp \
	\
	loader/icon/IconDatabase.cpp \
	loader/icon/IconDatabaseBase.cpp \
	loader/icon/IconLoader.cpp \
	loader/icon/IconRecord.cpp \
	loader/icon/PageURLRecord.cpp \
	\
	page/BarInfo.cpp \
	page/Chrome.cpp \
	page/Connection.cpp \
	page/Console.cpp \
	page/ContentSecurityPolicy.cpp \
	page/ContextMenuController.cpp \
	page/Crypto.cpp \
	page/DOMSelection.cpp \
	page/DOMTimer.cpp \
	page/DOMWindow.cpp \
	page/DragController.cpp \
	page/EventHandler.cpp \
	page/FocusController.cpp \
	page/Frame.cpp \
	page/FrameActionScheduler.cpp \
	page/FrameTree.cpp \
	page/FrameView.cpp \
	page/Geolocation.cpp \
	page/GeolocationController.cpp \
	page/GeolocationPositionCache.cpp \
	page/GroupSettings.cpp \
	page/History.cpp \
	page/Location.cpp \
	page/MemoryInfo.cpp \
	page/MouseEventWithHitTestResults.cpp \
	page/Navigator.cpp \
	page/NavigatorBase.cpp \
	page/OriginAccessEntry.cpp \
	page/Page.cpp \
	page/PageGroup.cpp \
	page/PageGroupLoadDeferrer.cpp \
	page/Performance.cpp \
	page/PerformanceNavigation.cpp \
	page/PerformanceTiming.cpp \
	page/PluginHalter.cpp \
	page/PrintContext.cpp \
	page/Screen.cpp \
	page/SecurityOrigin.cpp \
	page/Settings.cpp \
	page/SpatialNavigation.cpp \
	page/SpeechInput.cpp \
	page/SpeechInputResult.cpp \
	page/SuspendableTimer.cpp \
	page/UserContentURLPattern.cpp \
	page/WebKitAnimation.cpp \
	page/WebKitAnimationList.cpp \
	page/WindowFeatures.cpp \
	page/WorkerNavigator.cpp \
	\
	page/android/DragControllerAndroid.cpp \
	page/android/EventHandlerAndroid.cpp \
	\
	page/animation/AnimationBase.cpp \
	page/animation/AnimationController.cpp \
	page/animation/CompositeAnimation.cpp \
	page/animation/ImplicitAnimation.cpp \
	page/animation/KeyframeAnimation.cpp \
	\
	platform/Arena.cpp \
	platform/ContentType.cpp \
	platform/ContextMenuItem.cpp \
	platform/CrossThreadCopier.cpp \
	platform/Cursor.cpp \
	platform/DragData.cpp \
	platform/DragImage.cpp \
	platform/FileChooser.cpp \
	platform/FileStream.cpp \
	platform/FileSystem.cpp \
	platform/KURL.cpp \
	platform/KURLGoogle.cpp \
	platform/KillRingNone.cpp \
	platform/Language.cpp \
	platform/Length.cpp \
	platform/LinkHash.cpp \
	platform/LocalizedStrings.cpp \
	platform/Logging.cpp \
	platform/MIMETypeRegistry.cpp \
	platform/RuntimeApplicationChecks.cpp \
	platform/SchemeRegistry.cpp \
	platform/ScrollAnimator.cpp \
	platform/ScrollView.cpp \
	platform/Scrollbar.cpp \
	platform/ScrollableArea.cpp \
	platform/ScrollbarThemeComposite.cpp \
	platform/SharedBuffer.cpp \
	platform/Theme.cpp \
	platform/ThreadGlobalData.cpp \
	platform/ThreadTimers.cpp \
	platform/Timer.cpp \
	platform/UUID.cpp \
	platform/Widget.cpp \
	\
	platform/android/ClipboardAndroid.cpp \
	platform/android/CursorAndroid.cpp \
	platform/android/DragDataAndroid.cpp \
	platform/android/EventLoopAndroid.cpp \
	platform/android/FileChooserAndroid.cpp \
	platform/android/FileSystemAndroid.cpp \
	platform/android/KeyEventAndroid.cpp \
	platform/android/LanguageAndroid.cpp \
	platform/android/LocalizedStringsAndroid.cpp \
	platform/android/PlatformTouchEventAndroid.cpp \
	platform/android/PlatformTouchPointAndroid.cpp \
	platform/android/PopupMenuAndroid.cpp \
	platform/android/RenderThemeAndroid.cpp \
	platform/android/PackageNotifier.cpp \
	platform/android/ScreenAndroid.cpp \
	platform/android/ScrollViewAndroid.cpp \
	platform/android/SharedTimerAndroid.cpp \
	platform/android/SoundAndroid.cpp \
	platform/android/SSLKeyGeneratorAndroid.cpp \
	platform/android/SystemTimeAndroid.cpp \
	platform/android/TemporaryLinkStubs.cpp \
	platform/android/WidgetAndroid.cpp \
	\
	platform/animation/Animation.cpp \
	platform/animation/AnimationList.cpp \
	\
	platform/audio/mkl/FFTFrameMKL.cpp \
	\
	platform/graphics/BitmapImage.cpp \
	platform/graphics/Color.cpp \
	platform/graphics/FloatPoint.cpp \
	platform/graphics/FloatPoint3D.cpp \
	platform/graphics/FloatQuad.cpp \
	platform/graphics/FloatRect.cpp \
	platform/graphics/FloatSize.cpp \
	platform/graphics/Font.cpp \
	platform/graphics/FontCache.cpp \
	platform/graphics/FontData.cpp \
	platform/graphics/FontDescription.cpp \
	platform/graphics/FontFallbackList.cpp \
	platform/graphics/FontFamily.cpp \
	platform/graphics/FontFastPath.cpp \
	platform/graphics/GeneratedImage.cpp \
	platform/graphics/GlyphPageTreeNode.cpp \
	platform/graphics/Gradient.cpp \
	platform/graphics/GraphicsContext.cpp \
	platform/graphics/GraphicsLayer.cpp \
	platform/graphics/GraphicsTypes.cpp \
	platform/graphics/Image.cpp \
	platform/graphics/ImageBuffer.cpp \
	platform/graphics/IntRect.cpp \
	platform/graphics/MediaPlayer.cpp \
	platform/graphics/Path.cpp \
	platform/graphics/PathTraversalState.cpp \
	platform/graphics/Pattern.cpp \
	platform/graphics/RoundedIntRect.cpp \
	platform/graphics/SegmentedFontData.cpp \
	platform/graphics/SimpleFontData.cpp \
	platform/graphics/StringTruncator.cpp \
	platform/graphics/WidthIterator.cpp \
	\
	platform/graphics/android/BitmapAllocatorAndroid.cpp \
	platform/graphics/android/GraphicsLayerAndroid.cpp \
	platform/graphics/android/GLWebViewState.cpp \
	platform/graphics/android/ImageAndroid.cpp \
	platform/graphics/android/ImageBufferAndroid.cpp \
	platform/graphics/android/ImageSourceAndroid.cpp \
	platform/graphics/android/PathAndroid.cpp \
	platform/graphics/android/PatternAndroid.cpp \
	platform/graphics/android/SharedBufferStream.cpp \
	\
	platform/graphics/android/context/GraphicsContextAndroid.cpp \
	platform/graphics/android/context/GraphicsOperation.cpp \
	platform/graphics/android/context/PlatformGraphicsContext.cpp \
	platform/graphics/android/context/PlatformGraphicsContextRecording.cpp \
	platform/graphics/android/context/PlatformGraphicsContextSkia.cpp \
	platform/graphics/android/context/RecordingContextCanvasProxy.cpp \
	platform/graphics/android/context/RTree.cpp \
	\
	platform/graphics/android/fonts/FontAndroid.cpp \
	platform/graphics/android/fonts/FontCacheAndroid.cpp \
	platform/graphics/android/fonts/FontCustomPlatformData.cpp \
	platform/graphics/android/fonts/FontDataAndroid.cpp \
	platform/graphics/android/fonts/FontPlatformDataAndroid.cpp \
	platform/graphics/android/fonts/GlyphMapAndroid.cpp \
	platform/graphics/android/fonts/VerticalTextMap.cpp \
	\
	platform/graphics/android/layers/AndroidAnimation.cpp \
	platform/graphics/android/layers/BaseLayerAndroid.cpp \
	platform/graphics/android/layers/CanvasLayer.cpp \
	platform/graphics/android/layers/CanvasTexture.cpp \
	platform/graphics/android/layers/DumpLayer.cpp \
	platform/graphics/android/layers/FixedPositioning.cpp \
	platform/graphics/android/layers/IFrameContentLayerAndroid.cpp \
	platform/graphics/android/layers/IFrameLayerAndroid.cpp \
	platform/graphics/android/layers/Layer.cpp \
	platform/graphics/android/layers/LayerAndroid.cpp \
	platform/graphics/android/layers/MediaLayer.cpp \
	platform/graphics/android/layers/MediaTexture.cpp \
	platform/graphics/android/layers/PictureLayerContent.cpp \
	platform/graphics/android/layers/PicturePileLayerContent.cpp \
	platform/graphics/android/layers/ScrollableLayerAndroid.cpp \
	platform/graphics/android/layers/VideoLayerAndroid.cpp \
	platform/graphics/android/layers/VideoLayerManager.cpp \
	\
	platform/graphics/android/rendering/BaseRenderer.cpp \
	platform/graphics/android/rendering/GaneshContext.cpp \
	platform/graphics/android/rendering/GaneshRenderer.cpp \
	platform/graphics/android/rendering/GLExtras.cpp \
	platform/graphics/android/rendering/GLUtils.cpp \
	platform/graphics/android/rendering/ImagesManager.cpp \
	platform/graphics/android/rendering/ImageTexture.cpp \
	platform/graphics/android/rendering/InspectorCanvas.cpp \
	platform/graphics/android/rendering/PaintTileOperation.cpp \
	platform/graphics/android/rendering/RasterRenderer.cpp \
	platform/graphics/android/rendering/ShaderProgram.cpp \
	platform/graphics/android/rendering/Surface.cpp \
	platform/graphics/android/rendering/SurfaceBacking.cpp \
	platform/graphics/android/rendering/SurfaceCollection.cpp \
	platform/graphics/android/rendering/SurfaceCollectionManager.cpp \
	platform/graphics/android/rendering/TextureInfo.cpp \
	platform/graphics/android/rendering/TexturesGenerator.cpp \
	platform/graphics/android/rendering/Tile.cpp \
	platform/graphics/android/rendering/TileGrid.cpp \
	platform/graphics/android/rendering/TileTexture.cpp \
	platform/graphics/android/rendering/TilesManager.cpp \
	platform/graphics/android/rendering/TilesProfiler.cpp \
	platform/graphics/android/rendering/TransferQueue.cpp \
	\
	platform/graphics/android/utils/ClassTracker.cpp \
	platform/graphics/android/utils/LinearAllocator.cpp

ifeq ($(ENABLE_WEBGL), true)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	platform/graphics/ANGLEWebKitBridge.cpp \
	platform/graphics/GraphicsContext3D.cpp \
	platform/graphics/android/Extensions3DAndroid.cpp \
	platform/graphics/android/GraphicsContext3DAndroid.cpp \
	platform/graphics/android/GraphicsContext3DInternal.cpp \
	platform/graphics/android/GraphicsContext3DProxy.cpp \
	platform/graphics/android/WebGLLayer.cpp \
	platform/image-decoders/png/PNGImageDecoder.cpp
endif

ifeq ($(ENABLE_SVG), true)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	platform/graphics/filters/DistantLightSource.cpp \
	platform/graphics/filters/FEBlend.cpp \
	platform/graphics/filters/FEColorMatrix.cpp \
	platform/graphics/filters/FEComponentTransfer.cpp \
	platform/graphics/filters/FEComposite.cpp \
	platform/graphics/filters/FEConvolveMatrix.cpp \
	platform/graphics/filters/FEDiffuseLighting.cpp \
	platform/graphics/filters/FEDisplacementMap.cpp \
	platform/graphics/filters/FEFlood.cpp \
	platform/graphics/filters/FEMerge.cpp \
	platform/graphics/filters/FEMorphology.cpp \
	platform/graphics/filters/FEOffset.cpp \
	platform/graphics/filters/FESpecularLighting.cpp \
	platform/graphics/filters/FETile.cpp \
	platform/graphics/filters/FETurbulence.cpp \
	platform/graphics/filters/LightSource.cpp \
	platform/graphics/filters/PointLightSource.cpp \
	platform/graphics/filters/SpotLightSource.cpp
endif

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	platform/graphics/skia/FloatPointSkia.cpp \
	platform/graphics/skia/FloatRectSkia.cpp \
	platform/graphics/skia/GlyphPageTreeNodeSkia.cpp \
	platform/graphics/skia/GradientSkia.cpp \
	platform/graphics/skia/IntPointSkia.cpp \
	platform/graphics/skia/IntRectSkia.cpp \
	platform/graphics/skia/NativeImageSkia.cpp \
	platform/graphics/skia/SkiaUtils.cpp \
	platform/graphics/skia/TransformationMatrixSkia.cpp \
	\
	platform/graphics/transforms/AffineTransform.cpp \
	platform/graphics/transforms/Matrix3DTransformOperation.cpp \
	platform/graphics/transforms/MatrixTransformOperation.cpp \
	platform/graphics/transforms/PerspectiveTransformOperation.cpp \
	platform/graphics/transforms/RotateTransformOperation.cpp \
	platform/graphics/transforms/ScaleTransformOperation.cpp \
	platform/graphics/transforms/SkewTransformOperation.cpp \
	platform/graphics/transforms/TransformOperations.cpp \
	platform/graphics/transforms/TransformationMatrix.cpp \
	platform/graphics/transforms/TranslateTransformOperation.cpp \
	\
	platform/image-decoders/ImageDecoder.cpp \
	platform/image-decoders/skia/ImageDecoderSkia.cpp \
	platform/image-decoders/gif/GIFImageDecoder.cpp \
	platform/image-decoders/gif/GIFImageReader.cpp \
	\
	platform/image-encoders/skia/JPEGImageEncoder.cpp \
	platform/image-encoders/skia/PNGImageEncoder.cpp \
	\
	platform/leveldb/LevelDBDatabase.cpp \
	platform/leveldb/LevelDBIterator.cpp \
	\
	platform/mock/DeviceOrientationClientMock.cpp \
	platform/mock/GeolocationClientMock.cpp \
	platform/mock/SpeechInputClientMock.cpp \
	\
	platform/network/AuthenticationChallengeBase.cpp \
	platform/network/BlobData.cpp \
	platform/network/BlobRegistryImpl.cpp \
	platform/network/BlobResourceHandle.cpp \
	platform/network/Credential.cpp \
	platform/network/CredentialStorage.cpp \
	platform/network/DataURL.cpp \
	platform/network/FormData.cpp \
	platform/network/FormDataBuilder.cpp \
	platform/network/HTTPHeaderMap.cpp \
	platform/network/HTTPParsers.cpp \
	platform/network/NetworkStateNotifier.cpp \
	platform/network/ProtectionSpace.cpp \
	platform/network/ProxyServer.cpp \
	platform/network/ResourceErrorBase.cpp \
	platform/network/ResourceHandle.cpp \
	platform/network/ResourceRequestBase.cpp \
	platform/network/ResourceResponseBase.cpp \
	\
	platform/network/android/CookieJarAndroid.cpp \
	platform/network/android/ProxyServerAndroid.cpp \
	platform/network/android/ResourceHandleAndroid.cpp \
	platform/network/android/ResourceRequestAndroid.cpp \
	\
	platform/posix/FileSystemPOSIX.cpp \
	\
	platform/sql/SQLValue.cpp \
	platform/sql/SQLiteAuthorizer.cpp \
	platform/sql/SQLiteDatabase.cpp \
	platform/sql/SQLiteFileSystem.cpp \
	platform/sql/SQLiteStatement.cpp \
	platform/sql/SQLiteTransaction.cpp \
	\
	platform/text/Base64.cpp \
	platform/text/BidiContext.cpp \
	platform/text/Hyphenation.cpp \
	platform/text/LineEnding.cpp \
	platform/text/LocalizedNumberNone.cpp \
	platform/text/RegularExpression.cpp \
	platform/text/SegmentedString.cpp \
	platform/text/String.cpp \
	platform/text/TextBoundaries.cpp \
	platform/text/TextBreakIteratorICU.cpp \
	platform/text/TextCodec.cpp \
	platform/text/TextCodecICU.cpp \
	platform/text/TextCodecLatin1.cpp \
	platform/text/TextCodecUTF16.cpp \
	platform/text/TextCodecUTF8.cpp \
	platform/text/TextCodecUserDefined.cpp \
	platform/text/TextEncoding.cpp \
	platform/text/TextEncodingDetectorICU.cpp \
	platform/text/TextEncodingRegistry.cpp \
	platform/text/TextStream.cpp \
	platform/text/UnicodeRange.cpp \
	\
	platform/text/android/HyphenationAndroid.cpp \
	platform/text/android/TextBreakIteratorInternalICU.cpp \
	\
	platform/text/transcoder/FontTranscoder.cpp \
	\
	plugins/DOMMimeType.cpp \
	plugins/DOMMimeTypeArray.cpp \
	plugins/DOMPlugin.cpp \
	plugins/DOMPluginArray.cpp \
	plugins/PluginData.cpp \
	plugins/PluginDatabase.cpp \
	plugins/PluginDebug.cpp \
	plugins/PluginMainThreadScheduler.cpp \
	plugins/PluginPackage.cpp \
	plugins/PluginStream.cpp \
	plugins/PluginView.cpp \
	plugins/npapi.cpp \
	\
	plugins/android/PluginDataAndroid.cpp \
	plugins/android/PluginPackageAndroid.cpp \
	plugins/android/PluginViewAndroid.cpp \
	\
	rendering/AutoTableLayout.cpp \
	rendering/BidiRun.cpp \
	rendering/CounterNode.cpp \
	rendering/EllipsisBox.cpp \
	rendering/FixedTableLayout.cpp \
	rendering/HitTestResult.cpp \
	rendering/InlineBox.cpp \
	rendering/InlineFlowBox.cpp \
	rendering/InlineTextBox.cpp \
	rendering/LayoutState.cpp \
	rendering/PointerEventsHitRules.cpp \
	rendering/RenderApplet.cpp \
	rendering/RenderArena.cpp \
	rendering/RenderBR.cpp \
	rendering/RenderBlock.cpp \
	rendering/RenderBlockLineLayout.cpp \
	rendering/RenderBox.cpp \
	rendering/RenderBoxModelObject.cpp \
	rendering/RenderButton.cpp \
	rendering/RenderCombineText.cpp \
	rendering/RenderCounter.cpp \
	rendering/RenderDetails.cpp \
	rendering/RenderDetailsMarker.cpp \
	rendering/RenderEmbeddedObject.cpp \
	rendering/RenderFieldset.cpp \
	rendering/RenderFileUploadControl.cpp \
	rendering/RenderFlexibleBox.cpp \
	rendering/RenderFrame.cpp \
	rendering/RenderFrameBase.cpp \
	rendering/RenderFrameSet.cpp \
	rendering/RenderFullScreen.cpp \
	rendering/RenderHTMLCanvas.cpp \
	rendering/RenderIFrame.cpp \
	rendering/RenderImage.cpp \
	rendering/RenderImageResource.cpp \
	rendering/RenderImageResourceStyleImage.cpp \
	rendering/RenderInline.cpp \
	rendering/RenderLayer.cpp \
	rendering/RenderLayerBacking.cpp \
	rendering/RenderLayerCompositor.cpp \
	rendering/RenderLineBoxList.cpp \
	rendering/RenderListBox.cpp \
	rendering/RenderListItem.cpp \
	rendering/RenderListMarker.cpp \
	rendering/RenderMarquee.cpp \
	rendering/RenderMedia.cpp \
	rendering/RenderMenuList.cpp \
	rendering/RenderObject.cpp \
	rendering/RenderObjectChildList.cpp \
	rendering/RenderPart.cpp \
	rendering/RenderQuote.cpp \
	rendering/RenderReplaced.cpp \
	rendering/RenderReplica.cpp \
	rendering/RenderRuby.cpp \
	rendering/RenderRubyBase.cpp \
	rendering/RenderRubyRun.cpp \
	rendering/RenderRubyText.cpp

ifeq ($(ENABLE_SVG), true)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	rendering/svg/RenderSVGBlock.cpp \
	rendering/svg/RenderSVGContainer.cpp \
	rendering/svg/RenderSVGForeignObject.cpp \
	rendering/svg/RenderSVGGradientStop.cpp \
	rendering/svg/RenderSVGHiddenContainer.cpp \
	rendering/svg/RenderSVGImage.cpp \
	rendering/svg/RenderSVGInline.cpp \
	rendering/svg/RenderSVGInlineText.cpp \
	rendering/svg/RenderSVGModelObject.cpp \
	rendering/svg/RenderSVGPath.cpp \
	rendering/svg/RenderSVGResource.cpp \
	rendering/svg/RenderSVGResourceClipper.cpp \
	rendering/svg/RenderSVGResourceContainer.cpp \
	rendering/svg/RenderSVGResourceFilter.cpp \
	rendering/svg/RenderSVGResourceFilterPrimitive.cpp \
	rendering/svg/RenderSVGResourceGradient.cpp \
	rendering/svg/RenderSVGResourceLinearGradient.cpp \
	rendering/svg/RenderSVGResourceMarker.cpp \
	rendering/svg/RenderSVGResourceMasker.cpp \
	rendering/svg/RenderSVGResourcePattern.cpp \
	rendering/svg/RenderSVGResourceRadialGradient.cpp \
	rendering/svg/RenderSVGResourceSolidColor.cpp \
	rendering/svg/RenderSVGRoot.cpp \
	rendering/svg/RenderSVGShadowTreeRootContainer.cpp \
	rendering/svg/RenderSVGTSpan.cpp \
	rendering/svg/RenderSVGText.cpp \
	rendering/svg/RenderSVGTextPath.cpp \
	rendering/svg/RenderSVGTransformableContainer.cpp \
	rendering/svg/RenderSVGViewportContainer.cpp \
	rendering/svg/SVGImageBufferTools.cpp \
	rendering/svg/SVGInlineFlowBox.cpp \
	rendering/svg/SVGInlineTextBox.cpp \
	rendering/svg/SVGMarkerLayoutInfo.cpp \
	rendering/svg/SVGRenderSupport.cpp \
	rendering/svg/SVGRenderTreeAsText.cpp \
	rendering/svg/SVGResources.cpp \
	rendering/svg/SVGResourcesCache.cpp \
	rendering/svg/SVGResourcesCycleSolver.cpp \
	rendering/svg/SVGRootInlineBox.cpp \
	rendering/svg/SVGShadowTreeElements.cpp \
	rendering/svg/SVGTextChunk.cpp \
	rendering/svg/SVGTextChunkBuilder.cpp \
	rendering/svg/SVGTextLayoutAttributes.cpp \
	rendering/svg/SVGTextLayoutAttributesBuilder.cpp \
	rendering/svg/SVGTextLayoutEngine.cpp \
	rendering/svg/SVGTextLayoutEngineBaseline.cpp \
	rendering/svg/SVGTextLayoutEngineSpacing.cpp \
	rendering/svg/SVGTextMetrics.cpp \
	rendering/svg/SVGTextQuery.cpp
endif

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	rendering/RenderScrollbar.cpp \
	rendering/RenderScrollbarPart.cpp \
	rendering/RenderScrollbarTheme.cpp \
	rendering/RenderSlider.cpp \
	rendering/RenderSummary.cpp \
	rendering/RenderTable.cpp \
	rendering/RenderTableCell.cpp \
	rendering/RenderTableCol.cpp \
	rendering/RenderTableRow.cpp \
	rendering/RenderTableSection.cpp \
	rendering/RenderText.cpp \
	rendering/RenderTextControl.cpp \
	rendering/RenderTextControlMultiLine.cpp \
	rendering/RenderTextControlSingleLine.cpp \
	rendering/RenderTextFragment.cpp \
	rendering/RenderTheme.cpp \
	rendering/RenderTreeAsText.cpp \
	rendering/RenderVideo.cpp \
	rendering/RenderView.cpp \
	rendering/RenderWidget.cpp \
	rendering/RenderWordBreak.cpp \
	rendering/RootInlineBox.cpp \

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	rendering/ScrollBehavior.cpp \
	rendering/ShadowElement.cpp \
	rendering/TransformState.cpp \
	rendering/break_lines.cpp \
	\
	rendering/style/ContentData.cpp \
	rendering/style/CounterDirectives.cpp \
	rendering/style/FillLayer.cpp \
	rendering/style/KeyframeList.cpp \
	rendering/style/NinePieceImage.cpp \
	rendering/style/QuotesData.cpp \
	rendering/style/RenderStyle.cpp \

ifeq ($(ENABLE_SVG), true)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	rendering/style/SVGRenderStyle.cpp \
	rendering/style/SVGRenderStyleDefs.cpp
endif

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	rendering/style/ShadowData.cpp \
	rendering/style/StyleBackgroundData.cpp \
	rendering/style/StyleBoxData.cpp \
	rendering/style/StyleCachedImage.cpp \
	rendering/style/StyleFlexibleBoxData.cpp \
	rendering/style/StyleGeneratedImage.cpp \
	rendering/style/StyleInheritedData.cpp \
	rendering/style/StyleMarqueeData.cpp \
	rendering/style/StyleMultiColData.cpp \
	rendering/style/StyleRareInheritedData.cpp \
	rendering/style/StyleRareNonInheritedData.cpp \
	rendering/style/StyleSurroundData.cpp \
	rendering/style/StyleTransformData.cpp \
	rendering/style/StyleVisualData.cpp \
	\
	storage/AbstractDatabase.cpp \
	storage/ChangeVersionWrapper.cpp \
	storage/Database.cpp \
	storage/DatabaseAuthorizer.cpp \
	storage/DatabaseSync.cpp \
	storage/DatabaseTask.cpp \
	storage/DatabaseThread.cpp \
	storage/DatabaseTracker.cpp \
	storage/IDBAny.cpp \
	storage/IDBCursor.cpp \
	storage/IDBCursorBackendImpl.cpp \
	storage/IDBCursorWithValue.cpp \
	storage/IDBDatabase.cpp \
	storage/IDBDatabaseBackendImpl.cpp \
	storage/IDBEventDispatcher.cpp \
	storage/IDBFactory.cpp \
	storage/IDBFactoryBackendInterface.cpp \
	storage/IDBFactoryBackendImpl.cpp \
	storage/IDBIndex.cpp \
	storage/IDBIndexBackendImpl.cpp \
	storage/IDBKey.cpp \
	storage/IDBKeyRange.cpp \
	storage/IDBObjectStore.cpp \
	storage/IDBObjectStoreBackendImpl.cpp \
	storage/IDBRequest.cpp \
	storage/IDBTransaction.cpp \
	storage/IDBVersionChangeEvent.cpp \
	storage/IDBVersionChangeRequest.cpp \
	storage/LocalStorageTask.cpp \
	storage/LocalStorageThread.cpp \
	storage/OriginQuotaManager.cpp \
	storage/OriginUsageRecord.cpp \
	storage/SQLResultSet.cpp \
	storage/SQLResultSetRowList.cpp \
	storage/SQLStatement.cpp \
	storage/SQLStatementSync.cpp \
	storage/SQLTransaction.cpp \
	storage/SQLTransactionClient.cpp \
	storage/SQLTransactionCoordinator.cpp \
	storage/SQLTransactionSync.cpp \
	storage/Storage.cpp \
	storage/StorageAreaImpl.cpp \
	storage/StorageAreaSync.cpp \
	storage/StorageEvent.cpp \
	storage/StorageEventDispatcher.cpp \
	storage/StorageMap.cpp \
	storage/StorageNamespace.cpp \
	storage/StorageNamespaceImpl.cpp \
	storage/StorageSyncManager.cpp \
	storage/StorageTracker.cpp

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	websockets/WebSocket.cpp \
	websockets/WebSocketChannel.cpp \
	websockets/WebSocketHandshake.cpp \
	websockets/ThreadableWebSocketChannel.cpp \
	websockets/WorkerThreadableWebSocketChannel.cpp \
	websockets/WebSocketHandshakeRequest.cpp \
	websockets/WebSocketHandshakeResponse.cpp \
	platform/network/SocketStreamHandleBase.cpp \
	platform/network/SocketStreamErrorBase.cpp \
	platform/network/android/SocketStreamHandleAndroid.cpp

ifeq ($(ENABLE_SVG), true)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	svg/ColorDistance.cpp \
	svg/SVGAElement.cpp \
	svg/SVGAllInOne.cpp \
	svg/SVGAltGlyphElement.cpp \
	svg/SVGAngle.cpp \
	svg/SVGAnimateColorElement.cpp \
	svg/SVGAnimateElement.cpp \
	svg/SVGAnimateMotionElement.cpp \
	svg/SVGAnimateTransformElement.cpp \
	svg/SVGAnimationElement.cpp \
	svg/SVGCircleElement.cpp \
	svg/SVGClipPathElement.cpp \
	svg/SVGColor.cpp \
	svg/SVGComponentTransferFunctionElement.cpp \
	svg/SVGCursorElement.cpp \
	svg/SVGDefsElement.cpp \
	svg/SVGDescElement.cpp \
	svg/SVGDocument.cpp \
	svg/SVGDocumentExtensions.cpp \
	svg/SVGElement.cpp \
	svg/SVGElementInstance.cpp \
	svg/SVGElementInstanceList.cpp \
	svg/SVGEllipseElement.cpp \
	svg/SVGExternalResourcesRequired.cpp \
	svg/SVGFEBlendElement.cpp \
	svg/SVGFEColorMatrixElement.cpp \
	svg/SVGFEComponentTransferElement.cpp \
	svg/SVGFECompositeElement.cpp \
	svg/SVGFEConvolveMatrixElement.cpp \
	svg/SVGFEDiffuseLightingElement.cpp \
	svg/SVGFEDisplacementMapElement.cpp \
	svg/SVGFEDistantLightElement.cpp \
	svg/SVGFEFloodElement.cpp \
	svg/SVGFEFuncAElement.cpp \
	svg/SVGFEFuncBElement.cpp \
	svg/SVGFEFuncGElement.cpp \
	svg/SVGFEFuncRElement.cpp \
	svg/SVGFEGaussianBlurElement.cpp \
	svg/SVGFEImageElement.cpp \
	svg/SVGFELightElement.cpp \
	svg/SVGFEMergeElement.cpp \
	svg/SVGFEMergeNodeElement.cpp \
	svg/SVGFEOffsetElement.cpp \
	svg/SVGFEPointLightElement.cpp \
	svg/SVGFESpecularLightingElement.cpp \
	svg/SVGFESpotLightElement.cpp \
	svg/SVGFETileElement.cpp \
	svg/SVGFETurbulenceElement.cpp \
	svg/SVGFilterElement.cpp \
	svg/SVGFilterPrimitiveStandardAttributes.cpp \
	svg/SVGFitToViewBox.cpp \
	svg/SVGFont.cpp \
	svg/SVGFontData.cpp \
	svg/SVGFontElement.cpp \
	svg/SVGFontFaceElement.cpp \
	svg/SVGFontFaceFormatElement.cpp \
	svg/SVGFontFaceNameElement.cpp \
	svg/SVGFontFaceSrcElement.cpp \
	svg/SVGFontFaceUriElement.cpp \
	svg/SVGForeignObjectElement.cpp \
	svg/SVGGElement.cpp \
	svg/SVGGlyphElement.cpp \
	svg/SVGGradientElement.cpp \
	svg/SVGHKernElement.cpp \
	svg/SVGImageElement.cpp \
	svg/SVGImageLoader.cpp \
	svg/SVGLangSpace.cpp \
	svg/SVGLength.cpp \
	svg/SVGLengthList.cpp \
	svg/SVGLineElement.cpp \
	svg/SVGLinearGradientElement.cpp \
	svg/SVGLocatable.cpp \
	svg/SVGMPathElement.cpp \
	svg/SVGMarkerElement.cpp \
	svg/SVGMaskElement.cpp \
	svg/SVGMetadataElement.cpp \
	svg/SVGMissingGlyphElement.cpp \
	svg/SVGNumberList.cpp \
	svg/SVGPaint.cpp \
	svg/SVGParserUtilities.cpp \
	svg/SVGPathBlender.cpp \
	svg/SVGPathBuilder.cpp \
	svg/SVGPathByteStreamBuilder.cpp \
	svg/SVGPathByteStreamSource.cpp \
	svg/SVGPathElement.cpp \
	svg/SVGPathParser.cpp \
	svg/SVGPathParserFactory.cpp \
	svg/SVGPathSegList.cpp \
	svg/SVGPathSegListBuilder.cpp \
	svg/SVGPathSegListSource.cpp \
	svg/SVGPathStringBuilder.cpp \
	svg/SVGPathStringSource.cpp \
	svg/SVGPathTraversalStateBuilder.cpp \
	svg/SVGPatternElement.cpp \
	svg/SVGPointList.cpp \
	svg/SVGPolyElement.cpp \
	svg/SVGPolygonElement.cpp \
	svg/SVGPolylineElement.cpp \
	svg/SVGPreserveAspectRatio.cpp \
	svg/SVGRadialGradientElement.cpp \
	svg/SVGRectElement.cpp \
	svg/SVGSVGElement.cpp \
	svg/SVGScriptElement.cpp \
	svg/SVGSetElement.cpp \
	svg/SVGStopElement.cpp \
	svg/SVGStringList.cpp \
	svg/SVGStylable.cpp \
	svg/SVGStyleElement.cpp \
	svg/SVGStyledElement.cpp \
	svg/SVGStyledLocatableElement.cpp \
	svg/SVGStyledTransformableElement.cpp \
	svg/SVGSwitchElement.cpp \
	svg/SVGSymbolElement.cpp \
	svg/SVGTRefElement.cpp \
	svg/SVGTSpanElement.cpp \
	svg/SVGTests.cpp \
	svg/SVGTextContentElement.cpp \
	svg/SVGTextElement.cpp \
	svg/SVGTextPathElement.cpp \
	svg/SVGTextPositioningElement.cpp \
	svg/SVGTitleElement.cpp \
	svg/SVGTransform.cpp \
	svg/SVGTransformDistance.cpp \
	svg/SVGTransformList.cpp \
	svg/SVGTransformable.cpp \
	svg/SVGURIReference.cpp \
	svg/SVGUseElement.cpp \
	svg/SVGViewElement.cpp \
	svg/SVGViewSpec.cpp \
	svg/SVGVKernElement.cpp \
	svg/SVGZoomAndPan.cpp \
	svg/SVGZoomEvent.cpp \
	\
	svg/animation/SMILTime.cpp \
	svg/animation/SMILTimeContainer.cpp \
	svg/animation/SVGSMILElement.cpp \
	\
	svg/graphics/SVGImage.cpp \
	\
	svg/graphics/filters/SVGFEImage.cpp \
	svg/graphics/filters/SVGFilter.cpp \
	svg/graphics/filters/SVGFilterBuilder.cpp \
	\
	svg/properties/SVGPathSegListPropertyTearOff.cpp
endif

LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	webaudio/AudioParam.cpp \
	\
	workers/AbstractWorker.cpp \
	workers/DedicatedWorkerContext.cpp \
	workers/DedicatedWorkerThread.cpp \
	workers/DefaultSharedWorkerRepository.cpp \
	workers/SharedWorker.cpp \
	workers/SharedWorkerContext.cpp \
	workers/SharedWorkerThread.cpp \
	workers/Worker.cpp \
	workers/WorkerContext.cpp \
	workers/WorkerLocation.cpp \
	workers/WorkerMessagingProxy.cpp \
	workers/WorkerRunLoop.cpp \
	workers/WorkerScriptLoader.cpp \
	workers/WorkerThread.cpp \
	\
	xml/DOMParser.cpp \
	xml/XMLHttpRequest.cpp \
	xml/XMLHttpRequestProgressEventThrottle.cpp \
	xml/XMLHttpRequestUpload.cpp \
	xml/XMLSerializer.cpp \
	xml/XMLTreeViewer.cpp

# For XPath
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	xml/NativeXPathNSResolver.cpp \
	xml/XPathEvaluator.cpp \
	xml/XPathExpression.cpp \
	xml/XPathExpressionNode.cpp \
	xml/XPathFunctions.cpp \
	xml/XPathNSResolver.cpp \
	xml/XPathNamespace.cpp \
	xml/XPathNodeSet.cpp \
	xml/XPathParser.cpp \
	xml/XPathPath.cpp \
	xml/XPathPredicate.cpp \
	xml/XPathResult.cpp \
	xml/XPathStep.cpp \
	xml/XPathUtil.cpp \
	xml/XPathValue.cpp \
	xml/XPathVariableReference.cpp

# For XSLT
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	loader/cache/CachedXSLStyleSheet.cpp \
	dom/TransformSourceLibxslt.cpp \
	xml/XSLImportRule.cpp \
	xml/XSLStyleSheetLibxslt.cpp \
	xml/XSLTExtensions.cpp \
	xml/XSLTProcessorLibxslt.cpp \
	xml/XSLTProcessor.cpp \
	xml/XSLTUnicodeSort.cpp

# For Archive
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	loader/archive/ArchiveFactory.cpp \
	loader/archive/ArchiveResource.cpp \
	loader/archive/ArchiveResourceCollection.cpp \
	loader/archive/android/WebArchiveAndroid.cpp

# For complex scripts(Arabic, Thai, Hindi...).
ifeq ($(SUPPORT_COMPLEX_SCRIPTS),true)
LOCAL_SRC_FILES := $(LOCAL_SRC_FILES) \
	platform/graphics/android/fonts/HarfbuzzSkia.cpp
endif

# HACK for qcom devices CPU upload mode
ifeq ($(TARGET_FORCE_CPU_UPLOAD),true)
LOCAL_CFLAGS += -DFORCE_CPU_UPLOAD
endif
