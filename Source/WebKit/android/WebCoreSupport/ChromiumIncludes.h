/*
 * Copyright 2010, The Android Open Source Project
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

// All source files wishing to include Chromium headers must include this file
// and must not incude Chromium headers directly.

#ifndef ChromiumIncludes_h
#define ChromiumIncludes_h

#include "config.h"

// Both WebKit and Chromium define LOG. In AOSP, the framework also defines
// LOG. To avoid conflicts, we undefine LOG before including Chromium code,
// then define it back to the WebKit macro.
#ifdef LOG
#define LOG_WAS_DEFINED
#undef LOG
#endif

// In AOSP, the framework still uses LOG_ASSERT (as well as ALOG_ASSERT), which
// conflicts with Chromium's LOG_ASSERT. So we undefine LOG_ASSERT to allow the
// Chromium implementation to be picked up. We also redefine ALOG_ASSERT to the
// underlying framework implementation without using LOG_ASSERT.
// TODO: Remove this once LOG_ASSERT is removed from the framework in AOSP.
#undef LOG_ASSERT
#undef ALOG_ASSERT
// Copied from log.h.
#define ALOG_ASSERT(cond, ...) LOG_FATAL_IF(!(cond), ## __VA_ARGS__)

// Chromium won't build without NDEBUG set, so we set it for all source files
// that use Chromium code. This means that if NDEBUG was previously unset, we
// have to redefine ASSERT() to a no-op, as this is enabled in debug builds.
// Unfortunately, ASSERT() is defined from config.h, so we can't get in first.
#ifndef NDEBUG
#define NDEBUG 1
#undef ASSERT
#define ASSERT(assertion) (void(0))
#endif

#include <android/net/android_network_library_impl.h>
#include <android/jni/jni_utils.h>
#include <base/callback.h>
#include <base/lazy_instance.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop_proxy.h>
#include <base/openssl_util.h>
#include <base/string_util.h>
#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>
#include <base/sys_string_conversions.h>
#include <base/threading/thread.h>
#include <base/time.h>
#include <base/tuple.h>
#include <base/utf_string_conversions.h>
#include <chrome/browser/net/sqlite_persistent_cookie_store.h>
#include <net/base/auth.h>
#include <net/base/cert_verifier.h>
#include <net/base/cookie_monster.h>
#include <net/base/cookie_policy.h>
#include <net/base/data_url.h>
#include <net/base/host_resolver.h>
#include <net/base/io_buffer.h>
#include <net/base/load_flags.h>
#include <net/base/net_errors.h>
#include <net/base/mime_util.h>
#include <net/base/net_util.h>
#include <net/base/openssl_private_key_store.h>
#include <net/base/ssl_cert_request_info.h>
#include <net/base/ssl_config_service.h>
#include <net/disk_cache/disk_cache.h>
#include <net/http/http_auth_handler_factory.h>
#include <net/http/http_cache.h>
#include <net/http/http_network_layer.h>
#include <net/http/http_response_headers.h>
#include <net/proxy/proxy_config_service_android.h>
#include <net/proxy/proxy_service.h>
#include <net/url_request/url_request.h>
#include <net/url_request/url_request_context.h>

#if ENABLE(WEB_AUTOFILL)
#include <autofill/autofill_manager.h>
#include <autofill/autofill_profile.h>
#include <autofill/personal_data_manager.h>
#include <base/logging.h>
#include <base/memory/scoped_vector.h>
#include <base/string16.h>
#include <base/utf_string_conversions.h>
#include <chrome/browser/autofill/autofill_host.h>
#include <chrome/browser/profiles/profile.h>
#include <content/browser/tab_contents/tab_contents.h>
#include <webkit/glue/form_data.h>
#include <webkit/glue/form_field.h>
#endif

#undef LOG
// If LOG was defined, restore it to the WebKit macro.
#ifdef LOG_WAS_DEFINED
// If LOG was defined, JOIN_LOG_CHANNEL_WITH_PREFIX must be too.
// Copied from Assertions.h.
#if LOG_DISABLED
#define LOG(channel, ...) ((void)0)
#else
#define LOG(channel, ...) WTFLog(&JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, channel), __VA_ARGS__)
#endif
#endif

#endif
