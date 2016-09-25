function Instrument() {

	var data = null;

	var samples = null;

	function enter(calls, functions, id, tick) {
		var call = {
			hits: 1,
			self: 0,
			total: null,
			enter: tick,
			leave: null,
			memory: null,
			caller: null,
			calltree: null,
			//offs: id,
			func: functions[id] || {}
		};

		var samples = calls[0];

		// update global execution time
		if (samples.enter != null && samples.enter > tick) {
			samples.enter = tick;
		}

		if (samples.samples != null) {
			if (functions[id] == null) {
				console.log('invalid offset: ' + id);
			}
			var sample = samples.samples[id] || (samples.samples[id] = {
					func: functions[id] || {},
					hits: 0,
					self: 0,
					total: 0,
					get others() {
						return this.total - this.self;
					},
					callers: {},
					callees: {}
				});
			if (sample.hits != null) {
				sample.hits += 1;
			}
		}
		return call;
	}

	function leave(calls, caller, callee, tick, memory) {
		var j, recursive = false;
		var time = tick - callee.enter;
		var calleeId = callee.func.offs;
		var callerId = caller.func && caller.func.offs;

		for (j = 1; j < calls.length; ++j) {
			if (calleeId === calls[j].func.offs) {
				if (calls[j].hits != null) {
					calls[j].hits += 1;
				}
				recursive = true;
			}
		}

		if (callee.caller !== null ||
			callee.memory !== null ||
			callee.leave !== null ||
			callee.total !== null
		) {
			throw "invalid state"
		}
		callee.leave = tick;
		callee.caller = caller;
		callee.memory = memory;
		callee.total = time;

		//*
		if (callee.self != null) {
			callee.self = callee.total;
			if (callee.calltree != null) {
				for (j = 0; j < callee.calltree.length; ++j) {
					callee.self -= callee.calltree[j].total;
				}
			}
		}// */
		var samples = calls[0];
		if (samples.excluded == null || samples.excluded.indexOf(callee.name) < 0) {
			caller.calltree = (caller.calltree || []);
			caller.calltree.push(callee);
		}

		// update global execution time
		if (samples.leave != null && samples.leave < tick) {
			samples.leave = tick;
		}

		// compute global information
		if (samples.samples != null) {
			var sample = samples.samples[calleeId] || {};
			// update time spent in function
			if (sample.total != null && !recursive) {
				sample.total += time;
			}
			if (sample.self != null) {
				sample.self += time;
			}

			// update list of functions which have calls to this function
			if (callerId != null) {
				if (sample.callers != null) {
					sample.callers[callerId] = (sample.callers[callerId] || 0) + 1;
				}

				sample = samples.samples[callerId];
				// update time spent in function
				if (sample.self != null) {
					sample.self -= time;
				}

				// update list of functions invoked from this function
				if (sample.callees != null) {
					sample.callees[calleeId] = (sample.callees[calleeId] || 0) + 1;
				}
			}
		}
	}

	function getSamples() {
		return samples;
	}

	function getFiltered(checkSymbol) {
		var result = '';
		if (data == null) {
			return 'No data available!';
		}

		function symToString(sym, qual, type) {
			var result = '';
			if (sym.declaredIn && qual === true) {
				result += sym.declaredIn + '.';
			}
			result += sym.name;
			if (sym.args !== undefined) {
				result += '(';
				if (!(sym.args.length === 1 && sym.args[0].size == 0)) {
					for (var i = 0; i < sym.args.length; i += 1) {
						if (i > 0) {
							result += ', ';
						}
						result += symToString(sym.args[i], false, true);
					}
				}
				result += ')';
			}
			if (type === true) {
				result += ": " + sym.type;
			}
			return result;
		}

		var api = data.symbols || [];
		for (var i = 0; i < api.length; i += 1) {
			var sym = api[i];
			if (!checkSymbol(sym)) {
				continue;
			}
			result += symToString(sym, true, true);
			result += '\n';
		}
		return result;
	}

	function loadData(dataIn) {
		data = dataIn;

		var ctRecSize = 2;
		var ctFunIndex = 1;
		var ctTickIndex = 0;
		var ctHeapIndex = null;

		if (data.profile == null) {
			data.profile = {
				callTree: [],
				ticksPerSec: -1,
				functions: [],
				functionCount: 0,
				statements: [],
				statementCount: 0
			};
		}
		if (data.profile.ctFunIndex != null) {
			ctFunIndex = data.profile.ctFunIndex;
			if (ctRecSize < ctFunIndex + 1) {
				ctRecSize = ctFunIndex + 1;
			}
		}
		if (data.profile.ctTickIndex != null) {
			ctTickIndex = data.profile.ctTickIndex;
			if (ctRecSize < ctTickIndex + 1) {
				ctRecSize = ctTickIndex + 1;
			}
		}
		if (data.profile.ctHeapIndex != null) {
			ctHeapIndex = data.profile.ctHeapIndex;
			if (ctRecSize < ctHeapIndex + 1) {
				ctRecSize = ctHeapIndex + 1;
			}
		}

		var i, calls = [{
			enter: +Infinity,
			leave: -Infinity,
			samples: {},
			calltree: null,
			//excluded: ['Halt', 'ToDays'],
			ticksPerSec: data.profile.ticksPerSec
		}];

		var caller, callee, functions = Object.create(null);	// empty object without prototype.

		for (i = 0; i < data.profile.functions.length; ++i) {
			var symbol = data.profile.functions[i];
			functions[symbol.offs] = symbol;
		}

		samples = null;
		for (i = 0; i < data.profile.callTree.length; i += ctRecSize) {
			var offs = data.profile.callTree[i + ctFunIndex];
			var time = ctTickIndex == null ? undefined : data.profile.callTree[i + ctTickIndex];
			var heap = ctHeapIndex == null ? undefined : data.profile.callTree[i + ctHeapIndex];

			if (offs !== -1) {
				callee = enter(calls, functions, offs, time);
				calls.push(callee);
			}
			else {
				callee = calls.pop();
				caller = calls[calls.length - 1];
				leave(calls, caller, callee, time, heap);
				Object.freeze(callee);
			}
		}

		while (calls.length > 0) {
			samples = calls.pop();
			if (calls.length > 1) {
				caller = calls[calls.length - 1];
				leave(calls, caller, samples, 0);
			}
			Object.freeze(samples);
		}
		if (calls.length === 1) {
			samples = calls.pop();
			Object.freeze(samples);
		}
	}

	return {
		api: {
			getAll: function() {
				return getFiltered(function() {
					return true;
				});
			},
			getFiltered: function(types, funcs, vars, defs, opcodes) {
				return getFiltered(function (sym) {
					if (types && sym.kind === '.rec') {
						return true;
					}
					if (funcs && sym.kind === '.ref' && sym.args != null) {
						return true;
					}
					if (vars && sym.kind === '.ref' && sym.args == null) {
						return true;
					}
					if (defs && sym.kind === '.def') {
						return true;
					}
					if (opcodes && sym.kind === 'emit') {
						return true;
					}
					console.log("symbol was filtered: " + sym.proto);
					return false;
				});
			}
		},
		loadFile: function(file, action) {
			var reader = new FileReader();
			reader.onload = function() {
				try {
					loadData(JSON.parse(reader.result));
					if (action != null) {
						action(null);
					}
				}
				catch (error) {
					console.log(error);
					if (action != null) {
						action(error);
					}
				}
			};
			reader.readAsText(file);
		},
		getSamples: getSamples,

		// to be removed
		loadData: loadData
	}
}
