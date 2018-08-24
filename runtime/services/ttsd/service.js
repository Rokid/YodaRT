'use strict'

var logger = require('logger')('ttsdService')

function Tts (options) {
  this.handle = {}
  this.options = options
}

Tts.prototype.speak = function (appId, text) {
  return new Promise((resolve, reject) => {
    this.options.permit.invoke('check', [appId, 'ACCESS_TTS'])
      .then((res) => {
        logger.log('ttsd say', res, appId, text)
        if (res['0'] === 'true') {
          var req
          req = this.options.tts.speak(text)
          if (this.handle[appId]) {
            setTimeout(() => {
              this.handle[appId].stop()
              delete this.handle[appId]
              this.handle[appId] = req
            }, 0)
          } else {
            this.handle[appId] = req
          }
          resolve(req.id)
        } else {
          reject(new Error('permission deny'))
        }
      })
      .catch((err) => {
        logger.log('ttsd say error', appId, text, err)
        reject(new Error('can not connect to vui'))
      })
  })
}

Tts.prototype.stop = function (appId) {
  if (this.handle[appId]) {
    this.handle[appId].stop()
    delete this.handle[appId]
  }
}

Tts.prototype.reset = function () {
  try {
    for (var index in this.handle) {
      this.handle[index].stop()
    }
  } catch (error) {
    logger.log('error when try to stop all tts')
  }
  this.handle = {}
}

module.exports = Tts
