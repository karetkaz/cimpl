var Module = {
	initialized: false,
	relativeUrl: function(path) {
		let worker = '/extras/demo/emscripten/worker.js';
		if (location.href.endsWith(worker) && !path.startsWith('/')) {
			return location.href.substr(0, location.href.length - worker.length + 1) + path;
		}
		return path;
	},
	locateFile: function(path) {
		return './' + path;
	},
	dynamicLibraries: [
		'libFile.wasm'
	],
	print: function(message) {
		console.log(message);
		postMessage({ print: message });
	},
	printErr: function(message) {
		console.error(message);
		postMessage({ error: message });
	},
	onRuntimeInitialized: function () {
		ENV.CMPL_HOME = '/';
		FS.mkdirTree(Module.workspace);
		FS.chdir(Module.workspace);
		Module.initialized = true;
		postMessage({initialized: true});
	}
};

importScripts("module.js");
importScripts("cmpl.js");

onmessage = function(event) {
	let data = event.data;
	//console.log(data);
	try {
		Module.openProjectFile(data, function(result) {
			// execute
			if (data.execute !== undefined) {
				try {
					callMain(data.execute);
					result.exitCode = 0;
					sync = true;
				} catch (error) {
					result.exitCode = -1;
				}
			}
			postMessage(result);
		});
	} catch (err) {
		Module.printErr('operation failed: ' + err);
	}
};
