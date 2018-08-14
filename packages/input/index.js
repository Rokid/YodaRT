'use strict';

/**
 * @namespace input
 */

var InputWrap = require('./input.node').InputWrap;
var EventEmitter = require('events').EventEmitter;
var inherits = require('util').inherits;

var handler = null;
var events = [
  'keyup', 'keydown', 'longpress'
];

/**
 * @constructor
 * @param {Object} options - the options to input event
 * @param {Number} options.selectTimeout
 * @param {Number} options.dbclickTimeout
 * @param {Number} options.slideTimeout
 */
function InputEvent(options) {
  EventEmitter.call(this);

  this._options = options || {
    selectTimeout: 300,
    dbclickTimeout: 300,
    slideTimeout: 300,
  };
  this._handle = new InputWrap();
  this._handle.onevent = this.onevent.bind(this);
}
inherits(InputEvent, EventEmitter);

/**
 * event trigger
 * @param {Number} state - the event state
 * @param {Number} action - the event action
 * @param {Number} code - the event code
 * @param {Number} time - the event time
 */
InputEvent.prototype.onevent = function(state, action, code, time) {
  var name = events[state];
  if (!name) {
    this.emit('error', new Error(`unknown event name ${state}`));
    return;
  }
  this.emit(name, {
    keyCode: code,
    keyTime: time,
  });
};

/**
 * start handling event
 */
InputEvent.prototype.start = function() {
  return this._handle.start(this._options);
};

/**
 * disconnect from event handler
 */
InputEvent.prototype.disconnect = function() {
  return this._handle.disconnect();
};

/**
 * get the event handler
 * @memberof input
 */
function getHandler(options) {
  if (handler) {
    if (options)
      console.error('skip options setting because already init done');
    return handler;
  }

  handler = new InputEvent(options);
  handler.start();
  return handler;
}

module.exports = getHandler;

