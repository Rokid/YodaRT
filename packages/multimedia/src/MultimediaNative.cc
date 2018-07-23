#include "MultimediaNative.h"

enum PlayerEventType {
  MULTIMEDIA_PLAYER_NOOP		 = 0,
  MULTIMEDIA_PLAYER_PREPARED 		 = 1,
  MULTIMEDIA_PLAYER_PLAYBACK_COMPLETE    = 2,
  MULTIMEDIA_BUFFERING_UPDATE            = 3,
  MULTIMEDIA_SEEK_COMPLETE 		 = 4,
  MULTIMEDIA_ERROR             		 = 100,
};

void MultimediaListener::notify(int type, int ext1, int ext2, int from) {
  printf("got event %d thread %d\n", type, from);
  if (type == MULTIMEDIA_PLAYER_PREPARED) {
    this->prepared = true;
  }
  if (this->prepared) {
    // only if prepared, enables the notify
    uv_async_t* async_handle = new uv_async_t;
    iotjs_player_event_t* event = new iotjs_player_event_t;
    event->player = this->getPlayer();
    event->type = type;
    event->ext1 = ext1;
    event->ext2 = ext2;
    event->from = from;
    async_handle->data = (void*)event;
    uv_async_init(uv_default_loop(), async_handle, MultimediaListener::DoNotify);
    uv_async_send(async_handle);
  }
}

void MultimediaListener::DoNotify(uv_async_t* handle) {
  iotjs_player_event_t* event = (iotjs_player_event_t*)handle->data;
  iotjs_player_t* player_wrap = event->player;
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player_wrap);

  jerry_value_t jthis = iotjs_jobjectwrap_jobject(&_this->jobjectwrap);
  jerry_value_t notifyFn = iotjs_jval_get_property(jthis, "onevent");
  if (!jerry_value_is_function(notifyFn)) {
    fprintf(stderr, "no onevent function is registered\n");
    JS_CREATE_ERROR(COMMON, "no onevent function is registered");
    return;
  }

  uint32_t jargc = 4;
  jerry_value_t jargv[jargc] = {
    jerry_create_number((double)event->type),
    jerry_create_number((double)event->ext1),
    jerry_create_number((double)event->ext2),
    jerry_create_number((double)event->from),
  };
  jerry_call_function(notifyFn, jerry_create_undefined(), jargv, jargc);
  
  for (int i = 0; i < jargc; i++) {
    jerry_release_value(jargv[i]);
  }
  delete handle;
  delete event;
}

bool MultimediaListener::isPrepared() {
  return this->prepared;
}

iotjs_player_t* MultimediaListener::getPlayer() {
  return this->player;
}

static JNativeInfoType this_module_native_info = {
  .free_cb = (jerry_object_native_free_callback_t)iotjs_player_destroy
};

static iotjs_player_t* iotjs_player_create(jerry_value_t jplayer) {
  iotjs_player_t* player_wrap = IOTJS_ALLOC(iotjs_player_t);
  IOTJS_VALIDATED_STRUCT_CONSTRUCTOR(iotjs_player_t, player_wrap);

  static uint32_t global_id = 1000;
  jerry_value_t jobjectref = jerry_acquire_value(jplayer); // TODO: find someway to release this
  iotjs_jobjectwrap_initialize(&_this->jobjectwrap, jobjectref, &this_module_native_info);
  _this->handle = NULL;
  _this->listener = new MultimediaListener(player_wrap);
  _this->id = (global_id++);
  return player_wrap;
}

static void iotjs_player_destroy(iotjs_player_t* player_wrap) {
  fprintf(stdout, "player instance is destroyed\n");
  IOTJS_VALIDATED_STRUCT_DESTRUCTOR(iotjs_player_t, player_wrap);
  delete _this->handle;
  iotjs_jobjectwrap_destroy(&_this->jobjectwrap);
  IOTJS_RELEASE(player_wrap);
}

JS_FUNCTION(Player) {
  DJS_CHECK_THIS();
  
  const jerry_value_t jplayer = JS_GET_THIS();
  iotjs_player_t* player_wrap = iotjs_player_create(jplayer);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player_wrap);

  jerry_value_t jtag = jargv[0];
  if (!jerry_value_is_string(jtag)) {
    _this->handle = new MediaPlayer(NULL);
  } else {
    jerry_size_t size = jerry_get_string_size(jtag);
    char* tag = iotjs_buffer_allocate(size + 1);
    jerry_string_to_char_buffer(jtag, (jerry_char_t*)tag, size);
    tag[size] = '\0';
    _this->handle = new MediaPlayer(tag);
    iotjs_buffer_release(tag);
  }

  if (_this->listener == NULL)
    return JS_CREATE_ERROR(COMMON, "listener is not initialized");
  _this->handle->setListener(_this->listener);
  return jerry_create_undefined();
}

JS_FUNCTION(Prepare) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  if (_this->handle == NULL)
    return JS_CREATE_ERROR(COMMON, "player native handle is not initialized");

  jerry_value_t jsource = jargv[0];
  if (!jerry_value_is_string(jsource))
    return JS_CREATE_ERROR(COMMON, "source must be a string");

  jerry_size_t srclen = jerry_get_string_size(jsource);
  char source[srclen];
  jerry_string_to_char_buffer(jsource, (jerry_char_t*)source, srclen);
  source[srclen] = '\0';

  _this->handle->setDataSource(source);
  _this->handle->prepareAsync();
  return JS_GET_THIS();
}

JS_FUNCTION(Start) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  if (_this->handle == NULL)
    return JS_CREATE_ERROR(COMMON, "player native handle is not initialized");
  if (!_this->listener->isPrepared())
    return JS_CREATE_ERROR(COMMON, "player is not prepared");

  _this->handle->start();
  return JS_GET_THIS();
}

JS_FUNCTION(Stop) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  if (_this->handle == NULL)
    return JS_CREATE_ERROR(COMMON, "player native handle is not initialized");
  if (!_this->listener->isPrepared())
    return JS_CREATE_ERROR(COMMON, "player is not prepared");

  _this->handle->stop();
  return JS_GET_THIS();
}

JS_FUNCTION(Pause) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  if (_this->handle == NULL)
    return JS_CREATE_ERROR(COMMON, "player native handle is not initialized");
  if (!_this->listener->isPrepared())
    return JS_CREATE_ERROR(COMMON, "player is not prepared");

  _this->handle->pause();
  return JS_GET_THIS();
}

JS_FUNCTION(Resume) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  if (_this->handle == NULL)
    return JS_CREATE_ERROR(COMMON, "player native handle is not initialized");
  if (!_this->listener->isPrepared())
    return JS_CREATE_ERROR(COMMON, "player is not prepared");

  _this->handle->resume();
  return JS_GET_THIS();
}

JS_FUNCTION(Seek) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  if (_this->handle == NULL)
    return JS_CREATE_ERROR(COMMON, "player native handle is not initialized");
  if (!_this->listener->isPrepared())
    return JS_CREATE_ERROR(COMMON, "player is not prepared");

  int ms = JS_GET_ARG(0, number);
  _this->handle->seekTo(ms);
  return JS_GET_THIS();
}

JS_FUNCTION(Reset) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  // FIXME(Yorkie): reset needs forcily to reset without any errors.
  if (_this->handle) {
    _this->handle->reset();
  }
  return JS_GET_THIS();
}

JS_FUNCTION(Disconnect) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  if (_this->handle == NULL)
    return JS_CREATE_ERROR(COMMON, "player native handle is not initialized");
  if (!_this->listener->isPrepared())
    return JS_CREATE_ERROR(COMMON, "player is not prepared");

  _this->handle->disconnect();
  return JS_GET_THIS();
}

JS_FUNCTION(IdGetter) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);
  return jerry_create_number(_this->id);
}

JS_FUNCTION(PlayingStateGetter) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);
  
  if (_this->handle && _this->listener->isPrepared())
    return jerry_create_boolean(_this->handle->isPlaying());
  else
    return jerry_create_boolean(false);
}

JS_FUNCTION(DurationGetter) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  int ms = -1;
  if (_this->handle && _this->listener->isPrepared()) {
    _this->handle->getDuration(&ms);
  }
  return jerry_create_number(ms);
}

JS_FUNCTION(PositionGetter) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  int ms = -1;
  if (_this->handle && _this->listener->isPrepared()) {
    _this->handle->getCurrentPosition(&ms);
  }
  return jerry_create_number(ms);
}

JS_FUNCTION(LoopModeGetter) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  if (_this->handle && _this->listener->isPrepared()) {
    return jerry_create_boolean(_this->handle->isLooping());
  } else {
    return jerry_create_boolean(false);
  }
}

JS_FUNCTION(LoopModeSetter) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  int mode = JS_GET_ARG(0, number);
  if (_this->handle && _this->listener->isPrepared()) {
    _this->handle->setLooping(mode);
    return jerry_create_boolean(true);
  } else {
    return JS_CREATE_ERROR(COMMON, "player is not ready");
  }
}

JS_FUNCTION(VolumeGetter) {
  return jerry_create_undefined();
}

JS_FUNCTION(VolumeSetter) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  float vol = JS_GET_ARG(0, number);
  if (_this->handle) {
    _this->handle->setVolume(vol/* left */, vol/* right */);
    return jerry_create_boolean(true);
  } else {
    return JS_CREATE_ERROR(COMMON, "player is not ready");
  }
}

JS_FUNCTION(SessionIdGetter) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  if (_this->handle) {
    return jerry_create_boolean(_this->handle->getAudioSessionId());
  } else {
    return JS_CREATE_ERROR(COMMON, "player is not ready");
  }
}

JS_FUNCTION(SessionIdSetter) {
  JS_DECLARE_THIS_PTR(player, player);
  IOTJS_VALIDATED_STRUCT_METHOD(iotjs_player_t, player);

  int id = JS_GET_ARG(0, number);
  if (_this->handle) {
    _this->handle->setAudioSessionId(id);
    return jerry_create_boolean(true);
  } else {
    return JS_CREATE_ERROR(COMMON, "player is not ready");
  }
}

void iotjs_set_constant(jerry_value_t jobj, uint32_t idx, const char* v) {
  jerry_value_t jval = jerry_create_string((const jerry_char_t*)v);
  jerry_set_property_by_index(jobj, idx, jval);
  jerry_release_value(jval);
}

void init(jerry_value_t exports) {
  jerry_value_t jconstructor = jerry_create_external_function(Player);
  iotjs_jval_set_property_jval(exports, "Player", jconstructor);
  
  jerry_value_t proto = jerry_create_object();
  iotjs_jval_set_method(proto, "prepare", Prepare);
  iotjs_jval_set_method(proto, "start", Start);
  iotjs_jval_set_method(proto, "stop", Stop);
  iotjs_jval_set_method(proto, "pause", Pause);
  iotjs_jval_set_method(proto, "resume", Resume);
  iotjs_jval_set_method(proto, "seek", Seek);
  iotjs_jval_set_method(proto, "reset", Reset);
  iotjs_jval_set_method(proto, "disconnect", Disconnect);
  
  // the following methods are for getters and setters internally
  iotjs_jval_set_method(proto, "idGetter", IdGetter);
  iotjs_jval_set_method(proto, "playingStateGetter", PlayingStateGetter);
  iotjs_jval_set_method(proto, "durationGetter", DurationGetter);
  iotjs_jval_set_method(proto, "positionGetter", PositionGetter);
  iotjs_jval_set_method(proto, "loopModeGetter", LoopModeGetter);
  iotjs_jval_set_method(proto, "loopModeSetter", LoopModeSetter);
  iotjs_jval_set_method(proto, "volumeGetter", VolumeGetter);
  iotjs_jval_set_method(proto, "volumeSetter", VolumeSetter);
  iotjs_jval_set_method(proto, "sessionIdGetter", SessionIdGetter);
  iotjs_jval_set_method(proto, "sessionIdSetter", SessionIdSetter);
  iotjs_jval_set_property_jval(jconstructor, "prototype", proto);

  // set events
  jerry_value_t events = jerry_create_object();
  iotjs_set_constant(events, MULTIMEDIA_PLAYER_NOOP, "noop");
  iotjs_set_constant(events, MULTIMEDIA_PLAYER_PREPARED, "prepared");
  iotjs_set_constant(events, MULTIMEDIA_PLAYER_PLAYBACK_COMPLETE, "playback complete");
  iotjs_set_constant(events, MULTIMEDIA_BUFFERING_UPDATE, "buffering update");
  iotjs_set_constant(events, MULTIMEDIA_SEEK_COMPLETE, "seek complete");
  iotjs_set_constant(events, MULTIMEDIA_ERROR, "error");
  iotjs_jval_set_property_jval(exports, "Events", events);

  jerry_release_value(proto);
  jerry_release_value(jconstructor);
  jerry_release_value(events);
}

NODE_MODULE(tts, init)
