var Module = {
	files: null,
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
		if (Module.files == null || Module.initialized) {
			return;
		}

		Module.wgetFiles(Module.files);
		postMessage({files: Module.listFiles()});
		Module.initialized = true;
	}
};

importScripts("module.js");
importScripts("cmpl.js");

onmessage = function(event) {
	let data = event.data;
	//console.log(data);
	if (data.files !== undefined) {
		if (data.files === true || data.files === false) {
			let files = [];
			if (data.files === false) {
				files.push(...Module.listFiles());
			} else {
				files.push(...Module.listFiles(Module.workspace));
				files.push(...Module.listFiles('/lib'));
			}
			postMessage({files});
		} else {
			Module.files = data.files;
		}
		Module.onRuntimeInitialized();
	}
	if (data.file !== undefined) {
		let path = data.file;
		if (!path.startsWith('/')) {
			path = Module.workspace + '/' + path;
		}
		try {
			if (data.content !== undefined) {
				let exists = FS.analyzePath(path).exists;
				FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
				FS.writeFile(path, data.content, {encoding: 'utf8'});
				if (!exists) {
					postMessage({
						files: Module.listFiles(),
						//file: data.file,
						//line: data.line
					});
				}
			} else {
				postMessage({
					content: FS.readFile(path, {encoding: 'utf8'}),
					file: data.file,
					line: data.line
				});
			}
		} catch (err) {
			postMessage({error: 'file operation failed: `' + data.file + '`: ' + err});
		}
	}
	if (data.exec !== undefined) {
		Module['callMain'](data.exec);
		postMessage({ exec: 'success' });
	}
};
