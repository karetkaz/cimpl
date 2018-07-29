var Module = {
	workspace: '/workspace',
	files: [],
	print: function(text) {
		postMessage({ print: text });
	},
	onRuntimeInitialized: function () {
		FS.mkdirTree(Module.workspace);
		FS.chdir(Module.workspace);
		let files = [];
		for (let file of Module.files) {
			let path = Module.workspace + '/' + file.path;
			try {
				if (file.url === undefined) {
					FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
					FS.writeFile(path, file.content, {encoding: 'utf8'});
					files.push(file.path);
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
				files.push(file.path);
				Module.print('file downloaded: ' + xhr.responseURL);
			}
			catch (err) {
				Module.print('failed to download: `' + file.path + '`: ' + err);
				console.error(err);
			}
		}
		postMessage({files: files});
	}
};

importScripts("cmpl.js");

onmessage = function(event) {
	let data = event.data;
	//console.log(data);
	if (data.files !== undefined) {
		Module.files = data.files;
	}
	if (data.file !== undefined) {
		let path = data.file;
		if (!path.startsWith('/')) {
			path = Module.workspace + '/' + path;
		}
		try {
			if (data.content !== undefined) {
				FS.mkdirTree(path.replace(/^(.*[/])?(.*)(\..*)$/, "$1"));
				FS.writeFile(path, data.content, {encoding: 'utf8'});
				postMessage({
					file: data.file,
					line: data.line
				});
			} else {
				postMessage({
					file: data.file,
					line: data.line,
					content: FS.readFile(path, {encoding: 'utf8'})
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