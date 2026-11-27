set(N8V_HAVE_WEBP FALSE)
option(N8V_WEBP "Decode WebP images, with libwebp loaded at runtime if it is there." ON)

if(N8V_WEBP AND NOT EMSCRIPTEN)
  set(N8V_HAVE_WEBP TRUE)
  lzy_add_wrapper(n8v_webp_lazy
    LIBRARY_NAMES
      "libwebp.so.7"
      "libwebp.so"
      "libwebp.7.dylib"
      "libwebp.dylib"
      "libwebp-7.dll"
      "libwebp.dll"
    SYMBOLS
      WebPDecodeRGBA
      WebPFree
    REPLACE_PREFIX WebP n8v_WebP
  )
  lzy_add_wrapper(n8v_webpdemux_lazy
    LIBRARY_NAMES
      "libwebpdemux.so.2"
      "libwebpdemux.so"
      "libwebpdemux.2.dylib"
      "libwebpdemux.dylib"
      "libwebpdemux-2.dll"
      "libwebpdemux.dll"
    SYMBOLS
      WebPAnimDecoderOptionsInitInternal
      WebPAnimDecoderNewInternal
      WebPAnimDecoderGetInfo
      WebPAnimDecoderGetNext
      WebPAnimDecoderDelete
    REPLACE_PREFIX WebP n8v_WebP
  )
endif()
