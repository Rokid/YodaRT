var inherits = require('util').inherits;
var EventEmitter = require('events').EventEmitter;

function Application(options) {
  // 将对象直接展平在this中
  Object.assign(this, {
    beforeCreate: function () {
      return false;
    },
    created: function () {
      // create, restart
      return true;
    },
    paused: function () {
      // pause
      return true;
    },
    resumed: function () {
      // resume
      return true;
    },
    beforeDestroy: function () {
      // stop
      return true;
    },
    destroyed: function () {
      // destroy
      return true;
    },
    onrequest: function () {
      // voice_command or other request
      return true;
    },
    keyEvent: function () {
      // key event
      return true;
    }
  }, options);
}



function Client(appid, runtime, options) {
  var self = this;
  EventEmitter.call(this);
  this.runtime = runtime;
  this.appid = appid;
  // 创建隔离的App
  var app = new Application(options);
  this.app = app;

  this.on('beforeCreate', this._onBeforeCreate.bind(this));
  this.on('create', this._onCreate.bind(this));
  this.on('restart', this._onCreate.bind(this));
  this.on('pause', this._onPaused.bind(this));
  this.on('resume', this._onResumed.bind(this));
  this.on('stop', this._onBeforeDestroy.bind(this));
  this.on('destroy', this._onDestroyed.bind(this));
  this.on('voice_command', this._onVoiceCommand.bind(this));
  this.on('key_event', this._onKeyEvent.bind(this));

  var adapter = new runtime.adapter(runtime.service);
  this.adapter = adapter;

  this.ttsCallback = {};
  adapter.listenTtsdEvent((name, args) => {
    // ttsd的event事件
    if (name === 'ttsdevent') {
      logger.log('ttsevent', args);
      if (typeof this.ttsCallback['ttscb:' + args[0]] === 'function') {
        this.ttsCallback['ttscb:' + args[0]](args[1], args.slice(2));
        // 下面事件完成后不会再触发其它事件，也不应该触发，删除对应cb，防止内存泄漏
        if (args[1] === 'end' || args[1] === 'cancel' || args[1] === 'error') {
          logger.log('unregister', args[0]);
          delete this.ttsCallback['ttscb:' + args[0]];
        }
      }
    }
  }).catch((err) => {
    console.log('ttsd listen error', err);
  });

  this.multiMediaCallback = {};
  adapter.listenMultimediadEvent((name, args) => {
    if (name === 'multimediadevent') {
      logger.log('mediaevent', args);
      if (typeof this.multiMediaCallback['mediacb:' + args[0]] === 'function') {
        this.multiMediaCallback['mediacb:' + args[0]](args[1], args.slice(2));
        if (args[1] === 'end' || args[1] === 'error') {
          logger.log('unregister', args[0]);
          delete this.multiMediaCallback['mediacb:' + args[0]];
        }
      }
    }
  }).catch((err) => {
    console.log('mediad listen error', err);
  });

  //------------------------ 给App注入服务 -------------------------------
  app.getAppId = function () {
    return appId;
  };
  app.exit = function () {
    return self.runtime.exitAppById(appId);
  };
  app.setPickup = function (isPickup) {
    return self.runtime.setPickup(isPickup === true ? true : false);
  };
  app.mockNLPResponse = function (nlp, action) {
    self._onVoiceCommand(nlp, action);
  };
  // tts module
  app.tts = {
    say: function (text, cb) {
      self.adapter.ttsMethod('say', [appId, text])
        .then((args) => {
          // 返回的参数是一个数组，按顺序
          logger.log('tts register', args[0]);
          self.ttsCallback['ttscb:' + args[0]] = cb.bind(app);
        })
        .catch((err) => {
          logger.error(err);
        });
    },
    cancel: function (cb) {
      self.adapter.ttsMethod('cancel', [appId])
        .then((args) => {
          cb.call(app, null);
        })
        .catch((err) => {
          cb.call(app, err);
        });
    }
  };
  // media模块
  app.media = {
    play: function (url, cb) {
      self.adapter.multiMediaMethod('play', [appId, url])
        .then((args) => {
          logger.log('media register', args);
          self.multiMediaCallback['mediacb:' + args[0]] = cb.bind(app);
        })
        .catch((err) => {
          logger.error(err);
        });
    },
    pause: function (cb) {
      self.adapter.multiMediaMethod('pause', [appId])
        .then((args) => {
          cb.call(app, null);
        })
        .catch((err) => {
          cb.call(app, null);
        });
    },
    resume: function (cb) {
      self.adapter.multiMediaMethod('resume', [appId])
        .then((args) => {
          cb.call(app, null);
        })
        .catch((err) => {
          cb.call(app, null);
        });
    },
    cancel: function (cb) {
      self.adapter.multiMediaMethod('cancel', [appId])
        .then((args) => {
          cb.call(app, null);
        })
        .catch((err) => {
          cb.call(app, null);
        });
    }
  };
  // light module
  app.light = {
    setStandby: function () {
      return self.adapter.lightMethod('setStandby', [appId]);
    },
    sound: function (name) {
      return self.adapter.lightMethod('appSound', [appId, name]);
    }
  };
}
inherits(Client, EventEmitter);
/**
   * @method _onBeforeCreate
   */
Client.prototype._onBeforeCreate = function (appid) {
  this.state = 'beforeCreate';
  if (this.app.beforeCreate.apply(this.app, arguments)) {
    this.state = 'beforeCreate OK';
  }
}
/**
 * @method _onCreate
 */
Client.prototype._onCreate = function (context) {
  this.state = 'created';
  this.app.created.apply(this.app, arguments);
}
/**
 * @method _onPaused
 */
Client.prototype._onPaused = function () {
  this.state = 'paused';
  this.app.paused.apply(this.app, arguments);
}
/**
 * @method _onResumed
 */
Client.prototype._onResumed = function () {
  this.state = 'resumed';
  this.app.resumed.apply(this.app, arguments);
}
/**
 * @method _onBeforeDestroy
 */
Client.prototype._onBeforeDestroy = function () {
  this.state = 'beforeDestroy';
  this.app.beforeDestroy.apply(this.app, arguments);
}
/**
 * @method _onDestroyed
 */
Client.prototype._onDestroyed = function () {
  this.state = 'destroyed';
  this.app.destroyed.apply(this.app, arguments);
}
/**
 * @method _onVoiceCommand
 */
Client.prototype._onVoiceCommand = function (nlp, action) {
  this.state = 'voice_command';
  this.app.onrequest.apply(this.app, arguments);
}
/**
 * @method _onKeyEvent
 */
Client.prototype._onKeyEvent = function () {
  this.app.keyEvent.apply(this.app, arguments);
}
/**
 * 通知App manager该应用应该被销毁了
 */
Client.prototype.exit = function () {
  this.runtime.exitAppById(this.appid);
}


module.exports = function (options) {
  return function (appid, runtime) {
    return new Client(appid, runtime, options);
  }
};
