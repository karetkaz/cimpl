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
						//filepath = null;
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

Module.wgetFiles = function(files, onComplete) {
	let inProgress = files.length;
	function saveFile(path, content) {
		// persist the content of the file
		FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
		FS.writeFile(path, content, {encoding: 'binary'});
		inProgress -= 1;
		if (onComplete != null) {
			onComplete(inProgress);
		}
	}

	for (let file of files) {
		let path = file.file || file.path;
		try {
			if (path != null && path[0] != "/") {
				path = Module.workspace + '/' + path;
			}
			if (file.content != null) {
				saveFile(path, file.content);
				Module.print('file[' + path + '] created with content');
			}
			else if (file.url != null) {
				if (path == null) {
					path = file.url.replace(/^(.*[/])?(.*)(\..*)$/, "$2$3");
					path = Module.workspace + '/' + path;
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
						Module.print('failed to download: `' + xhr.responseURL + '`: ' + err);
						return err;
					}
					saveFile(path, new Uint8Array(xhr.response));
					Module.print('file[' + path + '] downloaded: ' + xhr.responseURL);
					if (inProgress == 0) {
						Module.print("Project file(s) download complete.");
					}
				}
				xhr.send(null);
			}
			else {
				saveFile(path, '');
				Module.print('file[' + path + '] created without content');
			}
		}
		catch (err) {
			Module.print('failed to download: `' + path + '`: ' + err);
			console.error(err);
			inProgress -= 1;
		}
	}
};
