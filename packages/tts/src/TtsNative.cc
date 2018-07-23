#include "TtsNative.h"

void TtsNative::SendEvent(void* self, TtsResultType event, int id, int code) {
  TtsNative* native = (TtsNative*)self;
  iotjs_tts_t* ttswrap = native->ttswrap;
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_tts_t, ttswrap);

  jerry_value_t jthis = iotjs_jobjectwrap_jobject(&_this->jobjectwrap);
  jerry_value_t onevent = iotjs_jval_get_property(jthis, "onevent");
  if (!jerry_value_is_function(onevent)) {
    fprintf(stderr, "no onevent function is registered\n");
    JS_CREATE_ERROR(COMMON, "no onevent function is registered");
    return;
  }

  uint32_t jargc = 3;
  jerry_value_t jargv[jargc] = {
    jerry_create_number((double)event),
    jerry_create_number((double)id),
    jerry_create_number((double)code),
  };
  jerry_call_function(onevent, jerry_create_undefined(), jargv, jargc);
  for (int i = 0; i < jargc; i++) {
    jerry_release_value(jargv[i]);
  }
  jerry_release_value(jthis);
  jerry_release_value(onevent);
}

static JNativeInfoType this_module_native_info = {
  .free_cb = (jerry_object_native_free_callback_t)iotjs_tts_destroy
};

static iotjs_tts_t* iotjs_tts_create(jerry_value_t jtts) {
  iotjs_tts_t* ttswrap = IOTJS_ALLOC(iotjs_tts_t);
  IOTJS_VALIDATED_STRUCT_CONSTRUCTOR(iotjs_tts_t, ttswrap);

  iotjs_jobjectwrap_initialize(&_this->jobjectwrap, jtts, &this_module_native_info);
  _this->handle = new TtsNative(ttswrap);
  _this->prepared = false;
  return ttswrap;
}

static void iotjs_tts_destroy(iotjs_tts_t* tts) {
  IOTJS_VALIDATED_STRUCT_DESTRUCTOR(iotjs_tts_t, tts);
  if (_this->handle) {
    delete _this->handle;
    _this->prepared = false;
  }
  iotjs_jobjectwrap_destroy(&_this->jobjectwrap);
  IOTJS_RELEASE(tts);
}

JS_FUNCTION(TTS) {
  DJS_CHECK_THIS();
  
  const jerry_value_t jtts = JS_GET_THIS();
  iotjs_tts_t* tts_instance = iotjs_tts_create(jtts);
  return jerry_create_undefined();
}

JS_FUNCTION(Prepare) {
  JS_DECLARE_THIS_PTR(tts, tts);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_tts_t, tts);

  if (_this->handle == NULL) {
    return JS_CREATE_ERROR(COMMON, "tts is not initialized");
  }
  if (_this->prepared == true) {
    return JS_CREATE_ERROR(COMMON, "instance has been prepared already");
  }

  char* host;
  int port;
  char* branch;
  char* key;
  char* device_type;
  char* device_id;
  char* secret;
  char* declaimer;

  port = (int)JS_GET_ARG(1, number);
  for (int i = 0; i < 8; i++) {
    if (i == 1)
      continue;
    jerry_size_t size = jerry_get_string_size(jargv[i]);
    jerry_char_t text_buf[size];
    jerry_string_to_char_buffer(jargv[i], text_buf, size);
    text_buf[size] = '\0';

    switch (i) {
      case 0: host = strdup((char*)&text_buf); break;
      case 2: branch = strdup((char*)&text_buf); break;
      case 3: key = strdup((char*)&text_buf); break;
      case 4: device_type = strdup((char*)&text_buf); break;
      case 5: device_id = strdup((char*)&text_buf); break;
      case 6: secret = strdup((char*)&text_buf); break;
      case 7: declaimer = strdup((char*)&text_buf); break;
      default:
        break;
    }
  }

  fprintf(stdout,
    "host: %s, port: %d, branch: %s\n", host, port, branch);

  _this->prepared = true;
  _this->handle->prepare(host, port, branch,
    key, device_type, device_id, secret, declaimer);
  
  free(host);
  free(branch);
  free(key);
  free(device_type);
  free(device_id);
  free(secret);
  free(declaimer);
  return jerry_create_boolean(true);
}

JS_FUNCTION(Speak) {
  JS_DECLARE_THIS_PTR(tts, tts);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_tts_t, tts);

  if (_this->handle == NULL) {
    return JS_CREATE_ERROR(COMMON, "tts is not initialized");
  }
  if (jargc == 0) {
    return JS_CREATE_ERROR(COMMON, "first argument should be a string");
  }

  char* text = NULL;
  jerry_size_t size = jerry_get_utf8_string_size(jargv[0]);
  jerry_char_t text_buf[size];
  jerry_string_to_utf8_char_buffer(jargv[0], text_buf, size);
  text_buf[size] = '\0';
  text = (char*)&text_buf;
 
  int32_t id = _this->handle->speak(text);
  return jerry_create_number(id);
}

JS_FUNCTION(Cancel) {
  JS_DECLARE_THIS_PTR(tts, tts);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_tts_t, tts);

  if (_this->handle == NULL) {
    return JS_CREATE_ERROR(COMMON, "tts is not initialized");
  }
  int id = JS_GET_ARG(0, number);
  _this->handle->cancel(id);
  return jerry_create_boolean(true);
}

JS_FUNCTION(Disconnect) {
  JS_DECLARE_THIS_PTR(tts, tts);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_tts_t, tts);

  if (_this->handle == NULL) {
    return JS_CREATE_ERROR(COMMON, "tts is not initialized");
  }
  _this->handle->disconnect();
  jerry_value_t jval = iotjs_jobjectwrap_jobject(&_this->jobjectwrap);
  jerry_release_value(jval);
  return jerry_create_boolean(true);
}

void init(jerry_value_t exports) {
  jerry_value_t jconstructor = jerry_create_external_function(TTS);
  iotjs_jval_set_property_jval(exports, "TtsWrap", jconstructor);
  
  jerry_value_t proto = jerry_create_object();
  iotjs_jval_set_method(proto, "prepare", Prepare);
  iotjs_jval_set_method(proto, "speak", Speak);
  iotjs_jval_set_method(proto, "cancel", Cancel);
  iotjs_jval_set_method(proto, "disconnect", Disconnect);
  iotjs_jval_set_property_jval(jconstructor, "prototype", proto);

  jerry_release_value(proto);
  jerry_release_value(jconstructor);
}

NODE_MODULE(tts, init)
