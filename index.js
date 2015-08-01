var events = require ("events");
var util = require ("util");
var cnproc = require ("./build/Release/connector");

for (var key in events.EventEmitter.prototype) {
  cnproc.ConnectorWrap.prototype[key] = events.EventEmitter.prototype[key];
}

function Connector (options) {
	Connector.super_.call (this);

	this.requests = [];

	this.wrap = new cnproc.ConnectorWrap();
	var me = this;
  this.wrap.on('fork', this.onFork.bind(me));
  this.wrap.on('exec', this.onExec.bind(me));
  this.wrap.on('exit', this.onExit.bind(me));
  this.wrap.on('uid', this.onUid.bind(me));
};

util.inherits (Connector, events.EventEmitter);

mkCallback = function(ev) { 
	return function() {
		var args = new Array();
		args.push(ev);
		for(var i = 0; i < arguments.length; i++) {
			args.push(arguments[i]);
		}
		this.emit.apply(this, args);
	}
}

Connector.prototype.onFork = mkCallback('fork');
Connector.prototype.onExec = mkCallback('exec');
Connector.prototype.onExit = mkCallback('exit');
Connector.prototype.onUid = mkCallback('uid');

Connector.prototype.close = function() {
	this.wrap.close();
	return this;
}

Connector.prototype.connect = function() {
	this.wrap.connect();
	return this;
}

exports.Connector = Connector;
exports.createConnector = function() {
	return new Connector();
}
