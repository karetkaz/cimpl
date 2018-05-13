function JsArgs(paramSeparator, onHashChange) {

	// if not specified use local hash to read/save params.
	var valueSeparator = '&';
	var oldHashChange = window.onhashchange;

	if (paramSeparator != null && paramSeparator.constructor === Function) {
		// if the first parameter is a function threat it as the callback
		if (onHashChange === undefined) {
			onHashChange = paramSeparator;
		}
	}
	if (paramSeparator !== '?') {
		paramSeparator = '#';
	}

	// read values from url
	function read(args) {
		var values = window.location.href;
		var pos = values.indexOf(paramSeparator);
		if (pos >= 0) {
			values = values.substr(pos + 1).split(valueSeparator);
		}
		else {
			values = [];
		}
		for (var i = 0; i < values.length; i += 1) {
			pos = values[i].indexOf('=');
			if (pos > 0) {
				var key = values[i].substr(0, pos);
				var value = values[i].substr(pos + 1);
				args[key] = decodeURIComponent(value);
			}
		}
		return args;
	}

	// write values to url
	function save(args, forceReload) {
		var values = '';
		for (var key in args) {
			if (!args.hasOwnProperty(key)) {
				// exclude prototype properties
				continue;
			}

			var value = args[key];
			if (value === null || value === undefined) {
				// exclude undefined and null values
				delete args[key];
				continue;
			}
			if (value.constructor !== String && value.constructor !== Number && value.constructor !== Boolean) {
				// exclude non string, number and boolean values.
				continue;
			}

			if (values !== '') {
				values += valueSeparator;
			}
			values += key + '=' + encodeURIComponent(value);
		}

		if (forceReload === true) {
			window.onhashchange = oldHashChange;
			window.location.href = paramSeparator + values;
			window.location.reload();
		}
		window.location.href = paramSeparator + values;
		return args;
	}

	var result = Object.create({
		update: function(values) {
			/*
			update(null) -> reload: [noescape, notifyChange]
			update() -> reload: [noescape, notifyForced]
			update(false) -> reload: escape params
			update(true) -> reload: escape params and force reload
			update({}) -> update escape params write back url
			*/
			var changes = Object.create(null);
			var forceNotify = values === undefined;
			var noescape = values == null;
			var reload = values === true;

			if (values == null || values.constructor != Object) {
				values = read({});
				for (var key in this) {
					if (!this.hasOwnProperty(key)) {
						// do not delete properties from prototype
						continue;
					}
					if (values.hasOwnProperty(key)) {
						// do not delete properties that changed
						continue;
					}
					// property to be deleted
					values[key] = undefined;
				}
			}

			// update existing values.
			for (var key in values) {
				if (!values.hasOwnProperty(key)) {
					// do not update properties in prototype
					continue;
				}
				var oldValue = this[key];
				var newValue = values[key];
				if (oldValue === newValue && !forceNotify) {
					// do not update properties whith same values
					continue;
				}
				if (newValue != null && oldValue != null) {
					// update
					changes[key] = {'new': newValue, 'old': oldValue};
					this[key] = newValue;
				}
				else if (newValue != null) {
					// insert
					changes[key] = {'new': newValue};
					this[key] = newValue;
				}
				else if (oldValue != null) {
					// remove
					changes[key] = {'old': oldValue};
					delete this[key];
				}
			}

			if (onHashChange !== undefined) {
				if (Object.keys(changes).length > 0) {
					onHashChange(this, changes);
				}
			}
			if (noescape) {
				return this;
			}
			return save(this, reload);
		}
	});

	read(result);
	if (onHashChange !== undefined) {
		window.onhashchange = function() {
			result.update(null);
			if (oldHashChange != null) {
				oldHashChange(result);
			}
		};
		onHashChange(result);
	}

	return result;
}
