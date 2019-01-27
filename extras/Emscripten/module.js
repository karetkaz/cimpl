Module.workspace = '/workspace';
Module.listFiles = function(workspace) {
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
};

Module.wgetFiles = function(files, onComplete) {
	Module.files = files;
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
		let path = file.path;
		if (path[0] != "/") {
			path = Module.workspace + '/' + path;
		}
		try {
			if (file.content != null) {
				saveFile(path, file.content);
				Module.print('file[' + path + '] created with content');
			}
			else if (file.url != null) {
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
				}
				xhr.send(null);
			}
			else {
				saveFile(path, '');
				Module.print('file[' + path + '] created without content');
			}
		}
		catch (err) {
			Module.print('failed to download: `' + file.path + '`: ' + err);
			console.error(err);
			inProgress -= 1;
		}
	}
};
