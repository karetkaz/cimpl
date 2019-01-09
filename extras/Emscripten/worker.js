var Module = {
	workspace: '/workspace',
	files: null,
	initialized: false,
	dynamicLibraries: [
		'libFile.wasm'
	],
	print: function(text) {
		postMessage({ print: text });
	},
	listFiles: function(workspace) {
		let result = [];
		(function lsr(path) {
			let dir = FS.analyzePath(path);
			if (dir && dir.exists && dir.object) {
				for (let file in dir.object.contents) {
					let filepath = path + '/' + file;
					if (dir.object.contents[file].isFolder) {
						lsr(filepath);
					} else {
						if (workspace === undefined && path.startsWith(Module.workspace)) {
							result.push(filepath.substr(Module.workspace.length + 1));
						} else {
							result.push(filepath);
						}
					}
				}
			}
		})(workspace || Module.workspace);
		return result;
	},
	onRuntimeInitialized: function () {
		FS.mkdirTree(Module.workspace);
		FS.chdir(Module.workspace);
		if (Module.files === null || Module.initialized) {
			return;
		}

		for (let file of Module.files) {
			let path = Module.workspace + '/' + file.path;
			try {
				if (file.url === undefined) {
					FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
					FS.writeFile(path, file.content, {encoding: 'utf8'});
					Module.print('file created: ' + file.path);
					continue;
				}

				let xhr = new XMLHttpRequest();
				xhr.open('GET', file.url, false);
				xhr.send(null);
				if (!xhr.response || xhr.status < 200 || xhr.status >= 300) {
					throw new Error(xhr.status + ' (' + xhr.statusText + ')');
				}
				FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
				FS.writeFile(path, xhr.responseText, {encoding: 'utf8'});
				Module.print('file downloaded: ' + xhr.responseURL);
			}
			catch (err) {
				Module.print('failed to download: `' + file.path + '`: ' + err);
				console.error(err);
			}
		}
		postMessage({files: Module.listFiles()});
		Module.initialized = true;
	}
};

importScripts("cmpl.js");

onmessage = function(event) {
	let data = event.data;
	console.log(data);
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
					postMessage({ files: Module.listFiles() });
				}
			} else {
				postMessage({
					content: FS.readFile(path, {encoding: 'utf8'}),
					file: data.file,
					line: data.line
				});
			}
		} catch (err) {
			Module.print('file operation failed: `' + data.file + '`: ' + err)
		}
	}
	if (data.exec !== undefined) {
		Module['callMain'](data.exec);
		postMessage({ exec: 'success' });
	}
};