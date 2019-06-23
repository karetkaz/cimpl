var Module = {
	initialized: false,
	dynamicLibraries: [
		'libFile.wasm'
	],
	print: function(text) {
		postMessage({ print: text });
	},
	onRuntimeInitialized: function () {
		ENV.CMPL_HOME = '/';
		FS.mkdirTree(Module.workspace);
		FS.chdir(Module.workspace);
		postMessage({initialized: true});
		Module.initialized = true;
	}
};

importScripts("module.js");
importScripts("cmpl.js");

onmessage = function(event) {
	let data = event.data;
	console.log(data);
	let result = {};
	try {
		if (!Module.initialized) {
			throw "Module not initialized";
		}

		let list = null;
		// list files from a directory
		if (data.list != null) {
			if (data.list.constructor === Array) {
				Module.wgetFiles(data.list);
				list = [Module.workspace];
			} else {
				list = data.list;
			}
		}

		// open, save, download file
		if (data.file !== undefined) {
			let operation = 'operation';
			try {
				result.file = data.file;
				result.line = data.line;

				let path = data.file;
				if (!path.startsWith('/')) {
					path = Module.workspace + '/' + path;
				}
				if (data.content != null) {
					if (!FS.analyzePath(path).exists) {
						list = [Module.workspace];
					}
					// save file content
					operation = 'save';
					FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
					FS.writeFile(path, data.content, {encoding: 'utf8'});
				}
				else if (data.url != null) {
					if (!FS.analyzePath(path).exists) {
						list = [Module.workspace];
					}
					operation = 'download';
					Module.wgetFiles([data]);
				}
				else {
					// read file content
					operation = 'read';
					result.content = FS.readFile(path, {encoding: 'utf8'});
				}
			} catch (err) {
				result.error = 'File ' + operation + ' failed[' + data.file + ']: ' + err;
				console.trace(err);
			}
		}

		// execute
		if (data.execute !== undefined) {
			try {
				Module['callMain'](data.execute);
				result.exitCode = 0;
			} catch (error) {
				result.exitCode = -1;
			}
		}

		if (list!= null) {
			result.list = Module.listFiles(list);
		}
	} catch (err) {
		result.error = 'operation failed: ' + err;
		console.trace(err);
	}
	postMessage(result);
};
