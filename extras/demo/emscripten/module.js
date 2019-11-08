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

Module.wgetFiles = function(files, onComplete) {
	let inProgress = files.length;
	function saveFile(path, content) {
		Module.saveFile(path, content);
		inProgress -= 1;
		if (onComplete != null) {
			onComplete(inProgress);
		}
	}

	for (let file of files) {
		let path = file.file || file.path;
		try {
			if (file.content != null) {
				saveFile(path, file.content);
				Module.print('Created file: ' + path);
			}
			else if (file.url != null) {
				if (path == null) {
					path = file.url.replace(/^(.*[/])?(.*)(\..*)$/, "$2$3");
				}
				let xhr = new XMLHttpRequest();
				xhr.open('GET', file.url, onComplete != null);
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
			else {
				saveFile(path, '');
				Module.print('Created empty file: ' + path);
			}
		}
		catch (err) {
			Module.print('Download failed: `' + path + '`: ' + err);
			console.error(err);
			inProgress -= 1;
		}
	}
};