Module.workspace = '/workspace';
Module.listFiles = function(folders, recursive) {
	let result = [];

	function listRecursive(path) {
		let dir = FS.analyzePath(path);
		if (dir && dir.exists && dir.object) {
			if (!path.endsWith('/')) {
				path = path + '/';
			}
			for (let file in dir.object.contents) {
				let filepath = path + file;
				if (dir.object.contents[file].isFolder) {
					filepath += '/';
					if (recursive) {
						listRecursive(filepath);
						filepath = null;
					}
				}
				if (filepath) {
					result.push(filepath);
				}
			}
		}
	}

	if (folders != null && folders.constructor === String) {
		if (folders.endsWith('/*')) {
			folders = folders.substr(0, folders.length - 1);
			recursive = true;
		}
		if (folders.endsWith('/') && folders !== '/') {
			folders = folders.substr(0, folders.length - 1);
		}

		if (folders === '.') {
			// list all files from workspace directory
			folders = [Module.workspace];
		}
		else if (folders === '*') {
			// list all files from workspace and lib directory
			folders = [Module.workspace, '/lib'];
		}
		else if (folders === '**') {
			// list all files from workspace and lib directory
			folders = [Module.workspace, '/lib'];
			recursive = true;
		}
		else {
			folders = [folders];
		}
	}

	for (let i = 0; i < folders.length; ++i) {
		let folder = folders[i];
		let tail = result.length;
		listRecursive(folder);
		let sorted = result.slice(tail).sort(function(lhs, rhs) {
			let lhsDir = lhs.endsWith("/");
			let rhsDir = rhs.endsWith("/");
			if (lhsDir !== rhsDir) {
				return lhsDir ? -1 : 1;
			}
			return lhs.localeCompare(rhs);
		});
		result = result.slice(0, tail).concat(sorted);

		if (folders.length === 1 && folders[0] === Module.workspace) {
			for (let i = 0; i < result. length; ++i) {
				result[i] = result[i].substr(Module.workspace.length + 1);
			}
		}
	}
	return result;
};

Module.pathExists = function(path) {
	return FS.analyzePath(path).exists;
}

Module.absolutePath = function(path) {
	if (path != null && path[0] != "/") {
		return Module.workspace + '/' + path;
	}
	return path;
}

Module.saveFile = function(path, content) {
	// persist the content of the file
	path = Module.absolutePath(path);
	FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
	FS.writeFile(path, content, {encoding: 'binary'});
}

Module.readFile = function(path) {
	// persist the content of the file
	return FS.readFile(path, {encoding: 'utf8'});
}

Module.initWorkspace = function(name, callback) {
	if (name == null || name === "") {
		return false;
	}
	if (!name.startsWith('/')) {
		name = '/' + name;
	}
	if (Module.workspace === name) {
		return false;
	}
	Module.workspace = name;
	FS.mkdirTree(Module.workspace);
	FS.chdir(Module.workspace);
	FS.mount(IDBFS, {}, Module.workspace);
	FS.syncfs(true, callback || function() {
		console.log("Workspace initialized");
	});
	return true;
}

Module.openProjectFile = function(data, callBack) {
	if (!Module.initialized) {
		throw "Module not initialized";
	}

	function onInitWorkSpace(error) {
		if (error != null) {
			Module.printErr('error: ' + error);
			return;
		}
		if (data.folder == null) {
			data.folder = Module.workspace;
		}
		try {
			// TODO: remove recursive calls
			Module.openProjectFile(data, callBack);
		} catch (err) {
			Module.printErr('operation failed: ' + err);
		}
	}

	// setup custom workspace directory
	if (Module.initWorkspace(data.workspace, onInitWorkSpace)) {
		return;
	}

	let list = null;
	let sync = false;

	let result = {};
	// download and store project files
	if (data.project != null) {
		sync = !Module.wgetFiles(data.project);
		list = [Module.workspace];
	}

	// open, save, download file
	if (data.file !== undefined) {
		let operation = 'operation';
		try {
			let path = data.file;
			if (data.content != null) {
				if (!Module.pathExists(path)) {
					list = [Module.workspace];
				}
				// save file content
				operation = 'save';
				Module.saveFile(path, data.content);
				sync = true;
			}
			else if (data.url != null) {
				if (!Module.pathExists(path)) {
					list = [Module.workspace];
				}
				operation = 'download';
				sync = Module.wgetFiles([data]);
			}
			else {
				// read file content
				operation = 'read';
				result.content = Module.readFile(path);
				result.file = data.file;
				result.line = data.line;
			}
		} catch (err) {
			result.error = 'File ' + operation + ' failed[' + data.file + ']: ' + err;
			console.trace(err);
		}
	}

	// list files from custom directory
	if (data.folder != null) {
		list = data.folder;
	}

	if (list != null) {
		result.list = Module.listFiles(list);
	}
	if (sync) {
		FS.syncfs(function(error) {
			if (error == null) {
				return;
			}
			Module.printErr('error: ' + error);
		});
	}
	return callBack(result);
}

Module.wgetFiles = function(files) {
	let isAsync = Module.onFileDownloaded != null;
	let inProgress = files.length;
	function saveFile(path, content) {
		Module.saveFile(path, content);
		FS.syncfs(function(error) {
			if (error == null) {
				return;
			}
			Module.printErr('error: ' + error);
		});

		inProgress -= 1;
		if (Module.onFileDownloaded != null) {
			Module.onFileDownloaded(inProgress);
		}
	}

	for (let file of files) {
		let path = file.file || file.path;
		try {
			if (file.content != null) {
				saveFile(path, file.content);
				Module.print('Created file: ' + path);
				continue;
			}

			// download the file
			if (path == null) {
				path = file.url.replace(/^(.*[/])?(.*)(\..*)$/, "$2$3");
			}
			let xhr = new XMLHttpRequest();
			let url = file.url;
			if (Module.relativeUrl != null) {
				url = Module.relativeUrl(url);
			}
			xhr.open('GET', url, isAsync);
			xhr.responseType = "arraybuffer";
			xhr.overrideMimeType("application/octet-stream");
			xhr.onreadystatechange = function () {
				if (xhr.readyState !== 4) {
					return;
				}
				if (xhr.status < 200 || xhr.status >= 300) {
					let err = new Error(xhr.status + ' (' + xhr.statusText + ')')
					Module.print('Download failed: `' + xhr.responseURL + '`: ' + err);
					return err;
				}
				saveFile(path, new Uint8Array(xhr.response));
				Module.print('Downloaded file: ' + path + ': ' + xhr.responseURL);
				if (inProgress == 0) {
					Module.print("Project file(s) download complete.");
				}
			}
			xhr.send(null);
		}
		catch (err) {
			Module.printErr('Download failed: `' + path + '`: ' + err);
			inProgress -= 1;
			if (Module.onFileDownloaded != null) {
				Module.onFileDownloaded(inProgress);
			}
		}
	}
	return isAsync && files.length > 0;
};
