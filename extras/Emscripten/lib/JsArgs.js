/**
 * Read and update the parameters in the url
 * to query, add, update or remove parameters use the update method
 * 
 * @param paramSeparator optional parameter separator `#` or `?`
 * @param onHashChange callback to be invoked when the something changes
 * @returns an object containing the parameter names associated with the values
 * 
 * basic usage of the update method:
 *  	update(true) -> reload page
 *  	update(false) -> reload params, without change detection
 * 
 *  	update() -> reload params, with change detection
 *  	update(key1, ...) -> reload params, force change detection on given keys
 * 
 *  	update({...}) -> update params, write back url (escaped)
 *  		update({}) just write back url (escaped)
 *  		update({key1 : 'value1', ...}) update or insert parameter 'key1'
 *  		update({key1 : undefined, ...}) delete parameter 'key1'
 *  		update({key1 : null, ...}) delete parameter 'key1'
 * 
 * @constructor
 */
function JsArgs(paramSeparator, onHashChange) {

	// if not specified use local hash to read/save params.
	let valueSeparator = '&';
	let oldHashChange = window.onhashchange;

	if (paramSeparator != null && paramSeparator.constructor === Function) {
		// if the first parameter is a function threat it as the callback
		if (onHashChange === undefined) {
			onHashChange = paramSeparator;
		}
	}
	if (paramSeparator !== '?') {
		paramSeparator = '#';
	}

	function typeOf(ref) {
		if (ref == null) {
			return null;
		}
		return ref.constructor;
	}

	// read values from url
	function read(args) {
		let values = window.location.href;
		let pos = values.indexOf(paramSeparator);
		if (pos >= 0) {
			values = values.substr(pos + 1).split(valueSeparator);
		}
		else {
			values = [];
		}
		for (let i = 0; i < values.length; i += 1) {
			const pos = values[i].indexOf('=');
			if (pos > 0) {
				let key = values[i].substr(0, pos);
				let value = values[i].substr(pos + 1);
				args[decodeURIComponent(key)] = decodeURIComponent(value);
			}
		}
		return args;
	}

	// write values to url
	function save(args) {
		let values = '';
		for (const key of Object.keys(args)) {
			const value = args[key];
			if (value === null || value === undefined) {
				// exclude undefined and null values
				delete args[key];
				continue;
			}
			if (value.constructor !== String && value.constructor !== Number && value.constructor !== Boolean) {
				console.log('removing invalid parameter value', value);
				// exclude non string, number and boolean values.
				delete args[key];
				continue;
			}

			if (values !== '') {
				values += valueSeparator;
			}
			values += encodeURIComponent(key) + '=' + encodeURIComponent(value);
		}

		window.location.href = paramSeparator + values;
		return args;
	}

	let result = Object.create({
		update: function() {
			let values = arguments[0];
			if (values === false && arguments.length === 1) {
				// update(false) -> reload params
				return read(this);
			}
			if (values === true && arguments.length === 1) {
				// update(true) -> reload page
				window.location.reload();
				return this;
			}

			let writeParams = undefined;
			let changes = Object.create(null);

			if (typeOf(values) === Object && arguments.length === 1) {
				if (Object.keys(values).length === 0) {
					writeParams = true;
				}
			}
			else {
				writeParams = false;

				for (let key of arguments) {
					// properties to be notified
					changes[key] = null;
				}

				values = read({});
				for (let key of Object.keys(this)) {
					if (Object.getOwnPropertyDescriptor(values, key)) {
						// do not delete properties that changed
						continue;
					}
					// property to be deleted
					values[key] = null;
				}
			}

			// update existing values.
			for (let key of Object.keys(values)) {
				let oldValue = this[key];
				let newValue = values[key];

				// do not update properties with same values
				if (oldValue == newValue) {
					// only if the notification was requested
					if (changes[key] === undefined) {
						continue;
					}
				}

				if (newValue != null && oldValue != null) {
					// update
					changes[key] = oldValue;
					this[key] = newValue;
				}
				else if (newValue != null) {
					// insert
					changes[key] = newValue;
					this[key] = newValue;
				}
				else if (oldValue != null) {
					// remove
					changes[key] = oldValue;
					delete this[key];
				}
			}

			let hasChanges = Object.keys(changes).length > 0;
			if (writeParams === undefined) {
				writeParams = hasChanges;
			}
			if (writeParams) {
				save(this);
			}

			if (hasChanges && onHashChange !== undefined) {
				if (onHashChange(this, changes) === true) {
					save(this);
				}
			}
			return this;
		}
	});

	read(result);
	if (onHashChange !== undefined) {
		window.onhashchange = function() {
			if (oldHashChange != null) {
				oldHashChange();
			}
			result.update();
		};
		onHashChange(result);
	}

	return result;
}
