An alternate JS engine has been added, called the v8_leading based on the svn 3610 Maintrunk 
of Google's V8 open source project (under Chromium). the existing v8 is old (sometime August 2009), while
the v8_leading has much improved performance and has bug fixes. 

The v8_leading can be turned on by setting USE_ALT_JS_ENGINE := true

Also a V8 API mismatch bug has been fixed under WebCore/bindings/v8/V8Proxy.cpp & V8proxy.h so that
the latest V8 javaScript engine can be used with the version of webkit used in eclair 2.1
